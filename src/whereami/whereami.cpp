/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <locationapi.h>
#include <winhttp.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <optional>

#pragma comment(lib, "Locationapi.lib")
#pragma comment(lib, "Winhttp.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Ole32.lib")

// ============================================================================
// Domain Models
// ============================================================================

struct Coordinates {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    double accuracy = 0.0;
    bool has_altitude = false;
    bool has_accuracy = false;

    std::string to_raw() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << latitude << "," << longitude;
        return oss.str();
    }

    std::string to_sexagesimal() const {
        auto format_dms = [](double deg, bool is_lat) {
            char dir = is_lat ? (deg >= 0 ? 'N' : 'S') : (deg >= 0 ? 'E' : 'W');
            double abs_deg = std::abs(deg);
            int d = static_cast<int>(abs_deg);
            double rem_m = (abs_deg - d) * 60.0;
            int m = static_cast<int>(rem_m);
            double s = (rem_m - m) * 60.0;

            std::ostringstream oss;
            oss << d << "\xC2\xB0 " << m << "' " << std::fixed << std::setprecision(2) << s << "\" " << dir;
            return oss.str();
        };

        return format_dms(latitude, true) + ", " + format_dms(longitude, false);
    }
};

struct LocationReport {
    Coordinates coords;
    std::string country;
    std::string country_code;
    std::string region;
    std::string region_name;
    std::string city;
    std::string postal_code;
    std::string timezone;
    std::string isp;
    std::string public_ip;
    std::string local_ip;
    std::string hostname;
    std::string provider_name;
    bool success = false;
    std::string error_message;
};

// ============================================================================
// CLI Options & Parser
// ============================================================================

enum class OutputFormat {
    Raw,          // Default: 41.386905,2.144257
    Json,         // Machine readable JSON object
    Sexagesimal,  // DMS: 41° 23' 12.86" N, 2° 8' 39.32" E
    Human,        // Formatted human-readable multi-line report
    Address       // Civic street/city address only
};

struct CliOptions {
    OutputFormat format = OutputFormat::Raw;
    bool show_accuracy = false;
    bool show_altitude = false;
    bool show_address = false;
    bool show_network = false;
    bool verbose = false;
    bool quiet = false;
    int timeout_seconds = 5;
    std::string from_provider = "auto"; // "auto", "sensor", "network"
    std::string basedir = "";
    std::string statedir = "";
    bool show_help = false;
    bool show_version = false;
};

// Minimal self-contained JSON extractor
class JsonMiniParser {
public:
    static std::string get_string(const std::string& json, const std::string& key) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return "";

        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";

        pos = json.find_first_not_of(" \t\r\n", pos + 1);
        if (pos == std::string::npos || json[pos] != '"') return "";

        size_t end_quote = json.find('"', pos + 1);
        if (end_quote == std::string::npos) return "";

        return json.substr(pos + 1, end_quote - pos - 1);
    }

    static double get_double(const std::string& json, const std::string& key, double default_val = 0.0) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return default_val;

        pos = json.find(':', pos);
        if (pos == std::string::npos) return default_val;

        pos = json.find_first_not_of(" \t\r\n", pos + 1);
        if (pos == std::string::npos) return default_val;

        size_t end_val = json.find_first_of(",}\r\n ", pos);
        std::string num_str = json.substr(pos, end_val - pos);
        try {
            return std::stod(num_str);
        } catch (...) {
            return default_val;
        }
    }
};

// ============================================================================
// Location Provider Strategy Pattern
// ============================================================================

class ILocationProvider {
public:
    virtual ~ILocationProvider() = default;
    virtual std::string get_name() const = 0;
    virtual bool query(LocationReport& out_report, int timeout_seconds, bool verbose) = 0;
};

class WindowsSensorLocationProvider : public ILocationProvider {
public:
    std::string get_name() const override { return "Windows Sensor / Location API"; }

    bool query(LocationReport& out_report, int /*timeout_seconds*/, bool verbose) override {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        bool co_initialized = SUCCEEDED(hr);

        ILocation* pLocation = nullptr;
        hr = CoCreateInstance(CLSID_Location, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pLocation));
        if (FAILED(hr) || !pLocation) {
            if (verbose) std::cerr << "[whereami:sensor] Windows Location API not available on this device.\n";
            if (co_initialized) CoUninitialize();
            return false;
        }

        IID reportTypes[] = { IID_ILatLongReport };
        pLocation->RequestPermissions(NULL, reportTypes, 1, FALSE);

        ILocationReport* pReport = nullptr;
        hr = pLocation->GetReport(IID_ILatLongReport, &pReport);
        if (FAILED(hr) || !pReport) {
            if (verbose) std::cerr << "[whereami:sensor] No active GPS/Wi-Fi sensor lock.\n";
            pLocation->Release();
            if (co_initialized) CoUninitialize();
            return false;
        }

        ILatLongReport* pLatLong = nullptr;
        hr = pReport->QueryInterface(IID_PPV_ARGS(&pLatLong));
        if (SUCCEEDED(hr) && pLatLong) {
            DOUBLE lat = 0, lon = 0, err = 0, alt = 0;
            pLatLong->GetLatitude(&lat);
            pLatLong->GetLongitude(&lon);
            pLatLong->GetErrorRadius(&err);

            out_report.coords.latitude = lat;
            out_report.coords.longitude = lon;
            out_report.coords.accuracy = err;
            out_report.coords.has_accuracy = (err > 0.0);

            if (SUCCEEDED(pLatLong->GetAltitude(&alt))) {
                out_report.coords.altitude = alt;
                out_report.coords.has_altitude = true;
            }

            out_report.provider_name = get_name();
            out_report.success = true;
            pLatLong->Release();
        }

        pReport->Release();
        pLocation->Release();
        if (co_initialized) CoUninitialize();
        return out_report.success;
    }
};

class NetworkGeoLocationProvider : public ILocationProvider {
public:
    std::string get_name() const override { return "Network / IP Geolocation Service"; }

    bool query(LocationReport& out_report, int timeout_seconds, bool verbose) override {
        if (verbose) std::cerr << "[whereami:network] Interrogating network geolocation infrastructure...\n";

        HINTERNET hSession = WinHttpOpen(L"whereami-bsd-clone/2.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        WinHttpSetTimeouts(hSession, timeout_seconds * 1000, timeout_seconds * 1000,
                           timeout_seconds * 1000, timeout_seconds * 1000);

        HINTERNET hConnect = WinHttpConnect(hSession, L"ip-api.com", INTERNET_DEFAULT_HTTP_PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
            L"/json/?fields=status,message,country,countryCode,region,regionName,city,zip,lat,lon,timezone,isp,query",
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

        if (bResults) {
            bResults = WinHttpReceiveResponse(hRequest, NULL);
        }

        std::string response;
        if (bResults) {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;
            do {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                if (dwSize == 0) break;

                std::vector<char> buffer(dwSize + 1, 0);
                if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                    response.append(buffer.data(), dwDownloaded);
                }
            } while (dwSize > 0);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (response.empty()) {
            if (verbose) std::cerr << "[whereami:network] No response from network location provider.\n";
            return false;
        }

        std::string status = JsonMiniParser::get_string(response, "status");
        if (status != "success") {
            out_report.error_message = JsonMiniParser::get_string(response, "message");
            return false;
        }

        out_report.coords.latitude = JsonMiniParser::get_double(response, "lat");
        out_report.coords.longitude = JsonMiniParser::get_double(response, "lon");
        out_report.coords.accuracy = 1000.0; // Typical IP geo accuracy: ~1km
        out_report.coords.has_accuracy = true;
        out_report.coords.has_altitude = false;

        out_report.country = JsonMiniParser::get_string(response, "country");
        out_report.country_code = JsonMiniParser::get_string(response, "countryCode");
        out_report.region = JsonMiniParser::get_string(response, "region");
        out_report.region_name = JsonMiniParser::get_string(response, "regionName");
        out_report.city = JsonMiniParser::get_string(response, "city");
        out_report.postal_code = JsonMiniParser::get_string(response, "zip");
        out_report.timezone = JsonMiniParser::get_string(response, "timezone");
        out_report.isp = JsonMiniParser::get_string(response, "isp");
        out_report.public_ip = JsonMiniParser::get_string(response, "query");

        out_report.provider_name = get_name();
        out_report.success = true;
        return true;
    }
};

class SystemContextCollector {
public:
    static void populate(LocationReport& report) {
        char host[256] = {0};
        if (gethostname(host, sizeof(host)) == 0) {
            report.hostname = host;
        }

        // Retrieve primary local IP address
        ULONG outBufLen = 15000;
        std::vector<BYTE> buf(outBufLen);
        PIP_ADAPTER_ADDRESSES pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());

        if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, pAddresses, &outBufLen) == NO_ERROR) {
            for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr != NULL; pCurr = pCurr->Next) {
                if (pCurr->OperStatus == IfOperStatusUp && pCurr->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
                    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurr->FirstUnicastAddress;
                    if (pUnicast) {
                        sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(pUnicast->Address.lpSockaddr);
                        char ip[INET_ADDRSTRLEN] = {0};
                        inet_ntop(AF_INET, &(sa_in->sin_addr), ip, INET_ADDRSTRLEN);
                        report.local_ip = ip;
                        break;
                    }
                }
            }
        }
    }
};

// ============================================================================
// Output Formatters (Strategy Pattern)
// ============================================================================

class IOutputFormatter {
public:
    virtual ~IOutputFormatter() = default;
    virtual void output(const LocationReport& report, const CliOptions& options) = 0;
};

class RawFormatter : public IOutputFormatter {
public:
    void output(const LocationReport& report, const CliOptions& options) override {
        std::cout << report.coords.to_raw();
        if (options.show_altitude && report.coords.has_altitude) {
            std::cout << "," << std::fixed << std::setprecision(1) << report.coords.altitude;
        }
        if (options.show_accuracy && report.coords.has_accuracy) {
            std::cout << " (+/-" << std::fixed << std::setprecision(0) << report.coords.accuracy << "m)";
        }
        std::cout << "\n";
    }
};

class SexagesimalFormatter : public IOutputFormatter {
public:
    void output(const LocationReport& report, const CliOptions& options) override {
        std::cout << report.coords.to_sexagesimal();
        if (options.show_altitude && report.coords.has_altitude) {
            std::cout << " (Alt: " << std::fixed << std::setprecision(1) << report.coords.altitude << "m)";
        }
        if (options.show_accuracy && report.coords.has_accuracy) {
            std::cout << " [Acc: \xC2\xB1" << std::fixed << std::setprecision(0) << report.coords.accuracy << "m]";
        }
        std::cout << "\n";
    }
};

class JsonFormatter : public IOutputFormatter {
public:
    void output(const LocationReport& report, const CliOptions& options) override {
        std::cout << "{\n";
        std::cout << "  \"latitude\": " << std::fixed << std::setprecision(6) << report.coords.latitude << ",\n";
        std::cout << "  \"longitude\": " << std::fixed << std::setprecision(6) << report.coords.longitude;

        if (report.coords.has_altitude || options.show_altitude) {
            std::cout << ",\n  \"altitude\": " << std::fixed << std::setprecision(1) << report.coords.altitude;
        }
        if (report.coords.has_accuracy || options.show_accuracy) {
            std::cout << ",\n  \"accuracy\": " << std::fixed << std::setprecision(1) << report.coords.accuracy;
        }

        if (options.show_address || !report.city.empty()) {
            std::cout << ",\n  \"city\": \"" << report.city << "\"";
            std::cout << ",\n  \"region\": \"" << report.region_name << "\"";
            std::cout << ",\n  \"country\": \"" << report.country << "\"";
            std::cout << ",\n  \"postal_code\": \"" << report.postal_code << "\"";
        }

        if (options.show_network || !report.public_ip.empty()) {
            std::cout << ",\n  \"public_ip\": \"" << report.public_ip << "\"";
            std::cout << ",\n  \"local_ip\": \"" << report.local_ip << "\"";
            std::cout << ",\n  \"isp\": \"" << report.isp << "\"";
            std::cout << ",\n  \"hostname\": \"" << report.hostname << "\"";
        }

        std::cout << ",\n  \"provider\": \"" << report.provider_name << "\"\n";
        std::cout << "}\n";
    }
};

class AddressFormatter : public IOutputFormatter {
public:
    void output(const LocationReport& report, const CliOptions& /*options*/) override {
        if (!report.city.empty()) {
            std::cout << report.city << ", " << report.region_name;
            if (!report.postal_code.empty()) std::cout << " " << report.postal_code;
            std::cout << ", " << report.country << "\n";
        } else {
            std::cout << report.coords.to_raw() << "\n";
        }
    }
};

class HumanFormatter : public IOutputFormatter {
public:
    void output(const LocationReport& report, const CliOptions& /*options*/) override {
        std::cout << "  Coordinates:  " << report.coords.to_raw()
                  << "  (" << report.coords.to_sexagesimal() << ")\n";

        if (report.coords.has_accuracy) {
            std::cout << "  Accuracy:     \xC2\xB1" << std::fixed << std::setprecision(0)
                      << report.coords.accuracy << " meters\n";
        }
        if (report.coords.has_altitude) {
            std::cout << "  Altitude:     " << std::fixed << std::setprecision(1)
                      << report.coords.altitude << " meters\n";
        }
        if (!report.city.empty()) {
            std::cout << "  Address:      " << report.city << ", "
                      << report.region_name << " " << report.postal_code << ", "
                      << report.country << " (" << report.country_code << ")\n";
        }
        if (!report.timezone.empty()) {
            std::cout << "  Timezone:     " << report.timezone << "\n";
        }
        if (!report.public_ip.empty() || !report.local_ip.empty()) {
            std::cout << "  Network:      Local: " << (report.local_ip.empty() ? "N/A" : report.local_ip)
                      << " | External: " << (report.public_ip.empty() ? "N/A" : report.public_ip)
                      << " (" << report.isp << ")\n";
        }
        if (!report.hostname.empty()) {
            std::cout << "  Hostname:     " << report.hostname << "\n";
        }
        std::cout << "  Provider:     " << report.provider_name << "\n";
    }
};

// ============================================================================
// BSD Manual Page Help Section
// ============================================================================

const char* const BSD_MANUAL =
R"(NAME
     whereami -- display current geographical coordinates and location

SYNOPSIS
     whereami [-aAdhjnqsvV] [-f format] [-t seconds] [--from provider]
              [--basedir directory] [--statedir directory]

DESCRIPTION
     The whereami utility interrogates available location sensors and network
     interfaces to determine the physical and geographical coordinates of the
     host. It is the location counterpart to whoami(1) and pwd(1).

OPTIONS
     -f format, --format=format
             Specify the output representation. Valid options:
             raw          Decimal degrees: <latitude>,<longitude> (Default)
             sexagesimal  Degrees, minutes, and seconds (DMS) format
             json         Standard JSON formatted object
             human        Multi-line detailed human-readable summary
             address      Civic geographic address (City, Region, Country)

     -j, --json
             Shorthand for --format=json.

     -s, --sexagesimal
             Shorthand for --format=sexagesimal.

     -a, --accuracy
             Display estimated horizontal accuracy radius in meters.

     -A, --altitude
             Display altitude above sea level (if supported by sensor).

     -d, --address
             Shorthand for --format=address.

     -n, --network
             Include network telemetry (Public IP, Local IP, ISP, Hostname).

     -t seconds, --timeout=seconds
             Set the provider response deadline in seconds (Default: 5).

     --from=provider
             Force a specific location discovery provider:
             auto     Probe local sensor first, fall back to network (Default)
             sensor   Query Windows Location Sensor API exclusively
             network  Query network-based positioning exclusively

     -v, --verbose, --debug
             Print diagnostic operational details during discovery.

     -q, --quiet
             Suppress all diagnostics and warnings; output only the location.

     --basedir=directory
             Specify base directory for configuration files (BSD compat).

     --statedir=directory
             Specify state directory for location records (BSD compat).

     -h, --help
             Display this manual documentation and exit.

     -V, --version
             Display version information and exit.

EXIT STATUS
     0       Location successfully acquired and displayed.
     1       Location could not be determined or permission was denied.
     2       Invalid command-line arguments or configuration error.

EXAMPLES
     Display location in standard decimal coordinates:
           $ whereami
           40.712776,-74.005974

     Display coordinates in sexagesimal DMS notation with accuracy:
           $ whereami -s -a
           40° 42' 46.00" N, 74° 0' 21.51" W [Acc: ±15m]

     Obtain a structured JSON object for automated scripting:
           $ whereami -j -n
)";

// ============================================================================
// Core Application Controller
// ============================================================================

class WhereAmIApp {
private:
    CliOptions options;

public:
    int parse_arguments(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                options.show_help = true;
                return 0;
            } else if (arg == "-V" || arg == "--version") {
                options.show_version = true;
                return 0;
            } else if (arg == "-j" || arg == "--json") {
                options.format = OutputFormat::Json;
            } else if (arg == "-s" || arg == "--sexagesimal") {
                options.format = OutputFormat::Sexagesimal;
            } else if (arg == "-a" || arg == "--accuracy") {
                options.show_accuracy = true;
            } else if (arg == "-A" || arg == "--altitude") {
                options.show_altitude = true;
            } else if (arg == "-d" || arg == "--address") {
                options.format = OutputFormat::Address;
                options.show_address = true;
            } else if (arg == "-n" || arg == "--network") {
                options.show_network = true;
            } else if (arg == "-v" || arg == "--verbose" || arg == "--debug") {
                options.verbose = true;
            } else if (arg == "-q" || arg == "--quiet") {
                options.quiet = true;
            } else if (arg == "-f" || arg.rfind("--format=", 0) == 0 || arg == "--format") {
                std::string fmt;
                if (arg.rfind("--format=", 0) == 0) {
                    fmt = arg.substr(9);
                } else if (i + 1 < argc) {
                    fmt = argv[++i];
                }
                if (fmt == "json") options.format = OutputFormat::Json;
                else if (fmt == "sexagesimal" || fmt == "dms") options.format = OutputFormat::Sexagesimal;
                else if (fmt == "human" || fmt == "verbose") options.format = OutputFormat::Human;
                else if (fmt == "address") options.format = OutputFormat::Address;
                else if (fmt == "raw") options.format = OutputFormat::Raw;
                else {
                    std::cerr << "whereami: unknown format '" << fmt << "'\n";
                    return 2;
                }
            } else if (arg == "-t" || arg.rfind("--timeout=", 0) == 0 || arg == "--timeout") {
                std::string t_str = (arg.rfind("--timeout=", 0) == 0) ? arg.substr(10) : argv[++i];
                try {
                    options.timeout_seconds = std::stoi(t_str);
                } catch (...) {
                    std::cerr << "whereami: invalid timeout value\n";
                    return 2;
                }
            } else if (arg.rfind("--from=", 0) == 0) {
                options.from_provider = arg.substr(7);
            } else if (arg.rfind("--basedir=", 0) == 0) {
                options.basedir = arg.substr(10);
            } else if (arg.rfind("--statedir=", 0) == 0) {
                options.statedir = arg.substr(11);
            } else {
                std::cerr << "whereami: unrecognized option '" << arg << "'\n";
                std::cerr << "usage: whereami [-aAdhjnqsvV] [-f format] [-t seconds] [--from provider]\n";
                return 2;
            }
        }
        return 0;
    }

    int execute() {
        if (options.show_help) {
            std::cout << BSD_MANUAL;
            return 0;
        }
        if (options.show_version) {
            std::cout << "whereami 2.0\n";
            return 0;
        }

        // Initialize Winsock for network querying
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        LocationReport report;
        std::vector<std::unique_ptr<ILocationProvider>> providers;

        if (options.from_provider == "sensor") {
            providers.push_back(std::make_unique<WindowsSensorLocationProvider>());
        } else if (options.from_provider == "network") {
            providers.push_back(std::make_unique<NetworkGeoLocationProvider>());
        } else {
            // Auto fallback chain: Sensor -> Network
            providers.push_back(std::make_unique<WindowsSensorLocationProvider>());
            providers.push_back(std::make_unique<NetworkGeoLocationProvider>());
        }

        bool acquired = false;
        for (const auto& provider : providers) {
            if (provider->query(report, options.timeout_seconds, options.verbose)) {
                acquired = true;
                break;
            }
        }

        if (!acquired) {
            if (!options.quiet) {
                std::cerr << "whereami: unable to ascertain current location";
                if (!report.error_message.empty()) {
                    std::cerr << " (" << report.error_message << ")";
                }
                std::cerr << "\n";
            }
            WSACleanup();
            return 1;
        }

        SystemContextCollector::populate(report);

        // Select formatter
        std::unique_ptr<IOutputFormatter> formatter;
        switch (options.format) {
            case OutputFormat::Json:
                formatter = std::make_unique<JsonFormatter>();
                break;
            case OutputFormat::Sexagesimal:
                formatter = std::make_unique<SexagesimalFormatter>();
                break;
            case OutputFormat::Human:
                formatter = std::make_unique<HumanFormatter>();
                break;
            case OutputFormat::Address:
                formatter = std::make_unique<AddressFormatter>();
                break;
            case OutputFormat::Raw:
            default:
                formatter = std::make_unique<RawFormatter>();
                break;
        }

        formatter->output(report, options);
        WSACleanup();
        return 0;
    }
};

// ============================================================================
// Windows Console Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    // Ensure UTF-8 output in Windows Console
    SetConsoleOutputCP(CP_UTF8);

    WhereAmIApp app;
    int parse_res = app.parse_arguments(argc, argv);
    if (parse_res != 0) return parse_res;

    return app.execute();
}