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
#include "cloudy/multi.h"

typedef struct cloudy_multi_entry {
	cloudy_data* data;
	bool enable;
} cloudy_multi_entry;

struct cloudy_multi {
	cloudy_multi_entry* tail;
	cloudy_multi_entry* end;
	cloudy_multi_entry* array;
	cloudy* ctx;
};

cloudy_multi* cloudy_multi_new(cloudy* ctx)
{
	cloudy_multi* multi = calloc(1, sizeof(cloudy_multi));
	if(!multi) {
		return NULL;
	}
	multi->ctx = ctx;
	return multi;
}

void cloudy_multi_free(cloudy_multi* multi)
{
	cloudy_multi_entry* e = multi->array;
	for(; e != multi->tail; ++e) {
		cloudy_data_free(e->data);
	}
	free(multi->array);
	free(multi);
}

bool cloudy_multi_add(cloudy_multi* multi, cloudy_data* data)
{
	cloudy_multi_entry* e = multi->tail;

	if(e == multi->end) {
		const size_t nused = multi->end - multi->array;

		size_t nnext;
		if(nused == 0) {
			nnext = (sizeof(cloudy_multi_entry) < 72/2) ?
					72 / sizeof(cloudy_multi_entry) : 8;
	
		} else {
			nnext = nused * 2;
		}

		cloudy_multi_entry* tmp =
			(cloudy_multi_entry*)realloc(multi->array,
					sizeof(cloudy_multi_entry) * nnext);
		if(tmp == NULL) {
			return false;
		}

		multi->array  = tmp;
		multi->end    = tmp + nnext;
		multi->tail   = tmp + nused;

		e = multi->tail;
	}

	e->data = data;
	e->enable = true;

	++multi->tail;

	return true;
}

bool cloudy_multi_sync_all(cloudy_multi* multi)
{
	cloudy_multi_entry* e = multi->array;
	for(; e != multi->tail; ++e) {
		if(e->enable) {
			if(!cloudy_sync_response(multi->ctx, e->data)) {
				return false;
			}
			e->enable = false;
		}
	}
	return true;
}

cloudy* cloudy_multi_ctx(cloudy_multi* multi)
{
	return multi->ctx;
}

