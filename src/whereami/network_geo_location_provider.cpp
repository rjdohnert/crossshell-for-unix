#include "json_mini_parser.hpp"
#include "location_report.hpp"
#include "network_geo_location_provider.hpp"

std::string NetworkGeoLocationProvider::get_name() const  { return "Network / IP Geolocation Service"; }

bool NetworkGeoLocationProvider::query(LocationReport& out_report, int timeout_seconds, bool verbose)  {
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
