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


#ifdef __cplusplus
}
#endif

#endif /* cloudy/cbtable.h */

