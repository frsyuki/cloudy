
struct cloudy_multi;
typedef struct cloudy_multi cloudy_multi;

cloudy_multi* cloudy_multi_new(void);
void cloudy_multi_free(cloudy_multi* multi);

bool cloudy_multi_add(cloudy_multi* multi, cloudy_data data);
cloudy_data* cloudy_multi_sync(cloudy* ctx, cloudy_multi* multi);

