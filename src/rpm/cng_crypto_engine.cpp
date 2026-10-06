#include "cng_crypto_engine.hpp"

std::string CngCryptoEngine::calculateFileSha256(const fs::path& filePath) {
        std::error_code ec;
        if (!fs::exists(filePath, ec) || fs::is_directory(filePath, ec)) return "";

        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_HASH_HANDLE hHash = NULL;
        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) return "";

        DWORD cbHash = 0, cbData = 0;
        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);
        std::vector<BYTE> hashBuffer(cbHash);

        if (BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) < 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            return "";
        }

        char buffer[65536];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            BCryptHashData(hHash, (PBYTE)buffer, (ULONG)file.gcount(), 0);
        }

        BCryptFinishHash(hHash, hashBuffer.data(), cbHash, 0);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        std::ostringstream oss;
        for (BYTE b : hashBuffer) oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        return oss.str();
    }
