#include "cloudy/cbtable.h"
#include <stdlib.h>

#ifndef CLOUDY_CBTABLE_BASE_SIZE
#define CLOUDY_CBTABLE_BASE_SIZE 32
#endif

bool cloudy_cbtable_init(cloudy_cbtable* cbtable)
{
	cloudy_entry* const base = (cloudy_entry*)calloc(
			CLOUDY_CBTABLE_BASE_SIZE, sizeof(cloudy_entry));
	if(!base) {
		return false;
	}

	cloudy_entry** const map = (cloudy_entry**)calloc(
			1, sizeof(cloudy_entry*));
	if(!map) {
		free(base);
		return false;
	}

	map[0] = base;
	cbtable->map = map;
	cbtable->nmap = 1;
	cbtable->nextid = 1;

	return true;
}

void cloudy_cbtable_destroy(cloudy_cbtable* cbtable)
{
	cloudy_entry** const map = cbtable->map;

	size_t i;
	for(i=0; i < cbtable->nmap; ++i) {
		free(map[i]);
	}

	free(map);
}


static bool cloudy_cbtable_expand(cloudy_cbtable* cbtable)
{
	cloudy_entry* const base = (cloudy_entry*)calloc(
			CLOUDY_CBTABLE_BASE_SIZE, sizeof(cloudy_entry));
	if(!base) {
		return false;
	}

	cloudy_entry** const map = (cloudy_entry**)realloc(
			cbtable->map,
			sizeof(cloudy_entry*) * (cbtable->nmap+1));
	if(!map) {
		free(base);
		return false;
	}

	cbtable->map = map;
	cbtable->map[cbtable->nmap] = base;
	cbtable->nmap += 1;

	return true;
}

cloudy_entry* cloudy_cbtable_add(
		cloudy_cbtable* cbtable,
		cloudy_header* header_template)
{
	uint32_t seqid = cbtable->nextid++;
	if(cbtable->nextid == 0) {
		cbtable->nextid = 1;
	}

	header_template->seqid = seqid;

	cloudy_entry* p;

	size_t n = 0;
	do {
		p = cbtable->map[n];
		cloudy_entry* const pend = p + CLOUDY_CBTABLE_BASE_SIZE;
		for(; p != pend; ++p) {
			if(p->seqid == 0) {
				goto found_usable;
			}
		}
	} while(++n < cbtable->nmap);

	if(!cloudy_cbtable_expand(cbtable)) {
		return NULL;
	}

	p = cbtable->map[cbtable->nmap-1];

found_usable:
	p->seqid = seqid;
	p->data.header = *header_template;
	p->data.body = NULL;
	p->data.reference = NULL;
	return p;
}

cloudy_entry* cloudy_cbtable_callback(
		cloudy_cbtable* cbtable,
		const cloudy_header* pos_header,
		cloudy_stream_reference* reference)
{
	const uint32_t seqid = pos_header->seqid;

	size_t n = 0;
	do {
		cloudy_entry* p = cbtable->map[n];
		cloudy_entry* const pend = p + CLOUDY_CBTABLE_BASE_SIZE;
		for(; p != pend; ++p) {
			if(p->seqid == seqid) {

				p->data.header = *pos_header;  /* CLOUDY_RESPONSE */
				p->data.body = ((char*)pos_header) + sizeof(cloudy_header);
				p->data.reference = cloudy_stream_reference_copy(reference);
				return p;

			}
		}
	} while(++n < cbtable->nmap);

	return NULL;
}

void cloudy_cbtable_callback_all(
		cloudy_cbtable* cbtable, cloudy_return ret)
{
	size_t n = 0;
	do {
		cloudy_entry* p = cbtable->map[n];
		cloudy_entry* const pend = p + CLOUDY_CBTABLE_BASE_SIZE;
		for(; p != pend; ++p) {
			if(p->seqid != 0 && p->data.header.magic != CLOUDY_RESPONSE) {

				cloudy_header header = CLOUDY_HEADER_INITIALIZER(
						CLOUDY_RESPONSE,
						p->data.header.opcode,
						0, 0, 0,
						CLOUDY_TYPE_RAW_BYTES,
						ret, p->data.header.seqid, 0);

				p->data.header = header;

			}
		}
	} while(++n < cbtable->nmap);
}


bool cloudy_cbtable_empty(cloudy_cbtable* cbtable)
{
	size_t n = 0;
	do {
		cloudy_entry* p = cbtable->map[n];
		cloudy_entry* const pend = p + CLOUDY_CBTABLE_BASE_SIZE;
		for(; p != pend; ++p) {
			if(p->seqid == 0) {
				return false;
			}
		}
	} while(++n < cbtable->nmap);
	return true;
}

