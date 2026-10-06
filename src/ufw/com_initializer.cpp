#include "com_initializer.hpp"

ComInitializer::ComInitializer() {
        m_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(m_hr)) {
            m_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        }
    }

ComInitializer::~ComInitializer() {
        if (SUCCEEDED(m_hr) && m_hr != S_FALSE) {
            CoUninitialize();
        }
    }

bool ComInitializer::Succeeded() const { return true; }
