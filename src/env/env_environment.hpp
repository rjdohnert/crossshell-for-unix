#ifndef ENV_ENVIRONMENT_HPP
#define ENV_ENVIRONMENT_HPP

#include <map>
#include <string>
#include <vector>

class EnvironmentManager {
public:
    struct CaseInsensitiveLess {
        bool operator()(const std::wstring& left,
                        const std::wstring& right) const;
    };

    using Map = std::map<std::wstring, std::wstring, CaseInsensitiveLess>;

    bool LoadSystemEnvironment(std::wstring& error);
    bool Set(const std::wstring& name, const std::wstring& value,
             std::wstring& error);
    bool Unset(const std::wstring& name, std::wstring& error);
    bool CreateEnvironmentBlock(std::vector<wchar_t>& block,
                                std::wstring& error) const;
    const Map& Values() const;

private:
    static bool IsValidName(const std::wstring& name);

    Map values_;
};

#endif
