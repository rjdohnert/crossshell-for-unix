#include "http_downloader.hpp"
#include "output_quoting.hpp"
#include "pipe_session.hpp"

int download_http_file(std::string url, const std::string& filename, OutputFormat outputFormat, const std::string& pipeCommand) {
    PipeSession pipeSession(pipeCommand);
    std::string tempFilename = filename + ".part";

    const DWORD timeoutMs = 30000;
    const int maxAttempts = 3;
    int redirectCount = 0;
    const int maxRedirects = 5;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        HINTERNET hInternet = InternetOpenA(
            "Wget/1.0",                // User-Agent
            INTERNET_OPEN_TYPE_PRECONFIG, // Use registry settings for proxy config
            NULL,
            NULL,
            0
        );

        if (!hInternet) {
            std::cerr << "Error: InternetOpen failed. Error code: " << GetLastError() << "\n";
            return 1;
        }

        InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));
        InternetSetOptionA(hInternet, INTERNET_OPTION_SEND_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));
        InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&timeoutMs, sizeof(timeoutMs));

        // Open the connection to the URL.
        HINTERNET hUrl = InternetOpenUrlA(
            hInternet,
            url.c_str(),
            NULL,
            0,
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_AUTO_REDIRECT,
            0
        );

        if (!hUrl) {
            DWORD err = GetLastError();
            std::cerr << "Error: InternetOpenUrl failed. Error code: " << err << "\n";
            InternetCloseHandle(hInternet);
            if (attempt == maxAttempts) {
                return 1;
            }
            std::cerr << "Retrying (" << (attempt + 1) << "/" << maxAttempts << ")...\n";
            continue;
        }

        char statusCodeBuffer[16] = { 0 };
        DWORD statusCodeBufferSize = sizeof(statusCodeBuffer);
        if (!HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE, statusCodeBuffer, &statusCodeBufferSize, NULL)) {
            DWORD err = GetLastError();
            std::cerr << "Error: Unable to determine HTTP status code. Error code: " << err << "\n";
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            if (attempt == maxAttempts) {
                return 1;
            }
            std::cerr << "Retrying (" << (attempt + 1) << "/" << maxAttempts << ")...\n";
            continue;
        }

        long statusCode = std::strtol(statusCodeBuffer, nullptr, 10);

        // Handle HTTP Redirects (301, 302, 303, 307, 308)
        if (statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308) {
            if (++redirectCount > maxRedirects) {
                std::cerr << "Error: Too many redirects.\n";
                InternetCloseHandle(hUrl);
                InternetCloseHandle(hInternet);
                return 1;
            }
            char locationBuffer[2048] = { 0 };
            DWORD locationBufferSize = sizeof(locationBuffer);
            if (HttpQueryInfoA(hUrl, HTTP_QUERY_LOCATION, locationBuffer, &locationBufferSize, NULL)) {
                url = locationBuffer;
                std::cout << "Following redirect to: " << url << "\n";
                InternetCloseHandle(hUrl);
                InternetCloseHandle(hInternet);
                attempt--; // Don't count redirects against download attempts
                continue;
            }
        }

        if (statusCode < 200 || statusCode >= 300) {
            std::cerr << "Error: Server returned HTTP status " << statusCode << ".\n";
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);

            bool isTransient = (statusCode == 429 || statusCode == 500 || statusCode == 502 || statusCode == 503 || statusCode == 504);
            if (isTransient && attempt < maxAttempts) {
                std::cerr << "Retrying (" << (attempt + 1) << "/" << maxAttempts << ")...\n";
                continue;
            }
            return 1;
        }

        // Attempt to get Content-Length header to show download size (optional)
        uint64_t contentLength = 0;
        char contentLengthBuffer[32] = { 0 };
        DWORD contentLengthBufferSize = sizeof(contentLengthBuffer);
        bool lengthAvailable = HttpQueryInfoA(
            hUrl,
            HTTP_QUERY_CONTENT_LENGTH,
            contentLengthBuffer,
            &contentLengthBufferSize,
            NULL
        );
        if (lengthAvailable) {
            contentLength = std::strtoull(contentLengthBuffer, nullptr, 10);
        }

        // Open temporary local file for writing in binary mode.
        std::ofstream outFile(tempFilename, std::ios::binary | std::ios::trunc);
        if (!outFile.is_open()) {
            std::cerr << "Error: Failed to open output file " << tempFilename << " for writing.\n";
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return 1;
        }

        if (outputFormat == OutputFormat::Human) {
            std::cout << "Downloading: " << url << "\n";
            std::cout << "Saving to  : " << filename << "\n";
        }

        char buffer[65536]; // 64 KB
        DWORD bytesRead = 0;
        size_t totalBytesDownloaded = 0;
        bool downloadSucceeded = true;

        // Read the data in chunks and write it to the file.
        while (true) {
            if (!InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead)) {
                std::cerr << "\nError: InternetReadFile failed. Error code: " << GetLastError() << "\n";
                downloadSucceeded = false;
                break;
            }

            if (bytesRead == 0) {
                break;
            }

            outFile.write(buffer, bytesRead);
            if (!outFile.good()) {
                std::cerr << "\nError: Failed to write downloaded data to file.\n";
                downloadSucceeded = false;
                break;
            }

            totalBytesDownloaded += bytesRead;

            if (outputFormat == OutputFormat::Human && lengthAvailable && contentLength > 0) {
                double percent = (static_cast<double>(totalBytesDownloaded) / contentLength) * 100.0;
                std::printf("\rProgress: %.2f%% (%zu / %llu bytes)", percent, totalBytesDownloaded, contentLength);
            } else if (outputFormat == OutputFormat::Human) {
                std::printf("\rDownloaded: %zu bytes", totalBytesDownloaded);
            }
            std::fflush(stdout);
        }

        // Clean up resources
        outFile.close();
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);

        if (!downloadSucceeded) {
            DeleteFileA(tempFilename.c_str());
            if (attempt == maxAttempts) {
                return 1;
            }
            std::cerr << "Retrying (" << (attempt + 1) << "/" << maxAttempts << ")...\n";
            continue;
        }

        if (!MoveFileExA(tempFilename.c_str(), filename.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            std::cerr << "\nError: Failed to finalize download file. Error code: " << GetLastError() << "\n";
            DeleteFileA(tempFilename.c_str());
            return 1;
        }

        if (outputFormat == OutputFormat::Json) {
            std::cout << "{\"status\":\"success\",\"url\":" << JsonQuote(url) << ",\"file\":" << JsonQuote(filename) << ",\"bytes\":" << totalBytesDownloaded << "}\n";
        } else if (outputFormat == OutputFormat::Csv) {
            std::cout << "\"status\",\"url\",\"file\",bytes\n\"success\"," << CsvQuote(url) << ',' << CsvQuote(filename) << ',' << totalBytesDownloaded << "\n";
        } else if (outputFormat == OutputFormat::Table) {
            std::cout << "STATUS\tURL\tFILE\tBYTES\nSUCCESS\t" << url << '\t' << filename << '\t' << totalBytesDownloaded << "\n";
        } else std::cout << "\nDownload complete.\n";
        return 0;
    }

    return 1;
}
