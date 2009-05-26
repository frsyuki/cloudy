 /*
  * cloudy memcache
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
#include "cloudy/memcache.h"

static cloudy_data* get_async(cloudy* ctx,
		const char* key, size_t keylen)
{
	cloudy_header header = CLOUDY_HEADER_INITIALIZER(
			CLOUDY_REQUEST,
			CLOUDY_CMD_GETK, //CLOUDY_CMD_GET,
			keylen, 0, 0,
			CLOUDY_TYPE_RAW_BYTES,
			0, 0, 0);

	return cloudy_send_request_async(ctx, &header, key, NULL, NULL);
}

static cloudy_data* set_async(cloudy* ctx,
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

	return cloudy_send_request_async(ctx, &header, key, val, extra);
}


cloudy_data* cloudy_get(cloudy* ctx,
		const char* key, size_t keylen)
{
	cloudy_data* data = get_async(ctx, key, keylen);
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
	cloudy_data* data = set_async(ctx,
			key, keylen, val, vallen,
			flags, expiration);
	if(!data) {
		return CLOUDY_RES_IO_ERROR;
	}

	cloudy_sync_response(ctx, data);

	cloudy_return ret = cloudy_data_error(data);
	cloudy_data_free(data);

	return ret;
}

cloudy_data* cloudy_get_async(cloudy_multi* multi,
		const char* key, size_t keylen)
{
	cloudy* ctx = cloudy_multi_ctx(multi);
	cloudy_data* data = get_async(ctx, key, keylen);
	if(!data) {
		return NULL;
	}

	if(!cloudy_multi_add(multi, data)) {
		cloudy_data_free(data);
		return NULL;
	}

	return data;
}

cloudy_return* cloudy_set_async(cloudy_multi* multi,
		const char* key, size_t keylen,
		const char* val, size_t vallen,
		uint32_t flags, uint32_t expiration)
{
	cloudy* ctx = cloudy_multi_ctx(multi);
	cloudy_data* data = set_async(ctx,
			key, keylen, val, vallen,
			flags, expiration);
	if(!data) {
		return NULL;
	}

	if(!cloudy_multi_add(multi, data)) {
		cloudy_data_free(data);
		return NULL;
	}

	return cloudy_data_error_ref(data);
}

