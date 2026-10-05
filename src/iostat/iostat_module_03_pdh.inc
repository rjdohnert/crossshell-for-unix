// ============================================================================
// 3. PDH COUNTER ENGINE
// ============================================================================

class PdhCounterEngine {
private:
    PDH_HQUERY m_hQuery = NULL;
    PDH_HCOUNTER m_hCpuUser = NULL;
    PDH_HCOUNTER m_hCpuSys = NULL;
    PDH_HCOUNTER m_hCpuIdle = NULL;
    PDH_HCOUNTER m_hReadsSec = NULL;
    PDH_HCOUNTER m_hWritesSec = NULL;
    PDH_HCOUNTER m_hReadBytesSec = NULL;
    PDH_HCOUNTER m_hWriteBytesSec = NULL;
    PDH_HCOUNTER m_hPctDiskTime = NULL;
    PDH_HCOUNTER m_hQueueLength = NULL;

    static PDH_HCOUNTER AddCounter(PDH_HQUERY hQuery, const std::wstring& path) {
        PDH_HCOUNTER hCounter = NULL;
        PDH_STATUS status = PdhAddEnglishCounterW(hQuery, path.c_str(), 0, &hCounter);
        if (status != ERROR_SUCCESS) {
            std::wcerr << L"Error: Failed to register counter: " << path 
                       << L" (Status code: 0x" << std::hex << status << L")\n";
        }
        return hCounter;
    }

    static void FetchDiskCounterArray(PDH_HCOUNTER hCounter, std::function<void(const std::wstring&, double)> assign_fn) {
        DWORD dwBufferSize = 0;
        DWORD dwItemCount = 0;
        PDH_STATUS status = PdhGetFormattedCounterArrayW(hCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, NULL);
        
        if (status == PDH_MORE_DATA) {
            std::vector<BYTE> buffer(dwBufferSize);
            PDH_FMT_COUNTERVALUE_ITEM_W* pItems = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
            status = PdhGetFormattedCounterArrayW(hCounter, PDH_FMT_DOUBLE, &dwBufferSize, &dwItemCount, pItems);
            
            if (status == ERROR_SUCCESS) {
                for (DWORD i = 0; i < dwItemCount; ++i) {
                    if (pItems[i].szName) {
                        assign_fn(pItems[i].szName, pItems[i].FmtValue.doubleValue);
                    }
                }
            }
        }
    }

public:
    PdhCounterEngine() = default;

    ~PdhCounterEngine() {
        Close();
    }

    bool Initialize() {
        if (PdhOpenQueryW(NULL, 0, &m_hQuery) != ERROR_SUCCESS) {
            std::cerr << "Fatal Error: Failed to initialize PDH Query engine.\n";
            return false;
        }

        m_hCpuUser = AddCounter(m_hQuery, L"\\Processor(_Total)\\% User Time");
        m_hCpuSys = AddCounter(m_hQuery, L"\\Processor(_Total)\\% Privileged Time");
        m_hCpuIdle = AddCounter(m_hQuery, L"\\Processor(_Total)\\% Idle Time");

        m_hReadsSec = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\Disk Reads/sec");
        m_hWritesSec = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\Disk Writes/sec");
        m_hReadBytesSec = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\Disk Read Bytes/sec");
        m_hWriteBytesSec = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\Disk Write Bytes/sec");
        m_hPctDiskTime = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\% Disk Time");
        m_hQueueLength = AddCounter(m_hQuery, L"\\PhysicalDisk(*)\\Current Disk Queue Length");

        if (!m_hCpuUser || !m_hCpuSys || !m_hCpuIdle || !m_hReadsSec || !m_hWritesSec || 
            !m_hReadBytesSec || !m_hWriteBytesSec || !m_hPctDiskTime || !m_hQueueLength) {
            std::cerr << "Fatal Error: One or more critical performance counters could not be resolved.\n";
            Close();
            return false;
        }

        PdhCollectQueryData(m_hQuery);
        return true;
    }

    bool CollectSample(double& userPct, double& sysPct, double& idlePct, std::map<std::wstring, DiskStats>& diskMap) {
        PDH_STATUS status = PdhCollectQueryData(m_hQuery);
        if (status != ERROR_SUCCESS) {
            std::cerr << "Error collecting query data. Status code: 0x" << std::hex << status << "\n";
            return false;
        }

        userPct = 0.0;
        sysPct = 0.0;
        idlePct = 0.0;
        PDH_FMT_COUNTERVALUE cvUser{}, cvSys{}, cvIdle{};

        if (PdhGetFormattedCounterValue(m_hCpuUser, PDH_FMT_DOUBLE, NULL, &cvUser) == ERROR_SUCCESS) {
            userPct = cvUser.doubleValue;
        }
        if (PdhGetFormattedCounterValue(m_hCpuSys, PDH_FMT_DOUBLE, NULL, &cvSys) == ERROR_SUCCESS) {
            sysPct = cvSys.doubleValue;
        }
        if (PdhGetFormattedCounterValue(m_hCpuIdle, PDH_FMT_DOUBLE, NULL, &cvIdle) == ERROR_SUCCESS) {
            idlePct = cvIdle.doubleValue;
        }

        diskMap.clear();
        FetchDiskCounterArray(m_hReadsSec, [&](const std::wstring& name, double val) { diskMap[name].reads_sec = val; });
        FetchDiskCounterArray(m_hWritesSec, [&](const std::wstring& name, double val) { diskMap[name].writes_sec = val; });
        FetchDiskCounterArray(m_hReadBytesSec, [&](const std::wstring& name, double val) { diskMap[name].read_bytes_sec = val; });
        FetchDiskCounterArray(m_hWriteBytesSec, [&](const std::wstring& name, double val) { diskMap[name].write_bytes_sec = val; });
        FetchDiskCounterArray(m_hPctDiskTime, [&](const std::wstring& name, double val) { diskMap[name].pct_disk_time = val; });
        FetchDiskCounterArray(m_hQueueLength, [&](const std::wstring& name, double val) { diskMap[name].queue_length = val; });

        return true;
    }

    void Close() {
        if (m_hQuery) {
            PdhCloseQuery(m_hQuery);
            m_hQuery = NULL;
        }
    }
};

