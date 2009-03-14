#ifndef CLOUDY_TYPES_H
#define CLOUDY_TYPES_H

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef uint32_t cloudy_seqid_t;
typedef uint16_t cloudy_return;

typedef enum {
	CLOUDY_REQUEST                = 0x80,
	CLOUDY_RESPONSE               = 0x81,
} cloudy_magic;


typedef enum {
	CLOUDY_RES_NO_ERROR           = 0x0000,
	CLOUDY_RES_KEY_NOT_FOUND      = 0x0001,
	CLOUDY_RES_KEY_EXISTS         = 0x0002,
	CLOUDY_RES_VALUE_TOO_BIG      = 0x0003,
	CLOUDY_RES_INVALID_ARGUMENTS  = 0x0004,
	CLOUDY_RES_ITEM_NOT_STORED    = 0x0005,
	CLOUDY_RES_UNKNOWN_COMMAND    = 0x0081,
	CLOUDY_RES_OUT_OF_MEMORY      = 0x0082,

	// FIXME
	CLOUDY_RES_TIMEOUT            = 0x00c0,
	CLOUDY_RES_CLOSED             = 0x00c1,
	CLOUDY_RES_IO_ERROR           = 0x00c2,
	CLOUDY_RES_SERVER_ERROR       = 0x00c3,
} cloudy_response_status;


typedef enum {
	CLOUDY_CMD_GET                 = 0x00,
	CLOUDY_CMD_SET                 = 0x01,
	CLOUDY_CMD_ADD                 = 0x02,
	CLOUDY_CMD_REPLACE             = 0x03,
	CLOUDY_CMD_DELETE              = 0x04,
	CLOUDY_CMD_INCREMENT           = 0x05,
	CLOUDY_CMD_DECREMENT           = 0x06,
	CLOUDY_CMD_QUIT                = 0x07,
	CLOUDY_CMD_FLUSH               = 0x08,
	CLOUDY_CMD_GETQ                = 0x09,
	CLOUDY_CMD_NOOP                = 0x0a,
	CLOUDY_CMD_VERSION             = 0x0b,
	CLOUDY_CMD_GETK                = 0x0c,
	CLOUDY_CMD_GETKQ               = 0x0d,
	CLOUDY_CMD_APPEND              = 0x0e,
	CLOUDY_CMD_PREPEND             = 0x0f,
} cloudy_command;


typedef enum {
	CLOUDY_TYPE_RAW_BYTES          = 0x00,
} cloudy_datatype;


//#pragma pack(push, 1)
typedef struct cloudy_header {
	uint8_t magic;
	uint8_t opcode;
	uint16_t keylen;
	uint8_t extralen;
	uint8_t type;
	uint16_t stat;
	uint32_t bodylen;
	uint32_t seqid;
	uint64_t cas;
}
__attribute__((packed))
cloudy_header;
//#pragma pack(pop)


static inline void cloudy_header_pack(char* src, const cloudy_header* dst);
static inline void cloudy_header_unpack(const char* src, cloudy_header* dst);



typedef struct cloudy_data {
	cloudy_header header;
	char* body;
	void* reference;
} cloudy_data;

static inline uint8_t  cloudy_data_extralen(cloudy_data* d);
static inline uint16_t cloudy_data_keylen(cloudy_data* d);
static inline uint32_t cloudy_data_vallen(cloudy_data* d);
static inline char* cloudy_data_extra(cloudy_data* d);
static inline char* cloudy_data_key(cloudy_data* d);
static inline char* cloudy_data_val(cloudy_data* d);
static inline cloudy_return cloudy_data_error(cloudy_data* d);
static inline cloudy_return* cloudy_data_error_ref(cloudy_data* d);



uint8_t  cloudy_data_extralen(cloudy_data* d)
{
	return d->header.extralen;
}

uint16_t cloudy_data_keylen(cloudy_data* d)
{
	return d->header.keylen;
}

uint32_t cloudy_data_vallen(cloudy_data* d)
{
	return d->header.bodylen - d->header.extralen - d->header.keylen;
}

char* cloudy_data_extra(cloudy_data* d)
{
	return d->body;
}

char* cloudy_data_key(cloudy_data* d)
{
	return d->body + d->header.extralen;
}

char* cloudy_data_val(cloudy_data* d)
{
	return d->body + d->header.extralen + d->header.keylen;
}

cloudy_return cloudy_data_error(cloudy_data* d)
{
	if(!d) { return CLOUDY_RES_OUT_OF_MEMORY; }
	return d->header.stat;
}

cloudy_return* cloudy_data_error_ref(cloudy_data* d)
{
	if(!d) { return NULL; }
	return &d->header.stat;
}


#if !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define __LITTLE_ENDIAN__
#elif __BYTE_ORDER == __BIG_ENDIAN
#define __BIG_ENDIAN__
#endif
#endif

#ifdef __LITTLE_ENDIAN__
#if defined(__bswap_64)
#  define cloudy_be64h(x) __bswap_64(x)
#elif defined(__DARWIN_OSSwapInt64)
#  define cloudy_be64h(x) __DARWIN_OSSwapInt64(x)
#else
static inline uint64_t cloudy_be64h(uint64_t x) {
	return	((x << 56) & 0xff00000000000000ULL ) |
			((x << 40) & 0x00ff000000000000ULL ) |
			((x << 24) & 0x0000ff0000000000ULL ) |
			((x <<  8) & 0x000000ff00000000ULL ) |
			((x >>  8) & 0x00000000ff000000ULL ) |
			((x >> 24) & 0x0000000000ff0000ULL ) |
			((x >> 40) & 0x000000000000ff00ULL ) |
			((x >> 56) & 0x00000000000000ffULL ) ;
}
#endif
#else
#define cloudy_be64h(x) (x)
#endif

#define CLOUDY_HEADER_INITIALIZER(magic, opcode, keylen, vallen, extralen, type, stat, seqid, cas) \
	{ magic \
	, opcode \
	, keylen \
	, extralen \
	, type \
	, stat \
	, keylen + vallen + extralen \
	, seqid \
	, cas }


void cloudy_header_pack(char* src, const cloudy_header* dst)
{
	*((uint8_t *)&src[ 0]) =       dst->magic;
	*((uint8_t *)&src[ 1]) =       dst->opcode;
	*((uint16_t*)&src[ 2]) = htons(dst->keylen);
	*((uint8_t *)&src[ 4]) =       dst->extralen;
	*((uint8_t *)&src[ 5]) =       dst->type;
	*((uint16_t*)&src[ 6]) = htons(dst->stat);
	*((uint32_t*)&src[ 8]) = ntohl(dst->bodylen);
	*((uint32_t*)&src[12]) = ntohl(dst->seqid);
	*((uint64_t*)&src[16]) = cloudy_be64h(dst->cas);
}

void cloudy_header_unpack(const char* src, cloudy_header* dst)
{
	dst->magic    =       *((uint8_t *)&src[ 0]);
	dst->opcode   =       *((uint8_t *)&src[ 1]);
	dst->keylen   = htons(*((uint16_t*)&src[ 2]));
	dst->extralen =       *((uint8_t *)&src[ 4]);
	dst->type     =       *((uint8_t *)&src[ 5]);
	dst->stat     = htons(*((uint16_t*)&src[ 6]));
	dst->bodylen  = htonl(*((uint32_t*)&src[ 8]));
	dst->seqid    = htonl(*((uint32_t*)&src[12]));
	dst->cas      = cloudy_be64h(*(uint64_t*)&src[16]);
}


#ifdef __cplusplus
}
#endif

#endif /* cloudy/types.h */

