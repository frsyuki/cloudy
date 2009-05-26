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
#ifndef CLOUDY_CORE_H__
#define CLOUDY_CORE_H__

#include "cloudy/types.h"
#include <stdbool.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif


struct cloudy;
typedef struct cloudy cloudy;

cloudy* cloudy_new(struct sockaddr* addr, socklen_t addrlen, struct timeval timeout);
void cloudy_free(cloudy* ctx);

bool cloudy_sync_response(cloudy* ctx, cloudy_data* data);

cloudy_data* cloudy_send_request_async(cloudy* ctx, cloudy_header* header,
		const char* key, const char* val, const char* extra);

bool cloudy_send_request_flush(cloudy* ctx);

void cloudy_data_free(cloudy_data* data);


#ifdef __cplusplus
}
#endif

#endif /* cloudy/core.h */

