#include "scm_handle.hpp"

ScmHandle::ScmHandle(SC_HANDLE h) : m_handle(h) {}

ScmHandle::~ScmHandle() {
        Close();
    }

ScmHandle::ScmHandle(ScmHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }

ScmHandle& ScmHandle::operator=(ScmHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

void ScmHandle::Close() {
        if (m_handle) {
            CloseServiceHandle(m_handle);
            m_handle = nullptr;
        }
    }

SC_HANDLE ScmHandle::Get() const { return m_handle; }

ScmHandle::operator bool() const { return m_handle != nullptr; }
