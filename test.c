#include "cloudy/memcache.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	const char* host = "127.0.0.1";
	unsigned short port = 11211;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
#ifndef _WIN32
	inet_aton(host, &addr.sin_addr);
#else
	addr.sin_addr.s_addr = inet_addr(host); 
#endif

	struct timeval timeout = {1, 0};

	cloudy* cl = cloudy_new((struct sockaddr*)&addr, sizeof(addr), timeout);
	if(!cl) {
		fprintf(stderr, "cloudy init failed\n");
		exit(1);
	}

	cloudy_multi* multi;

	multi = cloudy_multi_new(cl);
	if(!multi) {
		fprintf(stderr, "cloudy_multi init failed\n");
		exit(1);
	}

	cloudy_return* ret[3];
	ret[0] = cloudy_set_async(multi, "key0", 4, "val0", 4, 0, 0);
	ret[1] = cloudy_set_async(multi, "key1", 4, "val1", 4, 0, 0);
	ret[2] = cloudy_set_async(multi, "key2", 4, "val2", 4, 0, 0);
	if(!ret[0] || !ret[1] || !ret[2]) {
		fprintf(stderr, "cloudy_set_async failed\n");
		exit(1);
	}

	if(!cloudy_multi_sync_all(multi)) {
		fprintf(stderr, "cloudy_multi_sync_all failed\n");
		exit(1);
	}

	cloudy_multi_free(multi);


	multi = cloudy_multi_new(cl);
	if(!multi) {
		fprintf(stderr, "cloudy_multi init failed\n");
		exit(1);
	}

	cloudy_data* data[3];
	data[0] = cloudy_get_async(multi, "key0", 4);
	data[1] = cloudy_get_async(multi, "key1", 4);
	data[2] = cloudy_get_async(multi, "key2", 4);
	if(!data[0] || !data[1] || !data[2]) {
		fprintf(stderr, "cloudy_get_async failed\n");
		exit(1);
	}

	if(!cloudy_multi_sync_all(multi)) {
		fprintf(stderr, "cloudy_multi_sync_all failed\n");
		exit(1);
	}

	int i;
	for(i=0; i < 3; ++i) {
		printf("'");
		fwrite(cloudy_data_val(data[i]), cloudy_data_vallen(data[i]), 1, stdout);
		printf("'\n");
	}

	cloudy_multi_free(multi);

	return 0;
}

