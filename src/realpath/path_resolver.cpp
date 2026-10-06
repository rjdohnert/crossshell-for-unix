#include "path_resolver.hpp"
#include "realpath_options.hpp"

fs::path PathResolver::stripExtendedPrefix(const fs::path& p) {
        std::wstring s = p.wstring();
        if (s.rfind(L"\\\\?\\UNC\\", 0) == 0) {
            return fs::path(L"\\\\" + s.substr(8));
        }
        if (s.rfind(L"\\\\?\\", 0) == 0) {
            return fs::path(s.substr(4));
        }
        return p;
    }

bool PathResolver::isSubpath(const fs::path& target, const fs::path& base) {
        auto t = target.lexically_normal();
        auto b = base.lexically_normal();
        auto mismatchPair = std::mismatch(b.begin(), b.end(), t.begin(), t.end());
        return mismatchPair.first == b.end();
    }

bool PathResolver::resolve(const fs::path& input, const RealpathOptions& opts, fs::path& outPath, std::string& errMsg) {
        std::error_code ec;
        fs::path resolved;

        switch (opts.mode) {
            case CanonicalMode::Existing:
                resolved = fs::canonical(input, ec);
                if (ec) {
                    errMsg = ec.message();
                    return false;
                }
                break;

            case CanonicalMode::Missing:
                resolved = fs::weakly_canonical(input, ec);
                if (ec) {
                    ec.clear();
                    resolved = fs::absolute(input, ec).lexically_normal();
                    if (ec) {
                        errMsg = ec.message();
                        return false;
                    }
                }
                break;

            case CanonicalMode::NoSymlinks:
                resolved = fs::absolute(input, ec).lexically_normal();
                if (ec) {
                    errMsg = ec.message();
                    return false;
                }
                break;
        }

        resolved = stripExtendedPrefix(resolved);

        if (opts.relativeBase) {
            fs::path baseResolved;
            if (!resolve(*opts.relativeBase, opts, baseResolved, errMsg)) {
                return false;
            }

            if (isSubpath(resolved, baseResolved)) {
                fs::path relTo = opts.relativeTo ? *opts.relativeTo : baseResolved;
                fs::path relResolved;
                if (!resolve(relTo, opts, relResolved, errMsg)) {
                    return false;
                }
                resolved = resolved.lexically_relative(relResolved);
            }
        } else if (opts.relativeTo) {
            fs::path relToResolved;
            if (!resolve(*opts.relativeTo, opts, relToResolved, errMsg)) {
                return false;
            }
            resolved = resolved.lexically_relative(relToResolved);
        }

        outPath = resolved.make_preferred();
        return true;
    }

int PathResolver::execute(const RealpathOptions& opts) {
        int exitCode = 0;
        const char delimiter = opts.zero ? '\0' : '\n';

        for (const auto& file : opts.files) {
            fs::path resolved;
            std::string errMsg;

            if (resolve(fs::path(file), opts, resolved, errMsg)) {
                std::cout << resolved.string() << delimiter;
            } else {
                exitCode = 1;
                if (!opts.quiet) {
                    std::cerr << RealpathOptions::PROGRAM_NAME << ": " << file << ": " << errMsg << "\n";
                }
            }
        }

        return exitCode;
    }
