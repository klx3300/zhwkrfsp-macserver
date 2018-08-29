#include "fuse_client.h"
#include <fcntl.h>
#include <stdlib.h>
#include <mach/error.h>
#include <errno.h>

int do_getattr(const char* fn, struct stat* st){
    return stat(fn, st);
}

int do_readlink(const char* fn, char* dest, size_t bufsz){
    return (int)readlink(fn, dest, bufsz);
}

int do_mkdir(const char* fn, mode_t mode){
    return mkdir(fn, mode);
}

int do_rmdir(const char* fn){
    return rmdir(fn);
}

int do_symlink(const char* from, const char* to){
    return symlink(from, to);
}

int do_chmod(const char* fn, mode_t mode){
    return chmod(fn, mode);
}

int do_truncate(const char* fn, off_t pos){
    return truncate(fn, pos);
}

int do_open(const char* fn, int oflags){
    return open(fn, oflags);
}

int do_read(int fd, char* buf, size_t bufsz, off_t pos){
    return (int)pread(fd, buf, bufsz, pos);
}

int do_write(int fd, const char* cont, size_t contsz, off_t pos){
    return (int)pwrite(fd, cont, contsz, pos);
}

int do_statfs(const char* path, struct statfs* fstat){
    return statfs(path, fstat);
}

int do_release(int fd){
    return close(fd);
}

// for directories, there are special handling procedure:
// since opendir() doesn't return a file descriptor, I
// have to mimic this mechanism to satisfy the protocol.

typedef DIR *dirptr;

static dirptr dirdescs[MAX_DIR_DESC] = {0};
static int last_seek = 0;

int do_opendir(const char* fn){
    int tail = last_seek;
    while((dirdescs[last_seek] == NULL) && (last_seek < MAX_DIR_DESC)) last_seek++;
    if(last_seek >= MAX_DIR_DESC){
        // find again
        last_seek = 0;
        while((dirdescs[last_seek] == NULL) && (last_seek < tail)) last_seek++;
        if(last_seek >= tail){
            // fd list is full.
            errno = EMFILE;
            return -1;
        }
    }
    dirdescs[last_seek] = opendir(fn);
    if(dirdescs[last_seek] == NULL) return -1;
    return last_seek;
}

int do_readdir(int fd, qBinarySafeString* result){
    if(dirdescs[fd] == NULL){
        errno = EINVAL;
        return -1;
    }
    // or else..
    for(struct dirent* diriter = readdir(dirdescs[fd]); diriter != NULL; diriter = readdir(dirdescs[fd])){
        // do the append to bss
        q__bss_append(result, (char*)diriter, sizeof(struct dirent));
    }
    // rewind back
    rewinddir(dirdescs[fd]);
    return 0;
}

int do_releasedir(int fd){
    if(dirdescs[fd] == NULL){
        errno = EINVAL;
        return -1;
    }
    if(closedir(dirdescs[fd])){
        return -1;
    }
    dirdescs[fd] = NULL;
    return 0;
}

int do_access(const char* path, int mode){
    return access(path, mode);
}

int do_creat(const char* path, mode_t mode){
    return creat(path, mode);
}

int do_unlink(const char* path){
    return unlink(path);
}

