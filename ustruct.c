#include "ustruct.h"
#include <stdlib.h>
#include <string.h>

#define FAKE_FILENAME_MAX 4096

void unify_stat(struct Ustat* ustat, struct stat st){
    ustat->st_dev = (uint64_t)st.st_dev;
    ustat->st_ino = st.st_ino;
    ustat->st_mode = st.st_mode;
    ustat->st_nlink = st.st_nlink;
    ustat->st_uid = st.st_uid;
    ustat->st_gid = st.st_gid;
    ustat->st_rdev = (uint64_t)st.st_rdev;
    ustat->st_size = (uint64_t)st.st_size;
    ustat->st_atim = st.st_atimespec.tv_sec;
    ustat->st_atimensec = (uint64_t)st.st_atimespec.tv_nsec;
    ustat->st_mtim = st.st_mtimespec.tv_sec;
    ustat->st_mtimensec = (uint64_t)st.st_mtimespec.tv_nsec;
    ustat->st_ctim = st.st_ctimespec.tv_sec;
    ustat->st_ctimensec = (uint64_t)st.st_ctimespec.tv_nsec;
}

void fall_stat(struct stat* st, struct Ustat ustat){
    st->st_dev = (int)ustat.st_dev;
    st->st_ino = ustat.st_ino;
    st->st_mode = (uint16_t)ustat.st_mode;
    st->st_nlink = (uint16_t)ustat.st_nlink;
    st->st_uid = ustat.st_uid;
    st->st_gid = ustat.st_gid;
    st->st_rdev = (int)ustat.st_rdev;
    st->st_size = (int64_t)ustat.st_size;
    st->st_atimespec.tv_sec = ustat.st_atim;
    st->st_atimespec.tv_nsec = (long)ustat.st_atimensec;
    st->st_mtimespec.tv_sec = ustat.st_mtim;
    st->st_mtimespec.tv_nsec = (long)ustat.st_mtimensec;
    st->st_ctimespec.tv_sec = ustat.st_ctim;
    st->st_ctimespec.tv_nsec = (long)ustat.st_ctimensec;
}

void unify_statvfs(struct Ustatvfs* uvfs, struct statfs vfs){
    uvfs->f_bsize = vfs.f_bsize;
    uvfs->f_blocks = vfs.f_blocks;
    uvfs->f_bfree = vfs.f_bfree;
    uvfs->f_bavail = vfs.f_bavail;
    uvfs->f_files = vfs.f_files;
    uvfs->f_ffree = vfs.f_ffree;
    uvfs->f_fsid = (((uint64_t)vfs.f_fsid.val[0]) << 32) + (uint64_t)vfs.f_fsid.val[1];
    uvfs->f_flag = vfs.f_flags;
    uvfs->f_namemax = FAKE_FILENAME_MAX;
}

void fall_statvfs(struct statfs* vfs, struct Ustatvfs uvfs){
    vfs->f_bsize = (uint32_t) uvfs.f_bsize;
    vfs->f_blocks = uvfs.f_blocks;
    vfs->f_bfree = uvfs.f_bfree;
    vfs->f_bavail = uvfs.f_bavail;
    vfs->f_files = uvfs.f_files;
    vfs->f_ffree = uvfs.f_ffree;
    vfs->f_fsid = *(struct fsid*)&uvfs.f_fsid;
    vfs->f_flags = (uint32_t)uvfs.f_flag;
}

void unify_dirent(struct Udirent* udir, struct dirent dir){
    udir->d_ino = dir.d_ino;
    udir->d_off = dir.d_seekoff;
    udir->d_reclen = dir.d_reclen;
    udir->d_type = dir.d_type;
    memcpy(udir->d_name, dir.d_name, 256);
}

void fall_dirent(struct dirent* dir, struct Udirent udir){
    dir->d_ino = udir.d_ino;
    dir->d_seekoff = udir.d_off;
    dir->d_reclen = udir.d_reclen;
    dir->d_type = udir.d_type;
    memcpy(dir->d_name, udir.d_name, 256);
}

void unify_dirent_arr(qBinarySafeString* dest, qBinarySafeString src, int auto_free){
    struct dirent* srciter = (struct dirent*)(void*)src.str;
    struct dirent* srcend = (struct dirent*)(void*)(src.str + src.size);
    for(; srciter < srcend; srciter++){
        struct Udirent ud;
        // fill 
        unify_dirent(&ud, *srciter);
        q__bss_append(dest, (char*)&ud, sizeof(struct Udirent));
    }
    if(auto_free) qbss_destructor(src);
}
