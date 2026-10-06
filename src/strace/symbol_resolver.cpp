#include "symbol_resolver.hpp"

void SymbolResolver::Initialize(HANDLE hProcess) {
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    SymInitialize(hProcess, NULL, TRUE);
}

void SymbolResolver::Cleanup(HANDLE hProcess) {
    SymCleanup(hProcess);
}

std::string SymbolResolver::Resolve(HANDLE hProcess, DWORD64 address) {
    std::vector<char> buffer(sizeof(SYMBOL_INFO) + MAX_SYM_NAME);
    PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(buffer.data());
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = MAX_SYM_NAME;

    DWORD64 displacement = 0;
    if (SymFromAddr(hProcess, address, &displacement, pSymbol)) {
        std::ostringstream oss;
        oss << pSymbol->Name;
        if (displacement > 0) {
            oss << "+0x" << std::hex << std::uppercase << displacement;
        }
        return oss.str();
    }
    return "<unknown_symbol>";
}
