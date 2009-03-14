#include "cloudy/memcache.h"

cloudy_data* cloudy_get(cloudy* ctx,
		const char* key, size_t keylen)
{
	cloudy_header header = CLOUDY_HEADER_INITIALIZER(
			CLOUDY_REQUEST,
			CLOUDY_CMD_GETK, //CLOUDY_CMD_GET,
			keylen, 0, 0,
			CLOUDY_TYPE_RAW_BYTES,
			0, 0, 0);

	cloudy_data* data =
		cloudy_send_request_async(ctx, &header, key, NULL, NULL);
	if(!data) {
		return NULL;
	}

	cloudy_sync_response(ctx, data);
	return data;
}

cloudy_return cloudy_set(cloudy* ctx,
		const char* key, size_t keylen,
		const char* val, size_t vallen,
		uint32_t flags, uint32_t expiration)
{
	char extra[8];
	cloudy_header header = CLOUDY_HEADER_INITIALIZER(
			CLOUDY_REQUEST,
			CLOUDY_CMD_SET,
			keylen, vallen, sizeof(extra),
			CLOUDY_TYPE_RAW_BYTES,
			0, 0, 0);
	*((uint32_t*)(extra+0)) = htonl(flags);
	*((uint32_t*)(extra+4)) = htonl(expiration);

	cloudy_data* data =
		cloudy_send_request_async(ctx, &header, key, val, extra);
	if(!data) {
		return CLOUDY_RES_IO_ERROR;
	}

	cloudy_sync_response(ctx, data);

	cloudy_return ret = cloudy_data_error(data);
	cloudy_data_free(data);

	return ret;
}

