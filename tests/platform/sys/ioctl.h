/* Test-only POSIX shim; no actual device I/O is possible in host tests. */
#ifndef HPC_TEST_IOCTL_H
#define HPC_TEST_IOCTL_H
#include <fcntl.h>
#include <unistd.h>
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#define _IOWR(type, nr, data) ((3u << 30) | ((unsigned)sizeof(data) << 16) | ((type) << 8) | (nr))
#define _IOW(type, nr, data) ((1u << 30) | ((unsigned)sizeof(data) << 16) | ((type) << 8) | (nr))
int hpc_test_open(const char *path, int flags, ...);
int hpc_test_close(int fd);
int hpc_test_ioctl(int fd, unsigned long request, ...);
#define open hpc_test_open
#define close hpc_test_close
#define ioctl hpc_test_ioctl
#endif
