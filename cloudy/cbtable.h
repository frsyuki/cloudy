 /*
  * cloudy cbtable
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
#ifndef CLOUDY_CBTABLE_H__
#define CLOUDY_CBTABLE_H__

#include "cloudy/types.h"
#include "cloudy/stream.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct cloudy_entry {
	cloudy_data data;  /* must be first */
	cloudy_seqid_t seqid;
} cloudy_entry;

typedef struct cloudy_cbtable {
	size_t nmap;
	cloudy_entry** map;
	uint32_t nextid;
} cloudy_cbtable;

bool cloudy_cbtable_init(cloudy_cbtable* cbtable);
void cloudy_cbtable_destroy(cloudy_cbtable* cbtable);

cloudy_entry* cloudy_cbtable_add(
		cloudy_cbtable* cbtable,
		cloudy_header* header_template);

cloudy_entry* cloudy_cbtable_callback(
		cloudy_cbtable* cbtable,
		const cloudy_header* pos_header,
		cloudy_stream_reference* reference);

void cloudy_cbtable_callback_all(
		cloudy_cbtable* cbtable, cloudy_return ret);

bool cloudy_cbtable_empty(cloudy_cbtable* cbtable);


#ifdef __cplusplus
}
#endif

#endif /* cloudy/cbtable.h */

