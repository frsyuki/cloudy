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

