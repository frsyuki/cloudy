#ifndef CLOUDY_STREAM_H__
#define CLOUDY_STREAM_H__

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

struct cloudy_stream_reference;
typedef struct cloudy_stream_reference cloudy_stream_reference;

typedef struct cloudy_stream {
	char* buffer;
	size_t used;
	size_t free;
} cloudy_stream;


static inline bool cloudy_stream_init(cloudy_stream* stream, size_t init_size);
static inline void cloudy_stream_destroy(cloudy_stream* stream);

static inline bool cloudy_stream_reserve_buffer(cloudy_stream* stream,
		size_t size, size_t init_size);

static inline void* cloudy_stream_buffer(cloudy_stream* stream);
static inline size_t cloudy_stream_buffer_capacity(const cloudy_stream* stream);

static inline cloudy_stream_reference* cloudy_stream_allocate(cloudy_stream* stream,
		size_t size);

static inline void cloudy_stream_reference_free(cloudy_stream_reference* reference);
static inline cloudy_stream_reference* cloudy_stream_reference_copy(cloudy_stream_reference* reference);


bool cloudy_stream_expand_buffer(cloudy_stream* stream,
		size_t size, size_t init_size);


#define CLOUDY_STREAM_COUNTER_TYPE unsigned int
#define CLOUDY_STREAM_COUNTER_SIZE sizeof(CLOUDY_STREAM_COUNTER_TYPE)

#define CLOUDY_STREAM_GET_COUNT(b) (*(volatile CLOUDY_STREAM_COUNTER_TYPE*)(b))

#define CLOUDY_STREAM_INIT_COUNT(b) \
	do { *(volatile CLOUDY_STREAM_COUNTER_TYPE*)(b) = 1; } while(0)

#define CLOUDY_STREAM_INCR_COUNT(b) \
	do { __sync_add_and_fetch((CLOUDY_STREAM_COUNTER_TYPE*)(b), 1); } while(0)

#define CLOUDY_STREAM_DECR_COUNT(b) \
	do { \
		if(__sync_sub_and_fetch((CLOUDY_STREAM_COUNTER_TYPE*)(b), 1) == 0) { \
			free(b); \
		} \
	} while(0)

bool cloudy_stream_init(cloudy_stream* stream, size_t init_size)
{
	if(init_size < CLOUDY_STREAM_COUNTER_SIZE) {
		return false;
	}

	stream->buffer = (char*)malloc(init_size);
	if(!stream->buffer) {
		return false;
	}
	CLOUDY_STREAM_INIT_COUNT(stream->buffer);

	stream->used = CLOUDY_STREAM_COUNTER_SIZE;
	stream->free = init_size - CLOUDY_STREAM_COUNTER_SIZE;

	return true;
}

void cloudy_stream_destroy(cloudy_stream* stream)
{
	CLOUDY_STREAM_DECR_COUNT(stream->buffer);
}


void* cloudy_stream_buffer(cloudy_stream* stream)
{
	return stream->buffer + stream->used;
}

size_t cloudy_stream_buffer_capacity(const cloudy_stream* stream)
{
	return stream->free;
}


bool cloudy_stream_reserve_buffer(cloudy_stream* stream,
		size_t size, size_t init_size)
{
	if(CLOUDY_STREAM_GET_COUNT(stream->buffer) == 1) {
		/* rewind buffer */
		stream->free += stream->used - CLOUDY_STREAM_COUNTER_SIZE;
		stream->used = CLOUDY_STREAM_COUNTER_SIZE;
	}
	if(stream->free < size) {
		return cloudy_stream_expand_buffer(stream, size, init_size);
	}
	return true;
}

cloudy_stream_reference* cloudy_stream_allocate(cloudy_stream* stream, size_t size)
{
	if(stream->free < size) {
		return NULL;
	}
	stream->used += size;
	stream->free -= size;
	CLOUDY_STREAM_INCR_COUNT(stream->buffer);
	return (cloudy_stream_reference*)stream->buffer;
}

void cloudy_stream_reference_free(cloudy_stream_reference* reference)
{
	CLOUDY_STREAM_DECR_COUNT(reference);
}

cloudy_stream_reference* cloudy_stream_reference_copy(cloudy_stream_reference* reference)
{
	CLOUDY_STREAM_INCR_COUNT(reference);
	return reference;
}

#endif /* cloudy/stream.h */

