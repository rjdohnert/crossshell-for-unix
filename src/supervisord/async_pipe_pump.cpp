#include "async_pipe_pump.hpp"
#include "rotating_file_sink.hpp"

AsyncPipePump& AsyncPipePump::Instance() {
        static AsyncPipePump pump;
        return pump;
    }

bool AsyncPipePump::RegisterPipe(HANDLE sourceReadHandle, const std::wstring& path, size_t maxBytes, int backups) {
        if (sourceReadHandle == NULL || sourceReadHandle == INVALID_HANDLE_VALUE) return false;

        HANDLE asyncReadHandle = INVALID_HANDLE_VALUE;
        if (!DuplicateHandle(GetCurrentProcess(), sourceReadHandle, GetCurrentProcess(), &asyncReadHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            return false;
        }

        if (CreateIoCompletionPort(asyncReadHandle, iocp_, 0, 0) == NULL) {
            CloseHandle(asyncReadHandle);
            return false;
        }

        auto ctx = std::make_shared<PipeContext>();
        ctx->sourceKey = reinterpret_cast<uintptr_t>(sourceReadHandle);
        ctx->readHandle = asyncReadHandle;
        ctx->sink = std::make_unique<RotatingFileSink>(path, maxBytes, backups);

        {
            std::lock_guard<std::mutex> lock(mtx_);
            bySource_[ctx->sourceKey] = ctx;
            byOverlapped_[&ctx->ov] = ctx;
        }

        if (!IssueRead(ctx)) {
            RetireContext(ctx);
            return false;
        }
        return true;
    }

void AsyncPipePump::UnregisterPipe(HANDLE sourceReadHandle) {
        if (sourceReadHandle == NULL || sourceReadHandle == INVALID_HANDLE_VALUE) return;
        std::shared_ptr<PipeContext> ctx;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            auto it = bySource_.find(reinterpret_cast<uintptr_t>(sourceReadHandle));
            if (it == bySource_.end()) return;
            ctx = it->second;
        }

        ctx->stopping = true;
        CancelIoEx(ctx->readHandle, &ctx->ov);
        PostQueuedCompletionStatus(iocp_, 0, 0, &ctx->ov);

        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [&]() { return ctx->retired.load(); });
    }

AsyncPipePump::AsyncPipePump() {
        iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        size_t hw = static_cast<size_t>(std::max(1u, std::thread::hardware_concurrency()));
        size_t workerCount = std::max<size_t>(2, std::min<size_t>(8, hw));
        workers_.reserve(workerCount);
        for (size_t i = 0; i < workerCount; ++i) {
            workers_.emplace_back([this]() { WorkerLoop(); });
        }
    }

AsyncPipePump::~AsyncPipePump() {
        if (iocp_) {
            for (size_t i = 0; i < workers_.size(); ++i) {
                PostQueuedCompletionStatus(iocp_, 0, 1, NULL);
            }
            for (auto& t : workers_) {
                if (t.joinable()) t.join();
            }
            CloseHandle(iocp_);
            iocp_ = NULL;
        }
    }

bool AsyncPipePump::IssueRead(const std::shared_ptr<PipeContext>& ctx) {
        if (ctx->stopping.load()) return false;
        ZeroMemory(&ctx->ov, sizeof(ctx->ov));
        BOOL ok = ReadFile(ctx->readHandle, ctx->buffer.data(), static_cast<DWORD>(ctx->buffer.size()), NULL, &ctx->ov);
        if (ok) return true;
        DWORD err = GetLastError();
        return err == ERROR_IO_PENDING;
    }

void AsyncPipePump::WorkerLoop() {
        while (true) {
            DWORD bytes = 0;
            ULONG_PTR key = 0;
            LPOVERLAPPED ov = NULL;
            BOOL ok = GetQueuedCompletionStatus(iocp_, &bytes, &key, &ov, INFINITE);

            if (ov == NULL) {
                if (key == 1) break;
                continue;
            }

            std::shared_ptr<PipeContext> ctx;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                auto it = byOverlapped_.find(ov);
                if (it == byOverlapped_.end()) continue;
                ctx = it->second;
            }

            if (ctx->stopping.load()) {
                RetireContext(ctx);
                continue;
            }

            if (!ok) {
                DWORD err = GetLastError();
                if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED || err == ERROR_OPERATION_ABORTED) {
                    RetireContext(ctx);
                    continue;
                }
                RetireContext(ctx);
                continue;
            }

            if (bytes > 0) {
                ctx->sink->Write(ctx->buffer.data(), bytes);
            }

            if (!IssueRead(ctx)) {
                RetireContext(ctx);
            }
        }
    }

void AsyncPipePump::RetireContext(const std::shared_ptr<PipeContext>& ctx) {
        if (ctx->retired.exchange(true)) return;

        {
            std::lock_guard<std::mutex> lock(mtx_);
            byOverlapped_.erase(&ctx->ov);
            bySource_.erase(ctx->sourceKey);
        }

        if (ctx->readHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(ctx->readHandle);
            ctx->readHandle = INVALID_HANDLE_VALUE;
        }

        cv_.notify_all();
    }
