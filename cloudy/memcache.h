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
#ifndef CLOUDY_MEMCACHE_H__
#define CLOUDY_MEMCACHE_H__

#include "cloudy/core.h"
#include "cloudy/multi.h"

#ifdef __cplusplus
extern "C" {
#endif


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


#ifdef __cplusplus
}
#endif

#endif /* cloudy/memcache.h */

