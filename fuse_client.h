#ifndef Q_ZHWKRFSP_OSXCLIENT_H
#define Q_ZHWKRFSP_OSXCLIENT_H

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <utime.h>
#include <dirent.h>
#include <unistd.h>
#include "zhwkre/bss.h"

#define MAX_DIR_DESC 65536

int do_getattr(const char* fn, struct stat* st);
int do_readlink(const char* fn, char* dest, size_t bufsz);
int do_mkdir(const char* fn, mode_t mode);
int do_rmdir(const char* fn);
int do_symlink(const char* from, const char* to);
int do_chmod(const char* fn, mode_t mode);
int do_truncate(const char* fn, off_t pos);
int do_open(const char* fn, int oflags);
int do_read(int fd, char* buf, size_t bufsz, off_t pos);
int do_write(int fd, const char* cont, size_t contsz, off_t pos);
int do_statfs(const char* path, struct statfs* fstat);
int do_release(int fd);
int do_opendir(const char* fn);
int do_readdir(int fd, qBinarySafeString* result);
int do_releasedir(int fd);
int do_access(const char* path, int mode);
int do_creat(const char* path, mode_t mode);
int do_unlink(const char* path);
int do_utimens(const char* path, struct timespec times[2]);

#endif
