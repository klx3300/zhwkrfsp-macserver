#include "../bss.h"
#include "../error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

qBinarySafeString qbss_constructor(){
    qBinarySafeString bss;
    bss.str = malloc(64);
    if(bss.str == NULL){
        SETERR(ZHWK_ERR_MM_ALLOC_FAIL);
        return bss;
    }
    bss.size = 0;
    bss.capacity = 64;
    memset(bss.str,0,64);
    return bss;
}

qBinarySafeString qbss__from_array(void* arr,unsigned int size){
    qBinarySafeString bss = qbss_constructor();
    if(chkerr()==0){
        qbss_append(bss,arr,size);
    }
    return bss;
}

void q__bss_append(qBinarySafeString* bss,const char* str,unsigned int len){
    // check capacity
    if(bss->capacity<2*(bss->size+len)){
        // realloc
        char* tmp = malloc(2*(bss->size + len));
        if(tmp == NULL){
            SETERR(ZHWK_ERR_MM_ALLOC_FAIL);
            return;
        }
        memset(tmp,0,2*(bss->size + len));
        memcpy(tmp,bss->str,bss->size);
        bss->capacity = 2*(bss->size + len);
        free(bss->str);
        bss->str = tmp;
    }
    // copy to new dest
    memcpy(bss->str+(bss->size),str,len);
    bss->size += len;
}

void qbss_destructor(qBinarySafeString bss){
    free(bss.str);
}
