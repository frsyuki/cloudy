 /*
  * cloudy multi
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
#ifndef CLOUDY_MULTI_H__
#define CLOUDY_MULTI_H__

#include "cloudy/core.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cloudy_multi;
typedef struct cloudy_multi cloudy_multi;

cloudy_multi* cloudy_multi_new(cloudy* ctx);
void cloudy_multi_free(cloudy_multi* multi);

bool cloudy_multi_add(cloudy_multi* multi, cloudy_data* data);
bool cloudy_multi_sync_all(cloudy_multi* multi);

cloudy* cloudy_multi_ctx(cloudy_multi* multi);


#ifdef __cplusplus
}
#endif

#endif /* cloudy/multi.h */

