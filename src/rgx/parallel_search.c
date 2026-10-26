#include "parallel_search.h"
#include "allocation.h"
#include "pattern_set.h"
#include "file_search.h"
#include "search_state.h"
#include <process.h>

struct ParallelSearch {
    const RgxOptions* options;RgxPrinter* printer;bool multiple_targets;
    CRITICAL_SECTION lock;CONDITION_VARIABLE ready,space;
    char* queue[64];size_t head,count,queue_bytes,worker_count;
    HANDLE workers[16];bool stopping,found,error;
};

static unsigned __stdcall search_worker(void* argument) {
    ParallelSearch* pool=(ParallelSearch*)argument;
    IMatcher* matcher=PatternSet_New(pool->options,pool->options->effective_ignore_case);
    for(;;) {
        EnterCriticalSection(&pool->lock);
        while(!pool->count && !pool->stopping)SleepConditionVariableCS(&pool->ready,&pool->lock,INFINITE);
        if(!pool->count && pool->stopping){LeaveCriticalSection(&pool->lock);break;}
        char* path=pool->queue[pool->head];pool->head=(pool->head+1)%64;--pool->count;pool->queue_bytes-=strlen(path)+1;
        WakeConditionVariable(&pool->space);LeaveCriticalSection(&pool->lock);
        rgx_had_error=rgx_match_error=rgx_found=false;
        FILE* output=NULL;
        RgxPrinter* printer=RgxPrinter_New(pool->options);
        if(!matcher || tmpfile_s(&output)!=0 || !output){fprintf(stderr,"rgx: worker output allocation failed\n");rgx_had_error=true;}
        else {
            printer->stream=output;
            SearchSingleFile(path,pool->options,matcher,printer,pool->multiple_targets);
            if(fflush(output)!=0 || ferror(output) || _ftelli64(output)>64LL*1024*1024){fprintf(stderr,"rgx: worker output exceeds 64 MiB or failed\n");rgx_had_error=true;}
        }
        EnterCriticalSection(&pool->lock);
        pool->found=pool->found || rgx_found;pool->error=pool->error || rgx_had_error;
        printer_merge(pool->printer,printer);
        if(output && !rgx_had_error) {
            rewind(output);char buffer[65536];size_t amount;
            while((amount=fread(buffer,1,sizeof(buffer),output))!=0)
                if(fwrite(buffer,1,amount,pool->printer->stream)!=amount){pool->error=true;break;}
            if(ferror(output))pool->error=true;
        }
        LeaveCriticalSection(&pool->lock);
        if(output)fclose(output);RgxPrinter_Free(printer);free(path);
    }
    if(matcher)matcher->vptr->Destroy(matcher);
    return 0;
}

ParallelSearch* parallel_search_new(const RgxOptions* options,RgxPrinter* printer,bool multiple_targets,size_t workers) {
    ParallelSearch* pool=(ParallelSearch*)rgx_calloc(1,sizeof(ParallelSearch));
    pool->options=options;pool->printer=printer;pool->multiple_targets=multiple_targets;
    InitializeCriticalSection(&pool->lock);InitializeConditionVariable(&pool->ready);InitializeConditionVariable(&pool->space);
    for(size_t index=0;index<workers;++index) {
        HANDLE thread=(HANDLE)_beginthreadex(NULL,0,search_worker,pool,0,NULL);
        if(!thread){fprintf(stderr,"rgx: could not start search worker\n");rgx_had_error=true;break;}
        pool->workers[pool->worker_count++]=thread;
    }
    if(!pool->worker_count){DeleteCriticalSection(&pool->lock);free(pool);return NULL;}
    return pool;
}

bool parallel_search_submit(ParallelSearch* pool,const char* path) {
    size_t cost=strlen(path)+1;
    if(cost>16U*1024U*1024U){rgx_had_error=true;return false;}
    EnterCriticalSection(&pool->lock);
    while(pool->count==64 || cost>16U*1024U*1024U-pool->queue_bytes)SleepConditionVariableCS(&pool->space,&pool->lock,INFINITE);
    size_t slot=(pool->head+pool->count)%64;pool->queue[slot]=rgx_strdup(path);++pool->count;pool->queue_bytes+=cost;
    WakeConditionVariable(&pool->ready);LeaveCriticalSection(&pool->lock);return true;
}

void parallel_search_finish(ParallelSearch* pool) {
    if(!pool)return;
    EnterCriticalSection(&pool->lock);pool->stopping=true;WakeAllConditionVariable(&pool->ready);LeaveCriticalSection(&pool->lock);
    WaitForMultipleObjects((DWORD)pool->worker_count,pool->workers,TRUE,INFINITE);
    for(size_t index=0;index<pool->worker_count;++index)CloseHandle(pool->workers[index]);
    rgx_found=rgx_found || pool->found;rgx_had_error=rgx_had_error || pool->error;
    DeleteCriticalSection(&pool->lock);free(pool);
}