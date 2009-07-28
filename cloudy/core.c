 /*
  * cloudy core
  *
  * Copyright (C) 2008-2009 FURUHASHI Sadayuki
  *
  *    Licensed under the Apache License, Version 2.0 (the "License");
  *    you may not use this file except in compliance with the License.
  *    You may obtain a copy of the License at
  *
  *        http://www.apache.org/licenses/LICENSE-2.0
  *
  *    Unless required by applicable law or agreed to in writing, software
  *    distributed under the License is distributed on an "AS IS" BASIS,
  *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  *    See the License for the specific language governing permissions and
  *    limitations under the License.
  */
#include "cloudy/core.h"
#include "cloudy/wbuffer.h"
#include "cloudy/stream.h"
#include "cloudy/cbtable.h"
#include <fcntl.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#else
#include <ws2tcpip.h>
struct iovec {
  unsigned long iov_len;
  char *iov_base;
};
ssize_t
writev(SOCKET fd, const struct iovec *iov, int iovcnt)
{
  DWORD count = 0;
  int res;
  res = WSASend(fd, (LPWSABUF) iov, iovcnt, &count, 0, NULL, NULL);
  return (res == 0 ? count : -1);
}
#endif

#include <stdio.h>

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

	int flag = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));  // FIXME ignore error?

	struct linger opt = {0, 0};
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *)&opt, sizeof(opt));  // FIXME ignore error?

#ifndef _WIN32
	if(fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		close(fd);
		return false;
	}
#else
	{
		unsigned long flags = 1;
		ioctlsocket(fd, FIONBIO, &flags);
	}
#endif

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
#ifdef _WIN32
		errno = WSAGetLastError();
		if (errno == WSAENOTCONN) errno = EAGAIN;
#endif
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

#ifndef _WIN32
	if(fcntl(ctx->fd, F_SETFL, 0) < 0) {
		error_close(ctx, CLOUDY_RES_IO_ERROR);
		return false;
	}
#else
	{
		unsigned long flags = 0;
		ioctlsocket(ctx->fd, FIONBIO, &flags);
	}
#endif

	char* p = wbuf->data;
	char* const pend = p + wbuf->size;

	do {
#ifndef _WIN32
		ssize_t rl = write(ctx->fd, p, pend - p);
#else
		ssize_t rl = send(ctx->fd, p, pend - p, 0);
		errno = WSAGetLastError();
		if (errno == WSAENOTCONN) errno = EAGAIN;
#endif
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

#ifndef _WIN32
	if(fcntl(ctx->fd, F_SETFL, O_NONBLOCK) < 0) {
		error_close(ctx, CLOUDY_RES_IO_ERROR);
		return false;
	}
#else
	{
		unsigned long flags = 1;
		ioctlsocket(ctx->fd, FIONBIO, &flags);
	}
#endif

	return true;
}

static bool cloudy_send_request_try_flush(cloudy* ctx)
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

#ifndef _WIN32
	ssize_t rl = write(ctx->fd, wbuf->data, wbuf->size);
#else
	ssize_t rl = send(ctx->fd, wbuf->data, wbuf->size, 0);
	errno = WSAGetLastError();
	if (errno == WSAENOTCONN) errno = EAGAIN;
#endif
	if(rl <= 0) {
		if(errno == EAGAIN || errno == EINTR) {
			return true;
		}
		error_close(ctx, CLOUDY_RES_CLOSED);
		return false;
	}

	wbuf->data += rl;
	wbuf->size -= rl;

	return true;
}


static inline bool cloudy_receive_message(cloudy* ctx)
{
	cloudy_stream* const stream = &ctx->stream;
	cloudy_cbtable* const cbtable = &ctx->cbtable;
	const int fd = ctx->fd;

	if(fd < 0) {
		printf("xfd\n");
		cloudy_cbtable_callback_all(cbtable, CLOUDY_RES_CLOSED);
		return false;
	}

	ssize_t rl;
	if(!cloudy_stream_reserve_buffer(stream,
				CLOUDY_RECV_RESERVE_SIZE, CLOUDY_RECV_INIT_SIZE)) {
		printf("xreserve\n");
		return false;
	}
	goto skip_wait;

retry_wait:
	if(!cloudy_stream_reserve_buffer(stream,
				CLOUDY_RECV_RESERVE_SIZE, CLOUDY_RECV_INIT_SIZE)) {
		printf("xrecvinit\n");
		return false;
	}

retry_wait_retry:
	{
		struct timeval timeout = ctx->timeout;
		fd_set rfdset;
		fd_set wfdset;

		FD_ZERO(&rfdset);
		FD_ZERO(&wfdset);
		FD_SET(fd, &rfdset);
//printf("select...\n");
		int ret;
		if(ctx->wbuf.size > 0) {
			FD_SET(fd, &wfdset);
			ret = select(fd+1, &rfdset, &wfdset, NULL, &timeout);
		} else {
			ret = select(fd+1, &rfdset, NULL, NULL, &timeout);
		}
//printf("%d %d %d\n", ret, FD_ISSET(fd, &rfdset), ctx->wbuf.size );

		if(ret < 0) {
			printf("re<0 %d %d\n", errno, EINTR);
			perror("re<0");
			error_close(ctx, CLOUDY_RES_IO_ERROR);
			return false;
		} else if(ret == 0) {
			printf("re==0\n");
			cloudy_cbtable_callback_all(cbtable, CLOUDY_RES_TIMEOUT);
			return false;
		}

		if(FD_ISSET(fd, &wfdset)) {
			if(!cloudy_send_request_try_flush(ctx)) {
				printf("send fail\n");
				return false;
			}
			if(!FD_ISSET(fd, &rfdset)) {
				goto retry_wait_retry;
			}
		}
	}

skip_wait:
	if(!cloudy_stream_reserve_buffer(stream,
				CLOUDY_RECV_RESERVE_SIZE+ctx->received, CLOUDY_RECV_INIT_SIZE)) {
		printf("resv\n");
		return false;
	}

//	printf("before read used=%lu received=%lu\n", stream->used, ctx->received);
#ifndef _WIN32
	rl = read(fd,
			cloudy_stream_buffer(stream) + ctx->received,
			cloudy_stream_buffer_capacity(stream) - ctx->received);
#else
	rl = recv(fd,
			cloudy_stream_buffer(stream) + ctx->received,
			cloudy_stream_buffer_capacity(stream) - ctx->received,
			0);
	errno = WSAGetLastError();
	if (errno == WSAEWOULDBLOCK) errno = EAGAIN;
#endif
	if(rl <= 0) {
		if(errno == EAGAIN || errno == EINTR) {
			goto retry_wait;
		}
		error_close(ctx, CLOUDY_RES_CLOSED);
		printf("eag\n");
		return false;
	}
//printf("rl %ld\n", rl);
//printf("current stream %p\n", stream);

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
//	printf("buffer used %lu received %lu\n", stream->used, ctx->received);
		header = (cloudy_header*)cloudy_stream_buffer(stream);
		cloudy_header_unpack((char*)header, header);
		if(header->magic != CLOUDY_RESPONSE || header->seqid == 0) {
			ctx->hparsed = NULL;
			error_close(ctx, CLOUDY_RES_SERVER_ERROR);
		printf("unkn seqid=%u magic=%hu %d\n", header->seqid, header->magic, CLOUDY_RESPONSE);
			return false;
		}

		if(ctx->received - sizeof(cloudy_header) < header->bodylen) {
			ctx->hparsed = header;
			return true;
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


