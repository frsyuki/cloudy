#ifndef CLOUDY_WBUFFER_H__
#define CLOUDY_WBUFFER_H__

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CLOUDY_WBUFFER_INIT_SIZE
#define CLOUDY_WBUFFER_INIT_SIZE 2048
#endif

typedef struct cloudy_wbuffer {
	size_t size;
	char* data;
	size_t alloc;
} cloudy_wbuffer;

static inline void cloudy_wbuffer_init(cloudy_wbuffer* wbuf)
{
	memset(wbuf, 0, sizeof(cloudy_wbuffer));
}

static inline void cloudy_wbuffer_destroy(cloudy_wbuffer* wbuf)
{
	free(wbuf->data);
}

static inline bool cloudy_wbuffer_append(void* data, const char* buf, unsigned int size)
{
	cloudy_wbuffer* wbuf = (cloudy_wbuffer*)data;

	if(wbuf->alloc - wbuf->size < size) {
		size_t nsize = (wbuf->alloc) ?
				wbuf->alloc * 2 : CLOUDY_WBUFFER_INIT_SIZE;

		while(nsize < wbuf->size + size) { nsize *= 2; }

		void* tmp = realloc(wbuf->data, nsize);
		if(!tmp) { return false; }

		wbuf->data = (char*)tmp;
		wbuf->alloc = nsize;
	}

	memcpy(wbuf->data + wbuf->size, buf, size);
	wbuf->size += size;

	return true;
}

static inline bool cloudy_wbuffer_empty(cloudy_wbuffer* wbuf)
{
	return wbuf->size == 0;
}

static inline char* cloudy_wbuffer_release(cloudy_wbuffer* wbuf)
{
	char* tmp = wbuf->data;
	wbuf->size = 0;
	wbuf->data = NULL;
	wbuf->alloc = 0;
	return tmp;
}


#ifdef __cplusplus
}
#endif

#endif /* cloudy/wbuffer.h */

