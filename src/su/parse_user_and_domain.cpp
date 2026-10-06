#include "parse_user_and_domain.hpp"

void ParseUserAndDomain(const std::wstring& inputUser, std::wstring& username, std::wstring& domain) {
    size_t slashPos = inputUser.find(L'\\');
    size_t atPos = inputUser.find(L'@');

    if (slashPos != std::wstring::npos) {
        domain = inputUser.substr(0, slashPos);
        username = inputUser.substr(slashPos + 1);
    } else if (atPos != std::wstring::npos) {
        username = inputUser.substr(0, atPos);
        domain = inputUser.substr(atPos + 1);
    } else {
        username = inputUser;
        domain = L"."; // Local machine
    }
}
