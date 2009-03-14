#include "cloudy/core.h"
#include "cloudy/wbuffer.h"
#include "cloudy/stream.h"
#include "cloudy/cbtable.h"

#ifndef CLOUDY_RECV_INIT_SIZE
#define CLOUDY_RECV_INIT_SIZE 1024*8
#endif

#ifndef CLOUDY_RECV_RESERVE_SIZE
#define CLOUDY_RECV_RESERVE_SIZE 512
#endif

struct cloudy {
	cloudy_header* hparsed;
	cloudy_stream stream;
	size_t received;

	cloudy_wbuffer wbuf;

	cloudy_cbtable cbtable;

	int fd;
	struct timeval timeout;

	struct sockaddr* addr;
	socklen_t addrlen;
};



static inline void error_close(cloudy* ctx, cloudy_return err)
{
	cloudy_cbtable_callback_all(&ctx->cbtable, err);
	close(ctx->fd);
	ctx->fd = -1;
}

static bool connect_server(cloudy* ctx)
{
	// FIXME timeout
	int fd = socket(ctx->addr->sa_family, SOCK_STREAM, 0);
	if(fd < 0) {
		return false;
	}

	if(connect(fd, ctx->addr, ctx->addrlen) < 0) {
		close(fd);
		return false;
	}

	ctx->fd = fd;
	return true;
}

cloudy_data* cloudy_send_request_async(cloudy* ctx, cloudy_header* header,
		const char* key, const char* val, const char* extra)
{
	cloudy_entry* e = cloudy_cbtable_add(&ctx->cbtable, header);
	if(!e) {
		return NULL;
	}

	if(ctx->fd < 0) {
		if(!connect_server(ctx)) {
			cloudy_cbtable_callback_all(&ctx->cbtable, CLOUDY_RES_CLOSED);
			return (cloudy_data*)e;
		}
	}

	cloudy_wbuffer* const wbuf = &ctx->wbuf;

	char packed[sizeof(cloudy_header)];
	cloudy_header_pack(packed, header);

	if(cloudy_wbuffer_empty(wbuf)) {
		struct iovec iov[4];

		iov[0].iov_base = packed;
		iov[0].iov_len  = sizeof(cloudy_header);
		unsigned int cnt = 1;

		uint8_t extralen = header->extralen;
		if(extralen > 0) {
			iov[cnt].iov_base = (void*)extra;
			iov[cnt].iov_len  = extralen;
			++cnt;
		}

		uint16_t keylen = header->keylen;
		if(keylen > 0) {
			iov[cnt].iov_base = (void*)key;
			iov[cnt].iov_len  = keylen;
			++cnt;
		}

		uint32_t vallen = header->bodylen - keylen - extralen;
		if(vallen > 0) {
			iov[cnt].iov_base = (void*)val;
			iov[cnt].iov_len  = vallen;
			++cnt;
		}

		ssize_t rl = writev(ctx->fd, iov, cnt);
		if(rl <= 0) {
			if(errno == EAGAIN || errno == EINTR) {
				goto append_all;
			}
			goto socket_error;
		}

		size_t i;
		for(i = 0; i < cnt; ++i) {
			if((size_t)rl >= iov[i].iov_len) {
				rl -= iov[i].iov_len;
			} else {
				if(!cloudy_wbuffer_append(wbuf,
							(const char*)iov[i].iov_base + rl,
							iov[i].iov_len - rl)) {
					goto append_wbuf_error;
				}
				break;
			}
		}
		for(; i < cnt; ++i) {
			if(!cloudy_wbuffer_append(wbuf,
						(const char*)iov[i].iov_base, iov[i].iov_len)) {
				goto append_wbuf_error;
			}
		}

	} else {
append_all:
		if(!cloudy_wbuffer_append(wbuf, packed, sizeof(cloudy_header))) {
			goto append_wbuf_error;
		}

		uint8_t extralen = header->extralen;
		if(extralen > 0 && cloudy_wbuffer_append(wbuf, extra, extralen) < 0 ) {
			goto append_wbuf_error;
		}

		uint16_t keylen = header->keylen;
		if(keylen > 0 && cloudy_wbuffer_append(wbuf, key, keylen) < 0 ) {
			goto append_wbuf_error;
		}

		uint32_t vallen = header->bodylen - keylen - extralen;
		if(vallen > 0 && cloudy_wbuffer_append(wbuf, val, vallen) < 0 ) {
			goto append_wbuf_error;
		}
	}

	return (cloudy_data*)e;

socket_error:
	error_close(ctx, CLOUDY_RES_CLOSED);
	return (cloudy_data*)e;

append_wbuf_error:
	error_close(ctx, CLOUDY_RES_OUT_OF_MEMORY);
	return (cloudy_data*)e;
}

bool cloudy_send_request_flush(cloudy* ctx)
{
	cloudy_wbuffer* const wbuf = &ctx->wbuf;

	if(ctx->wbuf.size == 0) {
		return true;
	}

	if(ctx->fd < 0) {
		if(!connect_server(ctx)) {
			cloudy_cbtable_callback_all(&ctx->cbtable, CLOUDY_RES_CLOSED);
			return false;
		}
	}

	char* p = wbuf->data;
	char* const pend = p + wbuf->size;

	do {
		ssize_t rl = write(ctx->fd, p, pend - p);
		if(rl <= 0) {
			if(errno == EAGAIN || errno == EINTR) {
				continue;
			}
			error_close(ctx, CLOUDY_RES_CLOSED);
			return false;
		}

		p += rl;
	} while(p < pend);

	wbuf->size = 0;

	return true;
}


static inline bool cloudy_receive_message(cloudy* ctx)
{
	cloudy_stream* const stream = &ctx->stream;
	cloudy_cbtable* const cbtable = &ctx->cbtable;
	const int fd = ctx->fd;

	if(fd < 0) {
		cloudy_cbtable_callback_all(cbtable, CLOUDY_RES_CLOSED);
		return false;
	}

	ssize_t rl;
	if(!cloudy_stream_reserve_buffer(stream,
				CLOUDY_RECV_RESERVE_SIZE, CLOUDY_RECV_INIT_SIZE)) {
		return false;
	}
	goto skip_wait;

retry_wait:
	if(!cloudy_stream_reserve_buffer(stream,
				CLOUDY_RECV_RESERVE_SIZE, CLOUDY_RECV_INIT_SIZE)) {
		return false;
	}

	{
		struct timeval timeout = ctx->timeout;
		fd_set fdset;

		FD_ZERO(&fdset);
		FD_SET(fd, &fdset);

		int ret = select(fd+1, &fdset, NULL, NULL, &timeout);
		if(ret < 0) {
			error_close(ctx, CLOUDY_RES_IO_ERROR);
			return false;

		} else if(ret == 0) {
			cloudy_cbtable_callback_all(cbtable, CLOUDY_RES_TIMEOUT);
			return false;
		}
	}

skip_wait:
	if(!cloudy_stream_reserve_buffer(stream,
				CLOUDY_RECV_RESERVE_SIZE+ctx->received, CLOUDY_RECV_INIT_SIZE)) {
		return false;
	}

	rl = read(fd,
			cloudy_stream_buffer(stream) + ctx->received,
			cloudy_stream_buffer_capacity(stream) - ctx->received);
	if(rl <= 0) {
		if(errno == EAGAIN || errno == EINTR) {
			goto retry_wait;
		}
		error_close(ctx, CLOUDY_RES_CLOSED);
		return false;
	}

	ctx->received += rl;

	cloudy_header* header;

	if(ctx->hparsed) {
		if(ctx->received - sizeof(cloudy_header) < ctx->hparsed->bodylen) {
			goto retry_wait;
		} else {
			goto skip_header_parse;
		}
	} else if(ctx->received < sizeof(cloudy_header)) {
		goto retry_wait;
	}

/*header_parse:*/
	while(1) {
		header = (cloudy_header*)cloudy_stream_buffer(stream);
		cloudy_header_unpack((char*)header, header);
		if(header->magic != CLOUDY_RESPONSE || header->seqid == 0) {
			ctx->hparsed = NULL;
			error_close(ctx, CLOUDY_RES_SERVER_ERROR);
			return false;
		}

		if(ctx->received - sizeof(cloudy_header) < header->bodylen) {
			if(ctx->hparsed) {
				return true;
			} else {
				ctx->hparsed = header;
				goto retry_wait;
			}
		}
	
		ctx->hparsed = header;

	skip_header_parse:
		{
			size_t parsed_size = sizeof(cloudy_header) + ctx->hparsed->bodylen;

			ctx->received -= parsed_size;

			cloudy_stream_reference* reference = cloudy_stream_allocate(
					stream, parsed_size);
			cloudy_cbtable_callback(cbtable, ctx->hparsed, reference);
	
			if(ctx->received < sizeof(cloudy_header)) {
				ctx->hparsed = NULL;
				return true;
			}
		}

	}
}


bool cloudy_sync_response(cloudy* ctx, cloudy_data* data)
{
	if(!cloudy_send_request_flush(ctx)) {
		return false;
	}
	cloudy_entry* const e = (cloudy_entry*)data;
	while(e->data.header.magic != CLOUDY_RESPONSE) {
		if(!cloudy_receive_message(ctx)) {
			return false;
		}
	}
	return true;
}


void cloudy_data_free(cloudy_data* data)
{
	if(!data) { return; }

	cloudy_entry* const e = (cloudy_entry*)data;
	if(e->data.reference) {
		cloudy_stream_reference_free(e->data.reference);
	}

	e->seqid = 0;
}



cloudy* cloudy_new(struct sockaddr* addr, socklen_t addrlen, struct timeval timeout)
{
	cloudy* const ctx = (cloudy*)calloc(sizeof(cloudy), 1);
	if(!ctx) {
		return NULL;
	}

	ctx->fd = -1;

	ctx->addr = (struct sockaddr*)malloc(addrlen);
	if(!ctx->addr) {
		goto err_addr;
	}
	memcpy(ctx->addr, addr, addrlen);
	ctx->addrlen = addrlen;

	ctx->timeout = timeout;

	if(!cloudy_stream_init(&ctx->stream, CLOUDY_RECV_INIT_SIZE)) {
		goto err_stream;
	}

	if(!cloudy_cbtable_init(&ctx->cbtable)) {
		goto err_cbtable;
	}

	cloudy_wbuffer_init(&ctx->wbuf);

	return ctx;

err_cbtable:
	cloudy_stream_destroy(&ctx->stream);
err_stream:
	free(ctx->addr);
err_addr:
	free(ctx);

	return NULL;
}

void cloudy_free(cloudy* ctx)
{
	cloudy_wbuffer_destroy(&ctx->wbuf);
	cloudy_cbtable_destroy(&ctx->cbtable);
	cloudy_stream_destroy(&ctx->stream);
	free(ctx->addr);
	free(ctx);
}


