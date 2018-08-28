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
#include <stdlib.h>
#include <errno.h>

static char basepath[FILENAME_LEN] = {0};
static int debug_mode = 0;
static qSocket conn;

#define DEBUG_DETAILED_STR    "detailed"
#define DEBUG_INTERACTIVE_STR "interactive"

#define DEBUG_DETAILED    1
#define DEBUG_INTERACTIVE 2

int debug_console(void);
int process_args(int argc, char** argv);
void* serve_filesystem(char* eachop);

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
            return 1;
        }else if(fullstrcmp(command.str, "EINVAL")){
            return EINVAL;
        }else if(fullstrcmp(command.str, "EMFILE")){
            return EMFILE;
        }else if(fullstrcmp(command.str, "EBUSY")){
            return EBUSY;
        }
        printf("Unrecognized input, please try again.\n");
    }
}

void* serve_filesystem(char* eachop){
    struct OpHdr* ophead = (struct OpHdr*)(void*)eachop;
    if(ophead->opid == OPER_OPEN){

    }else if(ophead->opid == OPER_READ){

    }else if(ophead->opid == OPER_WRITE){

    }else if(ophead->opid == OPER_CLOSE){

    }
    return NULL;
}

int process_args(int argc, char** argv){
    if(argc < 2){
        // insufficient args, enter help mode.
        printf("Usage: %s <path> [detailed|interactive]\n", argv[0]);
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
    // there are two debug modes: detailed and interactive
    // interactive is more strict than detailed.
    if(argc > 2){
        if(fullstrcmp(argv[2], DEBUG_INTERACTIVE_STR)){
            debug_mode = DEBUG_INTERACTIVE;
        }else if(fullstrcmp(argv[2], DEBUG_DETAILED_STR)){
            debug_mode = DEBUG_DETAILED;
        }
    }
    return 0;
}
