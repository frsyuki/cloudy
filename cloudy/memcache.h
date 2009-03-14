#ifndef CLOUDY_MEMCACHE_H__
#define CLOUDY_MEMCACHE_H__

#include "cloudy/core.h"
#include "cloudy/multi.h"

cloudy_data* cloudy_get(cloudy* ctx,
		const char* key, size_t keylen);

cloudy_return cloudy_set(cloudy* ctx,
		const char* key, size_t keylen,
		const char* val, size_t vallen,
		uint32_t flags, uint32_t expiration);

//cloudy_return cloudy_del(cloudy* ctx,
//		const char* key, size_t keylen);


cloudy_data* cloudy_get_async(cloudy_multi* multi,
		const char* key, size_t keylen);

cloudy_return* cloudy_set_async(cloudy_multi* multi,
		const char* key, size_t keylen,
		const char* val, size_t vallen,
		uint32_t flags, uint32_t expiration);

//cloudy_return* cloudy_del_async(cloudy_multi* multi,
//		const char* key, size_t keylen);


#endif /* cloudy/memcache.h */

