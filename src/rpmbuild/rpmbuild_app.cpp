#include "cng_crypto.hpp"
#include "cpio_compiler.hpp"
#include "header_serializer.hpp"
#include "help_formatter.hpp"
#include "rpm_format.hpp"
#include "rpmbuild_app.hpp"
#include "spec_parser.hpp"
#include "stage_runner.hpp"

int runRpmbuild(int argc, char* argv[]) {
    if (argc <= 1) {
        std::cerr << "rpmbuild: no spec file or build mode specified.\nTry 'rpmbuild --help' for more information.\n";
        return 1;
    }

    std::string specFilePath;
    std::string explicitOutput;
    std::string overrideBuildRoot;
    std::string overrideTarget;
    std::map<std::string, std::string> cliMacros;
    bool modeAll = false, modeBinary = false, modeSource = false, modePrep = false, modeCompile = false, modeInstall = false, modeCheckFiles = false;
    bool optClean = false, optVerbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-?") {
            printHelp();
            return 0;
        } else if (arg == "--version") {
            std::cout << "RPM build version 3.8.19\n";
            return 0;
        } else if (arg == "-ba") { modeAll = true; }
        else if (arg == "-bb") { modeBinary = true; }
        else if (arg == "-bs") { modeSource = true; }
        else if (arg == "-bp") { modePrep = true; }
        else if (arg == "-bc") { modeCompile = true; }
        else if (arg == "-bi") { modeInstall = true; }
        else if (arg == "-bl") { modeCheckFiles = true; }
        else if (arg == "--clean") { optClean = true; }
        else if (arg == "-v" || arg == "--verbose") { optVerbose = true; }
        else if (arg.rfind("--output=", 0) == 0) { explicitOutput = arg.substr(9); }
        else if (arg.rfind("--buildroot=", 0) == 0) { overrideBuildRoot = arg.substr(12); }
        else if (arg.rfind("--target=", 0) == 0) { overrideTarget = arg.substr(9); }
        else if (arg == "--target" && i + 1 < argc) { overrideTarget = argv[++i]; }
        else if (arg.rfind("--define=", 0) == 0) {
            std::string def = arg.substr(9);
            auto sp = def.find_first_of(" =");
            if (sp != std::string::npos) cliMacros[def.substr(0, sp)] = SpecParser::stripQuotes(def.substr(sp + 1));
        } else if (arg == "-D" && i + 1 < argc) {
            std::string def = argv[++i];
            auto sp = def.find_first_of(" =");
            if (sp != std::string::npos) cliMacros[def.substr(0, sp)] = SpecParser::stripQuotes(def.substr(sp + 1));
        } else if (arg.rfind("-", 0) != 0) {
            specFilePath = arg;
        }
    }

    if (specFilePath.empty()) {
        std::cerr << "error: No spec file provided.\n";
        return 1;
    }

    if (!modeAll && !modeBinary && !modeSource && !modePrep && !modeCompile && !modeInstall && !modeCheckFiles) {
        modeBinary = true; // Default to binary package build
    }

    SpecParser spec;
    for (const auto& [k, v] : cliMacros) {
        spec.macros[k] = v;
        if (k == "name") spec.name = v;
        else if (k == "version") spec.version = v;
        else if (k == "release") spec.release = v;
    }

    if (!spec.parseFile(specFilePath)) return 1;
    if (!overrideBuildRoot.empty()) spec.buildRoot = overrideBuildRoot;
    if (!overrideTarget.empty()) spec.buildArch = overrideTarget;

    std::cout << "Executing build for package: " << spec.name << "-" << spec.version << "-" << spec.release << "\n";

    // Handle Source RPM only build (-bs)
    if (modeSource) {
        std::string srcOut = spec.name + "-" + spec.version + "-" + spec.release + ".src.rpm";
        std::ofstream srcRpm(srcOut, std::ios::binary);
        if (!srcRpm.is_open()) {
            std::cerr << "error: Cannot open output source package for writing: " << srcOut << "\n";
            return 1;
        }

        RpmLeadRaw srcLead{};
        srcLead.magic = swap32(RPM_MAGIC_LEAD);
        srcLead.major = 3;
        srcLead.type = swap16(1); // 1 = Source RPM
        srcLead.archnum = swap16(1);
        strncpy_s(srcLead.name, spec.name.c_str(), _TRUNCATE);
        srcLead.osnum = swap16(1);
        srcLead.signature_type = swap16(5);
        srcRpm.write(reinterpret_cast<char*>(&srcLead), sizeof(srcLead));

        HeaderSerializer srcMainHdr;
        srcMainHdr.addString(RPMTAG_NAME, spec.name);
        srcMainHdr.addString(RPMTAG_VERSION, spec.version);
        srcMainHdr.addString(RPMTAG_RELEASE, spec.release);
        srcMainHdr.addString(RPMTAG_SUMMARY, spec.summary);
        srcMainHdr.addString(RPMTAG_DESCRIPTION, spec.description);
        srcMainHdr.addString(RPMTAG_OS, "windows");
        srcMainHdr.addString(RPMTAG_ARCH, "src");
        srcMainHdr.addString(RPMTAG_PAYLOADFORMAT, "cpio");
        srcMainHdr.addString(RPMTAG_PAYLOADCOMPRESSOR, "none");
        srcMainHdr.addInt32(RPMTAG_BUILDTIME, (uint32_t)time(nullptr));

        std::vector<char> serSrcMain = srcMainHdr.serialize();
        std::string srcSha256 = CngCrypto::calculateMemorySha256(serSrcMain.data(), serSrcMain.size());

        HeaderSerializer srcSigHdr;
        srcSigHdr.addString(RPMTAG_SHA256HEADER, srcSha256);
        srcSigHdr.addInt32(RPMTAG_SIZE, (uint32_t)serSrcMain.size());
        std::vector<char> serSrcSig = srcSigHdr.serialize();

        srcRpm.write(serSrcSig.data(), serSrcSig.size());
        std::streampos srcPos = srcRpm.tellp();
        if (srcPos % 8 != 0) {
            size_t pad = 8 - (srcPos % 8);
            for (size_t i = 0; i < pad; ++i) srcRpm.put('\0');
        }
        srcRpm.write(serSrcMain.data(), serSrcMain.size());

        fs::path sPath(specFilePath);
        CpioCompiler::appendFile(srcRpm, "./" + sPath.filename().generic_string(), sPath, 0100644);
        CpioCompiler::appendTrailer(srcRpm);
        srcRpm.close();
        std::cout << "Wrote: " << fs::absolute(srcOut).string() << " (" << fs::file_size(srcOut) << " bytes)\n";
        return 0;
    }

    // 1. %prep Stage
    if (!StageRunner::execute(spec.prepScript, "prep", spec.buildRoot)) return 1;
    if (modePrep) return 0;

    // 2. %build Stage
    if (!StageRunner::execute(spec.buildScript, "build", spec.buildRoot)) return 1;
    if (modeCompile) return 0;

    // 3. %install Stage
    if (!StageRunner::execute(spec.installScript, "install", spec.buildRoot)) return 1;
    if (modeInstall) return 0;

    // 4. File Discovery & SHA-256 Digest Matrix Generation
    std::cout << "Processing %files section and calculating cryptographic SHA-256 digests...\n";
    std::vector<std::string> dirNames = {"/"};
    std::vector<std::string> baseNames;
    std::vector<uint32_t> dirIndexes;
    std::vector<uint32_t> fileSizes;
    std::vector<uint16_t> fileModes;
    std::vector<uint32_t> fileFlags;
    std::vector<std::string> fileDigests;

    struct DiscoveredFile {
        std::string relPath;
        fs::path fullDiskPath;
        uint32_t size;
        uint32_t mode;
        uint32_t flags;
        std::string sha256;
    };
    std::vector<DiscoveredFile> packageFiles;

    fs::path bRoot(spec.buildRoot);
    std::error_code ec;
    if (!fs::exists(bRoot, ec)) {
        std::cerr << "error: BUILDROOT directory does not exist: " << spec.buildRoot << "\n";
        return 1;
    }

    for (const auto& entry : fs::recursive_directory_iterator(bRoot, ec)) {
        if (entry.is_regular_file(ec)) {
            fs::path rel = fs::relative(entry.path(), bRoot, ec);
            std::string relStr = rel.generic_string();
            std::string dir = rel.parent_path().generic_string();
            if (!dir.empty()) dir = "/" + dir + "/";
            else dir = "/";

            auto it = std::find(dirNames.begin(), dirNames.end(), dir);
            uint32_t dIdx = 0;
            if (it == dirNames.end()) {
                dIdx = (uint32_t)dirNames.size();
                dirNames.push_back(dir);
            } else {
                dIdx = (uint32_t)(it - dirNames.begin());
            }

            uint32_t sz = static_cast<uint32_t>(entry.file_size(ec));
            std::string hash = CngCrypto::calculateFileSha256(entry.path());

            uint32_t mode = 0100644;
            uint32_t flags = 0;

            std::string fname = rel.filename().generic_string();
            bool matched = false;
            // Match exact relative path first
            for (const auto& f : spec.files) {
                std::string fClean = f.path;
                while (!fClean.empty() && (fClean[0] == '/' || fClean[0] == '\\')) fClean = fClean.substr(1);
                if (fClean == relStr) {
                    mode = f.mode;
                    flags = f.flags;
                    matched = true;
                    break;
                }
            }
            // Fallback to filename match
            if (!matched) {
                for (const auto& f : spec.files) {
                    fs::path fPath(f.path);
                    if (fPath.filename().generic_string() == fname) {
                        mode = f.mode;
                        flags = f.flags;
                        break;
                    }
                }
            }

            dirIndexes.push_back(dIdx);
            baseNames.push_back(fname);
            fileSizes.push_back(sz);
            fileModes.push_back((uint16_t)mode);
            fileFlags.push_back(flags);
            fileDigests.push_back(hash);

            packageFiles.push_back({relStr, entry.path(), sz, mode, flags, hash});
            if (optVerbose) {
                std::cout << "  File: " << relStr << " [SHA256: " << hash.substr(0, 12) << "... Mode: " << std::oct << mode << std::dec << "]\n";
            }
        }
    }

    if (packageFiles.empty()) {
        std::cerr << "error: No files staged in BUILDROOT to package.\n";
        return 1;
    }
    if (modeCheckFiles) {
        std::cout << "Check %files passed cleanly. Total files: " << packageFiles.size() << "\n";
        return 0;
    }

    // 5. Serialize Immutable Main Header
    HeaderSerializer mainHdr;
    mainHdr.addString(RPMTAG_NAME, spec.name);
    mainHdr.addString(RPMTAG_VERSION, spec.version);
    mainHdr.addString(RPMTAG_RELEASE, spec.release);
    
    uint32_t epVal = 0;
    try { epVal = static_cast<uint32_t>(std::stoul(spec.epoch)); } catch (...) {}
    mainHdr.addInt32(RPMTAG_EPOCH, epVal);

    mainHdr.addString(RPMTAG_SUMMARY, spec.summary);
    mainHdr.addString(RPMTAG_DESCRIPTION, spec.description);
    mainHdr.addString(RPMTAG_VENDOR, spec.vendor);
    mainHdr.addString(RPMTAG_PACKAGER, spec.packager);
    mainHdr.addString(RPMTAG_LICENSE, spec.license);
    mainHdr.addString(RPMTAG_GROUP, spec.group);
    mainHdr.addString(RPMTAG_URL, spec.url);
    mainHdr.addString(RPMTAG_OS, "windows");
    mainHdr.addString(RPMTAG_ARCH, spec.buildArch);
    mainHdr.addString(RPMTAG_PAYLOADFORMAT, "cpio");
    mainHdr.addString(RPMTAG_PAYLOADCOMPRESSOR, "none");
    mainHdr.addInt32(RPMTAG_BUILDTIME, (uint32_t)time(nullptr));

    uint32_t totalInstalledSize = 0;
    for (uint32_t sz : fileSizes) totalInstalledSize += sz;
    mainHdr.addInt32(RPMTAG_SIZE, totalInstalledSize);

    if (!spec.preScript.empty()) mainHdr.addString(RPMTAG_PREIN, spec.preScript);
    if (!spec.postScript.empty()) mainHdr.addString(RPMTAG_POSTIN, spec.postScript);
    if (!spec.preunScript.empty()) mainHdr.addString(RPMTAG_PREUN, spec.preunScript);
    if (!spec.postunScript.empty()) mainHdr.addString(RPMTAG_POSTUN, spec.postunScript);

    // Modern Indexed Path Triples & Digest Matrix
    mainHdr.addStringArray(RPMTAG_DIRNAMES, dirNames);
    mainHdr.addStringArray(RPMTAG_BASENAMES, baseNames);
    mainHdr.addUint32Array(RPMTAG_DIRINDEXES, dirIndexes);
    mainHdr.addUint32Array(RPMTAG_FILESIZES, fileSizes);
    mainHdr.addUint16Array(RPMTAG_FILEMODES, fileModes);
    mainHdr.addUint32Array(RPMTAG_FILEFLAGS, fileFlags);
    mainHdr.addStringArray(RPMTAG_FILEDIGESTS, fileDigests);

    // Dependencies
    if (!spec.requirements.empty()) {
        std::vector<std::string> reqN, reqV;
        std::vector<uint32_t> reqF;
        for (const auto& r : spec.requirements) {
            reqN.push_back(r.name);
            reqV.push_back(r.version);
            reqF.push_back(r.flags);
        }
        mainHdr.addStringArray(RPMTAG_REQUIRENAME, reqN);
        mainHdr.addStringArray(RPMTAG_REQUIREVERSION, reqV);
        mainHdr.addUint32Array(RPMTAG_REQUIREFLAGS, reqF);
    }

    if (!spec.provides.empty()) {
        std::vector<std::string> provN, provV;
        std::vector<uint32_t> provF;
        for (const auto& p : spec.provides) {
            provN.push_back(p.name);
            provV.push_back(p.version);
            provF.push_back(p.flags);
        }
        mainHdr.addStringArray(RPMTAG_PROVIDENAME, provN);
        mainHdr.addStringArray(RPMTAG_PROVIDEVERSION, provV);
        mainHdr.addUint32Array(RPMTAG_PROVIDEFLAGS, provF);
    }

    if (!spec.conflicts.empty()) {
        std::vector<std::string> confN;
        for (const auto& c : spec.conflicts) {
            confN.push_back(c.name);
        }
        mainHdr.addStringArray(RPMTAG_CONFLICTNAME, confN);
    }

    std::vector<char> serializedMainHdr = mainHdr.serialize();

    // 6. Calculate Immutable Header SHA-256 Digest
    std::string headerSha256 = CngCrypto::calculateMemorySha256(serializedMainHdr.data(), serializedMainHdr.size());

    // 7. Serialize Signature Header
    HeaderSerializer sigHdr;
    sigHdr.addString(RPMTAG_SHA256HEADER, headerSha256);
    sigHdr.addInt32(RPMTAG_SIZE, (uint32_t)serializedMainHdr.size());
    std::vector<char> serializedSigHdr = sigHdr.serialize();

    // 8. Determine Output Path
    std::string finalOut = explicitOutput;
    if (finalOut.empty()) {
        finalOut = spec.name + "-" + spec.version + "-" + spec.release + "." + spec.buildArch + ".rpm";
    }

    // 9. Write Binary RPM File (Lead + Sig Header + 8-byte Align + Main Header + CPIO Payload)
    std::ofstream rpm(finalOut, std::ios::binary);
    if (!rpm.is_open()) {
        std::cerr << "error: Cannot open output file for writing: " << finalOut << "\n";
        return 1;
    }

    RpmLeadRaw lead{};
    lead.magic = swap32(RPM_MAGIC_LEAD);
    lead.major = 3;
    lead.minor = 0;
    lead.type = 0;
    lead.archnum = swap16(1);
    strncpy_s(lead.name, spec.name.c_str(), _TRUNCATE);
    lead.osnum = swap16(1);
    lead.signature_type = swap16(5);
    rpm.write(reinterpret_cast<char*>(&lead), sizeof(lead));

    rpm.write(serializedSigHdr.data(), serializedSigHdr.size());

    std::streampos pos = rpm.tellp();
    if (pos % 8 != 0) {
        size_t pad = 8 - (pos % 8);
        for (size_t i = 0; i < pad; ++i) rpm.put('\0');
    }

    rpm.write(serializedMainHdr.data(), serializedMainHdr.size());

    for (const auto& pf : packageFiles) {
        std::string cpioPath = pf.relPath;
        while (!cpioPath.empty() && (cpioPath[0] == '/' || cpioPath[0] == '\\')) cpioPath = cpioPath.substr(1);
        CpioCompiler::appendFile(rpm, "./" + cpioPath, pf.fullDiskPath, pf.mode);
    }
    CpioCompiler::appendTrailer(rpm);

    rpm.close();
    std::cout << "Wrote: " << fs::absolute(finalOut).string() << " (" << fs::file_size(finalOut) << " bytes)\n";

    // If -ba was specified, also generate source package (.src.rpm)
    if (modeAll) {
        std::string srcOut = spec.name + "-" + spec.version + "-" + spec.release + ".src.rpm";
        std::ofstream srcRpm(srcOut, std::ios::binary);
        if (srcRpm.is_open()) {
            RpmLeadRaw srcLead{};
            srcLead.magic = swap32(RPM_MAGIC_LEAD);
            srcLead.major = 3;
            srcLead.type = swap16(1); // 1 = Source RPM
            srcLead.archnum = swap16(1);
            strncpy_s(srcLead.name, spec.name.c_str(), _TRUNCATE);
            srcLead.osnum = swap16(1);
            srcLead.signature_type = swap16(5);
            srcRpm.write(reinterpret_cast<char*>(&srcLead), sizeof(srcLead));

            HeaderSerializer srcMainHdr;
            srcMainHdr.addString(RPMTAG_NAME, spec.name);
            srcMainHdr.addString(RPMTAG_VERSION, spec.version);
            srcMainHdr.addString(RPMTAG_RELEASE, spec.release);
            srcMainHdr.addString(RPMTAG_SUMMARY, spec.summary);
            srcMainHdr.addString(RPMTAG_DESCRIPTION, spec.description);
            srcMainHdr.addString(RPMTAG_OS, "windows");
            srcMainHdr.addString(RPMTAG_ARCH, "src");
            srcMainHdr.addString(RPMTAG_PAYLOADFORMAT, "cpio");
            srcMainHdr.addString(RPMTAG_PAYLOADCOMPRESSOR, "none");
            srcMainHdr.addInt32(RPMTAG_BUILDTIME, (uint32_t)time(nullptr));

            std::vector<char> serSrcMain = srcMainHdr.serialize();
            std::string srcSha256 = CngCrypto::calculateMemorySha256(serSrcMain.data(), serSrcMain.size());

            HeaderSerializer srcSigHdr;
            srcSigHdr.addString(RPMTAG_SHA256HEADER, srcSha256);
            srcSigHdr.addInt32(RPMTAG_SIZE, (uint32_t)serSrcMain.size());
            std::vector<char> serSrcSig = srcSigHdr.serialize();

            srcRpm.write(serSrcSig.data(), serSrcSig.size());
            std::streampos srcPos = srcRpm.tellp();
            if (srcPos % 8 != 0) {
                size_t pad = 8 - (srcPos % 8);
                for (size_t i = 0; i < pad; ++i) srcRpm.put('\0');
            }
            srcRpm.write(serSrcMain.data(), serSrcMain.size());

            fs::path sPath(specFilePath);
            CpioCompiler::appendFile(srcRpm, "./" + sPath.filename().generic_string(), sPath, 0100644);
            CpioCompiler::appendTrailer(srcRpm);
            srcRpm.close();
            std::cout << "Wrote: " << fs::absolute(srcOut).string() << " (" << fs::file_size(srcOut) << " bytes)\n";
        }
    }

    if (!spec.cleanScript.empty()) {
        StageRunner::execute(spec.cleanScript, "clean", spec.buildRoot);
    }

    if (optClean) {
        fs::remove_all(bRoot, ec);
    }
    return 0;
}
