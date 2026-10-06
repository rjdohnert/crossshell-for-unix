/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * ninja - small build system closest in spirit to Make
 */

#ifndef NINJA_HPP
#define NINJA_HPP

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <deque>
#include <array>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace EnterpriseNinja {

inline std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], size_needed);
    return wstrTo;
}

inline std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

class ChainedArena {
public:
    static constexpr size_t CHUNK_SIZE = 64 * 1024 * 1024; // 64 MB Chunks

    ChainedArena() {
        AllocateNewChunk();
    }

    ~ChainedArena() {
        for (char* chunk : chunks_) {
            ::free(chunk);
        }
    }

    template <typename T, typename... Args>
    T* Alloc(Args&&... args) {
        size_t align = alignof(T);
        size_t current_addr = reinterpret_cast<size_t>(chunks_.back() + current_offset_);
        size_t padding = (align - (current_addr % align)) % align;

        if (current_offset_ + padding + sizeof(T) > CHUNK_SIZE) {
            AllocateNewChunk();
            current_addr = reinterpret_cast<size_t>(chunks_.back());
            padding = (align - (current_addr % align)) % align;
        }

        current_offset_ += padding;
        void* ptr = chunks_.back() + current_offset_;
        current_offset_ += sizeof(T);
        return new (ptr) T(std::forward<Args>(args)...);
    }

    char* DuplicateString(std::string_view sv) {
        if (current_offset_ + sv.size() + 1 > CHUNK_SIZE) {
            AllocateNewChunk();
        }
        char* dest = chunks_.back() + current_offset_;
        std::memcpy(dest, sv.data(), sv.size());
        dest[sv.size()] = '\0';
        current_offset_ += sv.size() + 1;
        return dest;
    }

private:
    std::vector<char*> chunks_;
    size_t current_offset_ = 0;

    void AllocateNewChunk() {
        char* chunk = static_cast<char*>(::malloc(CHUNK_SIZE));
        if (!chunk) {
            std::cerr << "ninja: fatal out-of-memory allocating 64MB arena chunk\n";
            std::exit(12);
        }
        chunks_.push_back(chunk);
        current_offset_ = 0;
    }
};

struct Edge;

struct Node {
    std::string_view path;
    uint64_t mtime = 0;
    bool exists = false;
    bool dirty = false;
    Edge* in_edge = nullptr;
    std::vector<Edge*> out_edges;

    explicit Node(std::string_view p) : path(p) {}
};

struct Rule {
    std::string_view name;
    std::string_view command;
    std::string_view description;
};

struct Edge {
    Rule rule;
    std::vector<Node*> inputs;
    std::vector<Node*> outputs;
    std::atomic<uint32_t> pending_inputs{0};

    std::string ExpandCommand() const {
        std::string cmd(rule.command);
        std::string in_str;
        for (size_t i = 0; i < inputs.size(); ++i) {
            if (i > 0) in_str += " ";
            in_str.append(inputs[i]->path.data(), inputs[i]->path.size());
        }
        std::string out_str;
        for (size_t i = 0; i < outputs.size(); ++i) {
            if (i > 0) out_str += " ";
            out_str.append(outputs[i]->path.data(), outputs[i]->path.size());
        }

        auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
            size_t start_pos = 0;
            while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
        };

        replace_all(cmd, "$in", in_str);
        replace_all(cmd, "$out", out_str);
        return cmd;
    }
};

} // namespace EnterpriseNinja

#endif // NINJA_HPP
