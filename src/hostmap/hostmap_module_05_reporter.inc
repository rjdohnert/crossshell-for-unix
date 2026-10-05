// ============================================================================
// 5. OUTPUT REPORTER
// ============================================================================

class HostmapReporter {
public:
    static void PrintSectionHeader(const std::string& title) {
        std::cout << "\n[" << title << "]\n" << std::string(60, '-') << "\n";
    }

    static void PrintKeyValue(const std::string& key, const std::string& value) {
        std::cout << "  " << std::left << std::setw(24) << key << ": " << value << "\n";
    }

    static void RenderText(const HostmapOptions& opts) {
        if (opts.showSys) {
            HostSystemInfo sys = HostTelemetryCollector::CollectSystemInfo();
            PrintSectionHeader("HOST & SYSTEM IDENTITY");
            PrintKeyValue("NetBIOS Name", sys.computerName);
            PrintKeyValue("DNS Hostname", sys.dnsHostname);
            PrintKeyValue("DNS Domain", sys.dnsDomain);
            PrintKeyValue("Operating System", sys.osName + " (Edition " + sys.osVersion + ", Build " + sys.osBuild + ")");
            PrintKeyValue("System Boot Time", sys.bootTime);
            PrintKeyValue("System Uptime", sys.uptime);
        }

        if (opts.showCpu) {
            HostCpuInfo cpu = HostTelemetryCollector::CollectCpuInfo();
            PrintSectionHeader("PROCESSOR & KERNEL TOPOLOGY");
            PrintKeyValue("Processor Model", cpu.processorModel);
            PrintKeyValue("Architecture", cpu.architecture);
            PrintKeyValue("Physical Sockets/Cores", std::to_string(cpu.physicalCores));
            PrintKeyValue("Logical Processors", std::to_string(cpu.logicalProcessors));
            PrintKeyValue("Page Size", std::to_string(cpu.pageSizeBytes) + " bytes");
        }

        if (opts.showMem) {
            HostMemoryInfo mem = HostTelemetryCollector::CollectMemoryInfo();
            PrintSectionHeader("MEMORY & PAGING UTILIZATION");
            PrintKeyValue("Physical RAM Total", FormatUtils::FormatBytes(mem.totalPhys));
            PrintKeyValue("Physical RAM Used", FormatUtils::FormatBytes(mem.usedPhys) + " (" + std::to_string(mem.memoryLoad) + "%)");
            PrintKeyValue("Physical RAM Free", FormatUtils::FormatBytes(mem.availPhys));
            PrintKeyValue("Commit / Paging Total", FormatUtils::FormatBytes(mem.totalVirtual));
            PrintKeyValue("Commit / Paging Free", FormatUtils::FormatBytes(mem.availVirtual));
        }

        if (opts.showNet) {
            auto adapters = HostTelemetryCollector::CollectNetworkInfo();
            PrintSectionHeader("NETWORK INTERFACES & ADDRESS MAP");
            for (const auto& adp : adapters) {
                std::cout << "  * " << adp.description << " [" << adp.status << "]\n";
                std::cout << "      MAC Address : " << adp.macAddress << "\n";
                for (const auto& ip4 : adp.ipv4Addresses) {
                    std::cout << "      IPv4 Address: " << ip4 << "\n";
                }
                for (const auto& ip6 : adp.ipv6Addresses) {
                    std::cout << "      IPv6 Address: " << ip6 << "\n";
                }
                for (const auto& gw : adp.gateways) {
                    std::cout << "      Gateway     : " << gw << "\n";
                }
                for (const auto& dns : adp.dnsServers) {
                    std::cout << "      DNS Server  : " << dns << "\n";
                }
                std::cout << "\n";
            }
        }
    }

    static void RenderJson() {
        HostSystemInfo sys = HostTelemetryCollector::CollectSystemInfo();
        HostCpuInfo cpu = HostTelemetryCollector::CollectCpuInfo();
        HostMemoryInfo mem = HostTelemetryCollector::CollectMemoryInfo();
        auto adapters = HostTelemetryCollector::CollectNetworkInfo();

        std::cout << "{\n"
                  << "  \"system\": {\n"
                  << "    \"netbios_name\": \"" << sys.computerName << "\",\n"
                  << "    \"dns_hostname\": \"" << sys.dnsHostname << "\",\n"
                  << "    \"dns_domain\": \"" << sys.dnsDomain << "\",\n"
                  << "    \"os_name\": \"" << sys.osName << "\",\n"
                  << "    \"os_version\": \"" << sys.osVersion << "\",\n"
                  << "    \"os_build\": \"" << sys.osBuild << "\",\n"
                  << "    \"uptime\": \"" << sys.uptime << "\",\n"
                  << "    \"boot_time\": \"" << sys.bootTime << "\"\n"
                  << "  },\n"
                  << "  \"processor\": {\n"
                  << "    \"model\": \"" << cpu.processorModel << "\",\n"
                  << "    \"architecture\": \"" << cpu.architecture << "\",\n"
                  << "    \"physical_cores\": " << cpu.physicalCores << ",\n"
                  << "    \"logical_processors\": " << cpu.logicalProcessors << ",\n"
                  << "    \"page_size_bytes\": " << cpu.pageSizeBytes << "\n"
                  << "  },\n"
                  << "  \"memory\": {\n"
                  << "    \"total_bytes\": " << mem.totalPhys << ",\n"
                  << "    \"available_bytes\": " << mem.availPhys << ",\n"
                  << "    \"used_bytes\": " << mem.usedPhys << ",\n"
                  << "    \"load_percentage\": " << mem.memoryLoad << ",\n"
                  << "    \"virtual_total_bytes\": " << mem.totalVirtual << ",\n"
                  << "    \"virtual_avail_bytes\": " << mem.availVirtual << "\n"
                  << "  },\n"
                  << "  \"adapters\": [\n";

        for (size_t i = 0; i < adapters.size(); ++i) {
            const auto& adp = adapters[i];
            std::cout << "    {\n"
                      << "      \"description\": \"" << adp.description << "\",\n"
                      << "      \"status\": \"" << adp.status << "\",\n"
                      << "      \"mac_address\": \"" << adp.macAddress << "\",\n"
                      << "      \"ipv4\": [";
            for (size_t j = 0; j < adp.ipv4Addresses.size(); ++j) {
                std::cout << "\"" << adp.ipv4Addresses[j] << "\"" << (j + 1 < adp.ipv4Addresses.size() ? ", " : "");
            }
            std::cout << "],\n      \"ipv6\": [";
            for (size_t j = 0; j < adp.ipv6Addresses.size(); ++j) {
                std::cout << "\"" << adp.ipv6Addresses[j] << "\"" << (j + 1 < adp.ipv6Addresses.size() ? ", " : "");
            }
            std::cout << "]\n    }" << (i + 1 < adapters.size() ? ",\n" : "\n");
        }
        std::cout << "  ]\n}\n";
    }
};

