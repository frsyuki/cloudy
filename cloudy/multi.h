#ifndef CLOUDY_MULTI_H__
#define CLOUDY_MULTI_H__

#include "core.h"

struct cloudy_multi;
typedef struct cloudy_multi cloudy_multi;

cloudy_multi* cloudy_multi_new(cloudy* ctx);
void cloudy_multi_free(cloudy_multi* multi);

bool cloudy_multi_add(cloudy_multi* multi, cloudy_data* data);
bool cloudy_multi_sync_all(cloudy_multi* multi);

cloudy* cloudy_multi_ctx(cloudy_multi* multi);

#endif /* cloudy/multi.h */

