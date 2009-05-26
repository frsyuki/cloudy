#!/bin/sh
gcc -g -Wall memrain.c -I. cloudy/multi.c cloudy/core.c cloudy/cbtable.c cloudy/memcache.c cloudy/stream.c -lpthread

