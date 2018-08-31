#include <stdio.h>
#include <string.h>
#include "netwrap.h"
#include "prot.h"
#include "ustruct.h"
#include "fuse_client.h"
#include "zhwkre/bss.h"
#include "zhwkre/network.h"
#include "zhwkre/concurrent.h"
#include "zhwkre/log.h"
#include "zhwkre/utils.h"
#include "zhwkre/concurrent.h"
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/mount.h>

typedef char *PtrChar;
static PtrChar READ_MODES[] = {
    "Regular",
    "Stat",
    "Statfs",
    "Link",
    "Directory"
};

static char basepath[FILENAME_LEN] = {0};
static char destaddr[40] = {0};
static int debug_mode = 0;
static qSocket conn;


#define DEBUG_DETAILED_STR    "detailed"
#define DEBUG_INTERACTIVE_STR "interactive"

#define DEBUG_DETAILED    1
#define DEBUG_INTERACTIVE 2

int debug_console(void);
void preprocess_path(char* srcfn);
// this function will write to socket directly, using the *conn* variable.
int bad_reply(uint32_t ouid, int32_t rtvl);
int good_reply(uint32_t ouid, int32_t rtvl, void* rephdr, size_t hdrsize, void* payload, size_t plsize);
int process_args(int argc, char** argv);
void* serve_filesystem(char* eachop);
void* serve_wrapper(void* recvbss);

#define PRINTERRNO(lvl) qLog##lvl##fmt("%s()[%s:%d] reported error: %s\n", __func__, __FILE__, __LINE__, strerror(errno))
#define DEBUGINFO(fmt, ...) do{if(debug_mode >= DEBUG_DETAILED) qLogDebugfmt(fmt, __VA_ARGS__);}while(0)
#define DEBUGCONS(fmt, ...) ({DEBUGINFO(fmt, __VA_ARGS__); debug_console();})
#define DEBUGCONS_BEGIN(varname, fmt, ...) do{ int varname = DEBUGCONS(fmt, __VA_ARGS__); if(varname != 0){
#define DEBUGCONS_END }}while(0);

int main(int argc, char** argv){
    // process arguments
    /* int rtvl = 0; */
    /* if((rtvl = DEBUGCONS("%s", "Program Staring -->"))) return rtvl; */
    DEBUGCONS_BEGIN(rtvl, "%s", "Program Starting -->")
        return rtvl;
    DEBUGCONS_END
    if(process_args(argc, argv)) return 1;
    conn.domain = qIPv4;
    conn.type = qStreamSocket;
    conn.protocol = qDefaultProto;
    if(qSocket_open(conn)){
        PRINTERRNO(Fail);
        return 1;
    }
    DEBUGINFO("Connecting ==> %s", destaddr);
    if(qStreamSocket_connect(conn, destaddr)){
        PRINTERRNO(Fail);
        return 1;
    }
    DEBUGINFO("Connected <=> %s", destaddr);
    // read things off
    while(1){
        qBinarySafeString *recvbss = malloc(sizeof(qBinarySafeString));
        *recvbss = qbss_constructor();
        if(sockread(conn, recvbss, sizeof(struct OpHdr))){
            PRINTERRNO(Fail);
            return 1;
        }
        if(sockread(conn, recvbss, (int)(((struct OpHdr*)(void*)(recvbss->str))->size - sizeof(struct OpHdr)))){
            PRINTERRNO(Fail);
            return 1;
        }
        // done, pass to..
        if(qRun(serve_wrapper, recvbss)){
            PRINTERRNO(Fail);
            return 1;
        }
    }
}

int debug_console(){
    if(debug_mode < DEBUG_INTERACTIVE) return 0;
    // perform debug console
    while(1){
        printf("DebugConsole > ");
        qBinarySafeString command = readline(stdin);
        if(fullstrcmp(command.str, "help") || fullstrcmp(command.str, "h")){
            printf("h, help: show this message.\n");
            printf("a, abort: forcefully terminate program.\n");
            printf("c, continue: continue the execution.\n");
            printf("ENORMAL, EINVAL, EMFILE, EBUSY: generate specified error.\n");
        }else if(fullstrcmp(command.str, "abort") || fullstrcmp(command.str, "a")){
            qLogFail("Abort from debug console. Aborting..");
            abort();
        }else if(fullstrcmp(command.str, "continue") || fullstrcmp(command.str, "c")){
            return 0;
        }else if(fullstrcmp(command.str, "ENORMAL")){
            return -1;
        }else if(fullstrcmp(command.str, "EINVAL")){
            return -EINVAL;
        }else if(fullstrcmp(command.str, "EMFILE")){
            return -EMFILE;
        }else if(fullstrcmp(command.str, "EBUSY")){
            return -EBUSY;
        }
        printf("Unrecognized input, please try again.\n");
    }
}

int bad_reply(uint32_t ouid, int32_t rtvl){
    struct RpHdr reply;
    reply.size = sizeof(struct RpHdr);
    reply.ouid = ouid;
    reply.rtvl = rtvl;
    qBinarySafeString buf = qbss_constructor();
    q__bss_append(&buf, (void*)&reply, sizeof(struct RpHdr));
    return sockwrite(conn, &buf, 1);
}

int good_reply(uint32_t ouid, int32_t rtvl, void* rephdr, size_t hdrsize, void* payload, size_t plsize){
    struct RpHdr reply;
    reply.size = sizeof(struct RpHdr);
    reply.ouid = ouid;
    reply.rtvl = rtvl;
    qBinarySafeString buf = qbss_constructor();
    if(rephdr != NULL){
        reply.size += hdrsize;
    }
    if(payload != NULL){
        reply.size += plsize;
    }
    q__bss_append(&buf, (void*)&reply, sizeof(struct RpHdr));
    if(rephdr != NULL){
        q__bss_append(&buf, rephdr, (uint32_t)hdrsize);
    }
    if(payload != NULL){
        q__bss_append(&buf, payload, (uint32_t)plsize);
    }
    return sockwrite(conn, &buf, 1);
}

void* serve_filesystem(char* eachop){
    struct OpHdr* ophead = (struct OpHdr*)(void*)eachop;
    if(ophead->opid == OPER_OPEN){
        struct OpOpen* openinfo = (struct OpOpen*)(void*)(eachop + sizeof(struct OpHdr));
        // do the path preprocess
        preprocess_path(openinfo->filename);
        // print debug message
        DEBUGCONS_BEGIN(rtvl, "Open[%s, %s, %s, %s, %s, %s] <- %s",
                openinfo->flags & OPEN_CREAT ? "CREAT" : "X",
                openinfo->flags & OPEN_DIRECT ? "DIRECT" : "X",
                openinfo->flags & OPEN_DIRECTORY ? "DIR" : "X",
                openinfo->flags & OPEN_NOFOLLOW ? "NOFOLLOW" : "X",
                openinfo->flags & OPEN_TRUNC ? "TRUNC" : "X",
                openinfo->flags & OPEN_MKDIR ? "MKDIR" : "X",
                openinfo->filename)
            if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
            return NULL;
        DEBUGCONS_END
        // process the open operation
        // we need to dispatch it to different functions!
        // direct, nofollow and truc can pass to the open();
        if(!(openinfo->flags & (OPEN_MKDIR |  OPEN_DIRECTORY | OPEN_CREAT))){
            int fd = do_open(openinfo->filename, O_RDWR | 
                    (openinfo->flags & OPEN_NOFOLLOW ? O_NOFOLLOW : 0) |
                    (openinfo->flags & OPEN_TRUNC ? O_TRUNC : 0));
            if(fd < 0){
                // error case
                int reply_result = bad_reply(ophead->ouid, -errno);
                if(reply_result) PRINTERRNO(Warn);
                return NULL;
            }
            // then there's correct fd
            struct RpOpen oresult;
            oresult.file_handle = (uint64_t)fd;
            oresult.ignore_above = 0;
            // do return
            if(good_reply(ophead->ouid, 0, &oresult, sizeof(struct RpOpen), NULL, 0)) PRINTERRNO(Warn);
            return NULL;
        }
        // then this is the mkdir/directory case
        if(openinfo->flags & OPEN_DIRECTORY){
            int fd = do_opendir(openinfo->filename);
            if(fd < 0){
                // error case
                int reply_result = bad_reply(ophead->ouid, -errno);
                if(reply_result) PRINTERRNO(Warn);
                return NULL;
            }
            // then there's correct fd
            struct RpOpen oresult;
            oresult.file_handle = (uint64_t)fd;
            oresult.ignore_above = 0;
            // do return
            if(good_reply(ophead->ouid, 0, &oresult, sizeof(struct RpOpen), NULL, 0)) PRINTERRNO(Warn);
            return NULL;
        }
        if(openinfo->flags & OPEN_MKDIR){
            int result = do_mkdir(openinfo->filename, openinfo->mode);
            if(result < 0){
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            if(good_reply(ophead->ouid, 0, NULL, 0, NULL, 0)) PRINTERRNO(Warn);
            return NULL;
        }
        if(openinfo->flags & OPEN_CREAT){
            int result = do_creat(openinfo->filename, openinfo->mode);
            if(result < 0){
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            struct RpOpen oresult;
            oresult.file_handle = (uint64_t)result;
            oresult.ignore_above = 0;
            if(good_reply(ophead->ouid, 0, &oresult, sizeof(struct RpOpen), NULL, 0)) PRINTERRNO(Warn);
            return NULL;
            
        }
        // there's no possible that the control flow reaches here.
        // so we fall into debug console here..
        qLogFailfmt("%s(): fatal in control flow design: is there protocol version mismatch?", __func__);
        DEBUGCONS_BEGIN( rtvl, "%s", "Control flow reached unexpected position")
            if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
            return NULL;
        DEBUGCONS_END
        if(bad_reply(ophead->ouid, -EINVAL)) PRINTERRNO(Warn);
        return NULL;
    }else if(ophead->opid == OPER_READ){
        struct OpRead* rdinfo = (struct OpRead*)(void*)(eachop + sizeof(struct OpHdr));
        preprocess_path(rdinfo->filename);
        DEBUGCONS_BEGIN(rtvl, "Read::%s <- [%llu]%s @ %llu <- +%llu",
                READ_MODES[rdinfo->read_mode], rdinfo->file_handle, rdinfo->filename,
                rdinfo->offset, rdinfo->count)
            if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
            return NULL;
        DEBUGCONS_END
        // different types of read operations..
        if(rdinfo->read_mode == RD_REGULAR){
            // create a buffer..
            char* readoff = malloc(rdinfo->count);
            int readed = do_read((int)rdinfo->file_handle, readoff, rdinfo->count, (off_t)rdinfo->offset);
            if(readed < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                free(readoff);
                return NULL;
            }
            // then this is a successful read, no matter how much is succeeded in reading..
            if(good_reply(ophead->ouid, readed, NULL, 0, readoff, (size_t)readed)) PRINTERRNO(Warn);
            free(readoff);
            return NULL;
        }else if(rdinfo->read_mode == RD_STAT){
            // use the stat!
            struct stat filestat;
            int result = do_getattr(rdinfo->filename, &filestat);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            struct Ustat unistat;
            unify_stat(&unistat, filestat);
            if(good_reply(ophead->ouid, 0, NULL, 0, &unistat, sizeof(struct Ustat))) PRINTERRNO(Warn);
            return NULL;
        }else if(rdinfo->read_mode == RD_STATFS){
            struct statfs fsstat;
            int result = do_statfs(rdinfo->filename, &fsstat);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            struct Ustatvfs unistat;
            unify_statvfs(&unistat, fsstat);
            if(good_reply(ophead->ouid, 0, NULL, 0, &unistat, sizeof(struct Ustatvfs))) PRINTERRNO(Warn);
            return NULL;
        }else if(rdinfo->read_mode == RD_DIR){
            qBinarySafeString dircont = qbss_constructor();
            int result = do_readdir((int)rdinfo->file_handle, &dircont);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                qbss_destructor(dircont);
                return NULL;
            }
            qBinarySafeString unified = qbss_constructor();
            unify_dirent_arr(&unified, dircont, 1);
            // and write it out..
            if(good_reply(ophead->ouid, 0, NULL, 0, unified.str, unified.size)) PRINTERRNO(Warn);
            qbss_destructor(unified);
            return NULL;
        }else if(rdinfo->read_mode == RD_LINK){
            char destfn[FILENAME_LEN] = {0};
            int result = do_readlink(rdinfo->filename, destfn, FILENAME_LEN);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            if(good_reply(ophead->ouid, 0, NULL, 0, destfn, FILENAME_LEN)) PRINTERRNO(Warn);
            return NULL;
        }
        // control flow should not reach here..
        qLogFailfmt("%s(): fatal in control flow design: is there protocol version mismatch?", __func__);
        DEBUGCONS_BEGIN( rtvl, "%s", "Control flow reached unexpected position")
            if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
            return NULL;
        DEBUGCONS_END
        if(bad_reply(ophead->ouid, -EINVAL)) PRINTERRNO(Warn);
        return NULL;
    }else if(ophead->opid == OPER_WRITE){
        struct OpWrit* wrinfo = (struct OpWrit*)(void*)(eachop + sizeof(struct OpHdr));
        preprocess_path(wrinfo->filename);
        char* contptr = eachop + sizeof(struct OpHdr) + sizeof(struct OpWrit);
        // there are distinct modes..
        if(wrinfo->write_mode == WR_REGULAR){
            DEBUGCONS_BEGIN(rtvl, "Writ::Regular -> +%llu -> %s[%llu] @ %llu",
                     wrinfo->offset, wrinfo->filename, wrinfo->file_handle, wrinfo->count)
                if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
                return NULL;
            DEBUGCONS_END;
            int result = do_write((int)wrinfo->file_handle, contptr, wrinfo->count, (off_t)wrinfo->offset);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            if(good_reply(ophead->ouid, result, NULL, 0, NULL, 0)) PRINTERRNO(Warn);
            return NULL;
        }else if(wrinfo->write_mode == WR_SYMLINK){
            // do the patch!
            preprocess_path(contptr);
            DEBUGCONS_BEGIN(rtvl, "Writ::Symlink %s -> %s", wrinfo->filename, contptr)
                if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
                return NULL;
            DEBUGCONS_END;
            int result = do_symlink(wrinfo->filename, contptr);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            if(good_reply(ophead->ouid, result, NULL, 0, NULL, 0)) PRINTERRNO(Warn);
            return NULL;
        }else if(wrinfo->write_mode == WR_REMOVE){
            DEBUGCONS_BEGIN(rtvl, "Writ::Remove * %s *", wrinfo->filename)
                if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
                return NULL;
            DEBUGCONS_END;
            int result = do_unlink(wrinfo->filename);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            if(good_reply(ophead->ouid, result, NULL, 0, NULL, 0)) PRINTERRNO(Warn);
            return NULL;
        }else if(wrinfo->write_mode == WR_TRUNC){
            DEBUGCONS_BEGIN(rtvl, "Writ::Trunc %s||[%llu]", wrinfo->filename, wrinfo->offset)
                if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
                return NULL;
            DEBUGCONS_END;
            int result = do_truncate(wrinfo->filename, (off_t)wrinfo->offset);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            if(good_reply(ophead->ouid, result, NULL, 0, NULL, 0)) PRINTERRNO(Warn);
            return NULL;
        }else if(wrinfo->write_mode == WR_CHMOD){
            char destmode[12] = {0};
            strmode(*(uint16_t*)(void*)contptr, destmode);
            DEBUGCONS_BEGIN(rtvl, "Writ::Chmod %s <- %s", wrinfo->filename, destmode)
                if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
                return NULL;
            DEBUGCONS_END;
            int result = do_chmod(wrinfo->filename, *(uint16_t*)(void*)contptr);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            if(good_reply(ophead->ouid, result, NULL, 0, NULL, 0)) PRINTERRNO(Warn);
            return NULL;
        }else if(wrinfo->write_mode == WR_ACCES){
            char destmode[12] = {0};
            strmode(*(uint16_t*)(void*)contptr, destmode);
            DEBUGCONS_BEGIN(rtvl, "Writ::Access %s <- %s ?", wrinfo->filename, destmode)
                if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
                return NULL;
            DEBUGCONS_END;
            int result = do_access(wrinfo->filename, *(uint16_t*)(void*)contptr);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            if(good_reply(ophead->ouid, result, NULL, 0, NULL, 0)) PRINTERRNO(Warn);
            return NULL;
        }else if(wrinfo->write_mode == WR_RMDIR){
            DEBUGCONS_BEGIN(rtvl, "Writ::Rmdir * %s *", wrinfo->filename)
                if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
                return NULL;
            DEBUGCONS_END;
            int result = do_rmdir(wrinfo->filename);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            if(good_reply(ophead->ouid, result, NULL, 0, NULL, 0)) PRINTERRNO(Warn);
            return NULL;
        }
        // control flow should not reach here..
        qLogFailfmt("%s(): fatal in control flow design: is there protocol version mismatch?", __func__);
        DEBUGCONS_BEGIN( rtvl, "%s", "Control flow reached unexpected position")
            if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
            return NULL;
        DEBUGCONS_END
        if(bad_reply(ophead->ouid, -EINVAL)) PRINTERRNO(Warn);
        return NULL;
    }else if(ophead->opid == OPER_CLOSE){
        struct OpClos* clinfo = (struct OpClos*)(void*)(eachop + sizeof(struct OpHdr));
        DEBUGCONS_BEGIN(rtvl, "Close::%s x %llu x", clinfo->is_dir ? "AsDirectory" : "Asfile", clinfo->file_handle)
            if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
            return NULL;
        DEBUGCONS_END;
        if(clinfo->is_dir){
            int result = do_releasedir((int)clinfo->file_handle);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            if(good_reply(ophead->ouid, 0, NULL, 0, 0, 0)) PRINTERRNO(Warn);
            return NULL;
        }else{
            int result = do_release((int)clinfo->file_handle);
            if(result < 0){
                PRINTERRNO(Warn);
                if(bad_reply(ophead->ouid, -errno)) PRINTERRNO(Warn);
                return NULL;
            }
            if(good_reply(ophead->ouid, 0, NULL, 0, 0, 0)) PRINTERRNO(Warn);
            return NULL;
        }
    }
    // illegal operation!
    qLogFailfmt("%s(): fatal in control flow design: is there protocol version mismatch?", __func__);
    DEBUGCONS_BEGIN( rtvl, "%s", "Control flow reached unexpected position")
        if(bad_reply(ophead->ouid, rtvl)) PRINTERRNO(Warn);
        return NULL;
    DEBUGCONS_END
    if(bad_reply(ophead->ouid, -EINVAL)) PRINTERRNO(Warn);
    return NULL;
}

int process_args(int argc, char** argv){
    if(argc < 3){
        // insufficient args, enter help mode.
        printf("Usage: %s <path> <destaddr:port> [detailed|interactive]\n", argv[0]);
        return 1;
    }
    // check if accessible
    if(access(argv[1], R_OK | W_OK | X_OK)){
        // cannot access?
        PRINTERRNO(Fail);
        qLogInfo("Please check the permission of given folder: It should be rwx for this user.");
        return -1;
    }
    // so it is accessible.
    strcpy(basepath, argv[1]);
    // copy the address
    strcpy(destaddr, argv[2]);
    // there are two debug modes: detailed and interactive
    // interactive is more strict than detailed.
    if(argc > 3){
        if(fullstrcmp(argv[3], DEBUG_INTERACTIVE_STR)){
            debug_mode = DEBUG_INTERACTIVE;
        }else if(fullstrcmp(argv[3], DEBUG_DETAILED_STR)){
            debug_mode = DEBUG_DETAILED;
        }
    }
    return 0;
}

void preprocess_path(char* fn){
    char buffer[FILENAME_LEN] = {0};
    strcpy(buffer, basepath);
    strcat(buffer, fn);
    DEBUGINFO("Concatenated path is %s", buffer);
    memcpy(fn, buffer, FILENAME_LEN);
}

void* serve_wrapper(void* rbss){
    qBinarySafeString* recv_bss = rbss;
    serve_filesystem(recv_bss->str);
    qbss_destructor(*recv_bss);
    free(rbss);
    return NULL;
}
