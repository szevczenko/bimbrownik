#pragma once

#define _DARWIN_UNLIMITED_SELECT 1    // No limit on file descriptors

#if defined( __APPLE__ )
#include <mach/mach_time.h>
#endif

#define MG_ENABLE_POLL 0

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( MG_ENABLE_EPOLL ) && MG_ENABLE_EPOLL
#include <sys/epoll.h>
#elif defined( MG_ENABLE_POLL ) && MG_ENABLE_POLL
#include <poll.h>
#else
#include <sys/select.h>
#endif

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void* _alloca( size_t size );

#ifndef alloca
#define alloca( a ) _alloca( a )
#endif

#define fseeko( ... ) fseek( __VA_ARGS__ )

#ifndef MG_ENABLE_DIRLIST
#define MG_ENABLE_DIRLIST 1
#endif

#ifndef MG_PATH_MAX
#define MG_PATH_MAX FILENAME_MAX
#endif

#ifndef MG_ENABLE_POSIX_FS
#define MG_ENABLE_POSIX_FS 1
#endif

#ifndef MG_IO_SIZE
#define MG_IO_SIZE 16384
#endif
