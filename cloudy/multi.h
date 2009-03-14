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

