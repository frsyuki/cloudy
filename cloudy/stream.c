 /*
  * cloudy stream
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
#include "cloudy/stream.h"
#include <string.h>

bool cloudy_stream_expand_buffer(cloudy_stream* stream,
		size_t size, size_t init_size)
{
	if(stream->used == CLOUDY_STREAM_COUNTER_SIZE) {
		size_t next_size = (stream->used + stream->free) * 2;
		while(next_size < size + stream->used) { next_size *= 2; }

		char* tmp = (char*)realloc(stream->buffer, next_size);
		if(!tmp) { return false; }

		stream->buffer = tmp;
		stream->free = next_size - stream->used;

	} else {
		const size_t initsz = (init_size > CLOUDY_STREAM_COUNTER_SIZE) ?
			init_size : CLOUDY_STREAM_COUNTER_SIZE;

		size_t next_size = initsz;  // include CLOUDY_STREAM_COUNTER_SIZE
		while(next_size < size + CLOUDY_STREAM_COUNTER_SIZE) { next_size *= 2; }

		char* tmp = (char*)malloc(next_size);
		if(!tmp) { return false; }
		CLOUDY_STREAM_INIT_COUNT(tmp);

		CLOUDY_STREAM_DECR_COUNT(stream->buffer);
memcpy(tmp+CLOUDY_STREAM_COUNTER_SIZE, stream->buffer+stream->used, stream->free);

		stream->buffer = tmp;
		stream->used = CLOUDY_STREAM_COUNTER_SIZE;
		stream->free = next_size - stream->used;
	}

	return true;
}

