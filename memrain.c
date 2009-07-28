 /*
  * memrain
  *
  * Copyright (C) 2008-2009 FURUHASHI Sadayuki
  *
  *    Licensed under the Apache License, Version 2.0 (the "License");
  *    you may not use this file except in compliance with the License.
  *    You may obtain a copy of the License at
  *
  *        http://www.apache.org/licenses/LICENSE-2.0
  *
  *    Unless required by applicable law or agreed to in writing, software
  *    distributed under the License is distributed on an "AS IS" BASIS,
  *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  *    See the License for the specific language governing permissions and
  *    limitations under the License.
  */
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <memory.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include <pthread.h>
#include "cloudy/memcache.h"

extern char* optarg;
extern int optint, opterr, optopt;
const char* g_progname;

static const char* g_host = "127.0.0.1";
static unsigned short g_port = 11211;

static unsigned long g_num_request;
static unsigned long g_num_multiplex = 1;
static unsigned long g_num_thread = 1;
static unsigned long g_keylen = 8;
static unsigned long g_vallen = 1024;
static bool g_noset = false;
static bool g_noget = false;

static pthread_mutex_t g_count_lock;
static pthread_cond_t  g_count_cond;
static volatile int g_thread_count;
static pthread_mutex_t g_thread_lock;

#define KEY_FILL 'k'

static struct timeval g_timer;

void reset_timer()
{
	gettimeofday(&g_timer, NULL);
}

void show_timer()
{
	struct timeval endtime;
	double sec;
	unsigned long size_bytes = (g_keylen+g_vallen) * g_num_request * g_num_thread;
	unsigned long requests = g_num_request * g_num_thread;
	gettimeofday(&endtime, NULL);
	sec = (endtime.tv_sec - g_timer.tv_sec)
		+ (double)(endtime.tv_usec - g_timer.tv_usec) / 1000 / 1000;
	printf("%f sec\n", sec);
	printf("%f MB\n", ((double)size_bytes)/1024/1024);
	printf("%f Mbps\n", ((double)size_bytes)*8/sec/1000/1000);
	printf("%f req/sec\n", ((double)requests)/sec);
	printf("%f usec/req\n", ((double)sec)/requests*1000*1000);
}


static pthread_t* create_worker(void* (*func)(void*))
{
	unsigned long i;
	pthread_t* threads = malloc(sizeof(pthread_t)*g_num_thread);

	pthread_mutex_lock(&g_thread_lock);
	g_thread_count = 0;

	for(i=0; i < g_num_thread; ++i) {
		int err = pthread_create(&threads[i], NULL, func, NULL);
		if(err != 0) {
			fprintf(stderr, "failed to create thread: %s\n", strerror(err));
			exit(1);
		}
	}

	pthread_mutex_lock(&g_count_lock);
	while(g_thread_count < g_num_thread) {
		pthread_cond_wait(&g_count_cond, &g_count_lock);
	}
	pthread_mutex_unlock(&g_count_lock);

	return threads;
}

static void start_worker()
{
	pthread_mutex_unlock(&g_thread_lock);
}

static void join_worker(pthread_t* threads)
{
	unsigned long i;
	for(i=0; i < g_num_thread; ++i) {
		void* ret;
		int err = pthread_join(threads[i], &ret);
		if(err != 0) {
			fprintf(stderr, "failed to join thread: %s\n", strerror(err));
		}
	}
}

static unsigned long wait_worker_ready()
{
	unsigned long index;
	pthread_mutex_lock(&g_count_lock);
	index = g_thread_count++;
	pthread_cond_signal(&g_count_cond);
	pthread_mutex_unlock(&g_count_lock);
	pthread_mutex_lock(&g_thread_lock);
	pthread_mutex_unlock(&g_thread_lock);
	return index;
}


static cloudy* initialize_user()
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(g_port);
#ifndef _WIN32
	inet_aton(g_host, &addr.sin_addr);  /* FIXME resolve host name */
#else
	addr.sin_addr.s_addr = inet_addr(g_host); 
#endif

	struct timeval timeout = {5, 0};

	cloudy* cl = cloudy_new((struct sockaddr*)&addr, sizeof(addr), timeout);
	if(!cl) {
		perror("cloudy_new failed");
		exit(1);
	}

	return cl;
}

static char* malloc_keybuf()
{
	char* keybuf = malloc(g_keylen+1);
	if(!keybuf) {
		perror("malloc for key failed");
		exit(1);
	}
	memset(keybuf, KEY_FILL, g_keylen);
	keybuf[g_keylen] = '\0';
	return keybuf;
}

static char* malloc_valbuf()
{
	char* valbuf = malloc(g_vallen);
	if(!valbuf) {
		perror("malloc for value failed");
		exit(1);
	}
	memset(valbuf, 'v', g_vallen);
	//memset(valbuf, 0, g_vallen);
	return valbuf;
}

static void pack_keynum(char* keybuf, uint32_t i)
{
	/* 0x40 - 0x4f is printable ascii character */
	unsigned char* prefix = (unsigned char*)keybuf + g_keylen - 8;
	prefix[0] = ((i >> 0) & 0x0f) + 0x40;
	prefix[1] = ((i >> 4) & 0x0f) + 0x40;
	prefix[2] = ((i >> 8) & 0x0f) + 0x40;
	prefix[3] = ((i >>12) & 0x0f) + 0x40;
	prefix[4] = ((i >>16) & 0x0f) + 0x40;
	prefix[5] = ((i >>20) & 0x0f) + 0x40;
	prefix[6] = ((i >>24) & 0x0f) + 0x40;
	prefix[7] = ((i >>28) & 0x0f) + 0x40;
}

static void* worker_set(void* trash)
{
	unsigned long i, t, m;
	cloudy* cl = initialize_user();
	char* keybuf = malloc_keybuf();
	char* valbuf = malloc_valbuf();
	
	printf("s");
	t = wait_worker_ready();

	if(g_num_multiplex == 1) {
		cloudy_return ret;
		for(i=t*g_num_request, t=i+g_num_request; i < t; ++i) {
			pack_keynum(keybuf, i);
			ret = cloudy_set(cl, keybuf, g_keylen, valbuf, g_vallen, 0, 0);
			if(ret != CLOUDY_RES_NO_ERROR) {
				fprintf(stderr, "get failed\n");
			}
		}
	} else {
		cloudy_return* ret[g_num_multiplex];
		for(i=t*g_num_request, t=i+g_num_request; i < t; ) {
			cloudy_multi* multi = cloudy_multi_new(cl);
	
			cloudy_return** retp = ret;
			for(m=i+g_num_multiplex; i < m; ++i) {
				pack_keynum(keybuf, i);
				*retp = cloudy_set_async(multi, keybuf, g_keylen, valbuf, g_vallen, 0, 0);
				if(!*retp) {
					fprintf(stderr, "set async failed\n");
				}
				++retp;
			}
	
			if(!cloudy_multi_sync_all(multi)) {
				fprintf(stderr, "sync all failed\n");
			} else {
				cloudy_return** reti = ret;
				for(; reti != retp; ++reti) {
					if(**reti != CLOUDY_RES_NO_ERROR) {
						perror("set failed");
					}
				}
			}
	
			cloudy_multi_free(multi);
		}
	}
	
	free(keybuf);
	free(valbuf);
	cloudy_free(cl);
	return NULL;
}

static void* worker_get(void* trash)
{
	unsigned long i, t, m;
	cloudy* cl = initialize_user();
	char* keybuf = malloc_keybuf();

	printf("g");
	t = wait_worker_ready();

	if(g_num_multiplex == 1) {
		cloudy_data* ret;
		for(i=t*g_num_request, t=i+g_num_request; i < t; ++i) {
			pack_keynum(keybuf, i);
			ret = cloudy_get(cl, keybuf, g_keylen);
			if(!ret) {
				fprintf(stderr, "get failed\n");
			}
			cloudy_data_free(ret);
		}
	} else {
		cloudy_data* ret[g_num_multiplex];
		for(i=t*g_num_request, t=i+g_num_request; i < t; ) {
			cloudy_multi* multi = cloudy_multi_new(cl);
	
			cloudy_data** retp = ret;
			for(m=i+g_num_multiplex; i < m; ++i) {
				pack_keynum(keybuf, i);
				*retp = cloudy_get_async(multi, keybuf, g_keylen);
				if(!*retp) {
					fprintf(stderr, "get async failed\n");
				}
				++retp;
			}
	
//printf("S\n");
			if(!cloudy_multi_sync_all(multi)) {
				fprintf(stderr, "sync all failed\n");
			} else {
				cloudy_data** reti = ret;
				for(; reti != retp; ++reti) {
					if(cloudy_data_error(*reti) != CLOUDY_RES_NO_ERROR) {
						perror("get failed");
					}
				}
			}
//printf("X\n");
	
			cloudy_multi_free(multi);
		}
	}

	free(keybuf);
	cloudy_free(cl);
	return NULL;
}


static void usage(const char* msg)
{
	printf("Usage: %s [options]    <num requests>\n"
		" -l HOST=127.0.0.1  : memcached server address\n"
		" -p PORT=11211      : memcached server port\n"
		" -k SIZE=8          : size of key >= 8\n"
		" -v SIZE=1024       : size of value\n"
		" -m NUM=1           : request multiplex\n"
		" -t NUM=1           : number of threads\n"
		" -g                 : omit to set values\n"
		" -s                 : omit to get benchmark\n"
		" -h                 : print this help message\n"
		, g_progname);
	if(msg) { printf("error: %s\n", msg); }
	exit(1);
}

static void parse_argv(int argc, char* argv[])
{
	int c;
	g_progname = argv[0];
	while((c = getopt(argc, argv, "hbgsl:p:k:v:m:t:")) != -1) {
		switch(c) {
		case 'l':
			g_host = optarg;
			break;

		case 'p':
			g_port = atoi(optarg);
			if(g_port == 0) { usage("invalid port number"); }
			break;

		case 'k':
			g_keylen = atoi(optarg);
			if(g_keylen < 8) { usage("invalid key size"); }
			break;

		case 'v':
			g_vallen  = atoi(optarg);
			if(g_vallen == 0) { usage("invalid value size"); }
			break;

		case 'm':
			g_num_multiplex  = atoi(optarg);
			break;

		case 't':
			g_num_thread  = atoi(optarg);
			break;

		case 'g':
			g_noset = true;
			break;

		case 's':
			g_noget = true;
			break;

		case 'h': /* FALL THROUGH */
		case '?': /* FALL THROUGH */
		default:
			usage(NULL);
		}
	}
	
	argc -= optind;

	if(argc != 1) { usage(NULL); }

	unsigned long multiplex_request = atoi(argv[optind]) / g_num_thread / g_num_multiplex;
	g_num_request = multiplex_request * g_num_multiplex;

	if(g_num_request == 0) { usage("invalid number of request"); }

	printf("number of threads    : %lu\n", g_num_thread);
	printf("number of requests   : %lu\n", g_num_thread * g_num_request);
	printf("requests per thread  : %lu\n", g_num_request);
	printf("request multiplex    : %lu\n", g_num_multiplex);
	printf("size of key          : %lu bytes\n", g_keylen);
	printf("size of value        : %lu bytes\n", g_vallen);
}

int main(int argc, char* argv[])
{
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	pthread_t* threads;

	parse_argv(argc, argv);	

#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif

	pthread_mutex_init(&g_count_lock, NULL);
	pthread_cond_init(&g_count_cond, NULL);
	pthread_mutex_init(&g_thread_lock, NULL);

	if(!g_noset) {
		printf("----\n[");
		threads = create_worker(worker_set);
		reset_timer();
		printf("] ...\n");
		start_worker();
		join_worker(threads);
		show_timer();
	}

	if(!g_noget) {
		printf("----\n[");
		threads = create_worker(worker_get);
		reset_timer();
		printf("] ...\n");
		start_worker();
		join_worker(threads);
		show_timer();
	}

	return 0;
}

