#include "zstd.h"

/* =========================================================================
 * MODULE: System Topology & Platform Timing
 * ========================================================================= */

double get_time_sec(void) {
    static int64_t freq = 0;
    LARGE_INTEGER li;
    if (freq == 0) {
        QueryPerformanceFrequency(&li);
        freq = li.QuadPart;
    }
    QueryPerformanceCounter(&li);
    return (double)li.QuadPart / (double)freq;
}

int detect_processor_count(void) {
    typedef DWORD (WINAPI *pfnGetActiveProcessorCount)(WORD);
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (hKernel) {
        pfnGetActiveProcessorCount pfn = (pfnGetActiveProcessorCount)GetProcAddress(hKernel, "GetActiveProcessorCount");
        if (pfn) {
            DWORD count = pfn(ALL_PROCESSOR_GROUPS);
            if (count > 0) return (int)count;
        }
    }
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;
}

WORD detect_processor_group_count(void) {
    typedef WORD (WINAPI *pfnGetActiveProcessorGroupCount)(void);
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (hKernel) {
        pfnGetActiveProcessorGroupCount pfn = (pfnGetActiveProcessorGroupCount)GetProcAddress(hKernel, "GetActiveProcessorGroupCount");
        if (pfn) {
            WORD count = pfn();
            if (count > 0) return count;
        }
    }
    return 1;
}

void assign_thread_to_group(HANDLE hThread, int worker_index, WORD group_count) {
    if (group_count <= 1) return;
    typedef BOOL (WINAPI *pfnSetThreadGroupAffinity)(HANDLE, const GROUP_AFFINITY*, PGROUP_AFFINITY);
    HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
    if (hKernel) {
        pfnSetThreadGroupAffinity pfn = (pfnSetThreadGroupAffinity)GetProcAddress(hKernel, "SetThreadGroupAffinity");
        if (pfn) {
            GROUP_AFFINITY ga;
            memset(&ga, 0, sizeof(ga));
            ga.Group = (WORD)(worker_index % group_count);
            ga.Mask = (KAFFINITY)(~0ULL);
            pfn(hThread, &ga, NULL);
        }
    }
}

/* =========================================================================
 * MODULE: ZstdThreadPool (Atomic Latch & Multi-Core Scaling)
 * ========================================================================= */

static DWORD WINAPI worker_thread_proc(LPVOID param) {
    WorkerThreadData *w = (WorkerThreadData *)param;
    while (1) {
        WaitForSingleObject(w->hStartEvent, INFINITE);
        if (w->terminate) break;

        if (w->src_sz > 0 && is_all_same_byte(w->src, w->src_sz)) {
            w->is_rle = true;
            w->comp_sz = 1;
        } else {
            w->is_rle = false;
            w->comp_sz = ZstdCodec_compress_block_core(w->ctx, w->src, w->src_sz,
                                                       w->dst, w->dst_cap,
                                                       w->max_chain, w->fast_mode);
        }

        if (InterlockedDecrement(&w->pool->remaining_workers) == 0) {
            SetEvent(w->pool->hAllDoneEvent);
        }
    }
    return 0;
}

ZstdThreadPool *ZstdThreadPool_Create(int threads, int max_chain, bool fast_mode) {
    if (threads < 1) threads = 1;
    WORD group_count = detect_processor_group_count();

    ZstdThreadPool *pool = (ZstdThreadPool *)calloc(1, sizeof(ZstdThreadPool));
    pool->num_workers = threads;
    pool->workers = (WorkerThreadData *)calloc(threads, sizeof(WorkerThreadData));
    pool->thread_handles = (HANDLE *)malloc(threads * sizeof(HANDLE));
    pool->hAllDoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    for (int i = 0; i < threads; i++) {
        pool->workers[i].max_chain = max_chain;
        pool->workers[i].fast_mode = fast_mode;
        pool->workers[i].pool = pool;
        pool->workers[i].ctx = (ZstdWorkerContext *)calloc(1, sizeof(ZstdWorkerContext));
        pool->workers[i].hStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        pool->workers[i].hThread = CreateThread(NULL, 0, worker_thread_proc, &pool->workers[i], 0, NULL);
        pool->thread_handles[i] = pool->workers[i].hThread;

        assign_thread_to_group(pool->workers[i].hThread, i, group_count);
    }
    return pool;
}

void ZstdThreadPool_Destroy(ZstdThreadPool *pool) {
    if (!pool) return;
    for (int i = 0; i < pool->num_workers; i++) {
        pool->workers[i].terminate = true;
        SetEvent(pool->workers[i].hStartEvent);
    }
    for (int i = 0; i < pool->num_workers; i++) {
        WaitForSingleObject(pool->thread_handles[i], INFINITE);
        CloseHandle(pool->workers[i].hThread);
        CloseHandle(pool->workers[i].hStartEvent);
        free(pool->workers[i].ctx);
    }
    CloseHandle(pool->hAllDoneEvent);
    free(pool->workers);
    free(pool->thread_handles);
    free(pool);
}
