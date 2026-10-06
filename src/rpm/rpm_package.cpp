#include "dependency.hpp"
#include "file_record.hpp"
#include "rpm_format.hpp"
#include "rpm_package.hpp"

bool RpmPackage::parse(const fs::path& rpmPath) {
        sourcePath = rpmPath;
        std::ifstream file(rpmPath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "error: open of " << rpmPath.string() << " failed: No such file\n";
            return false;
        }

        RpmLeadRaw lead{};
        file.read(reinterpret_cast<char*>(&lead), sizeof(lead));
        if (file.gcount() != sizeof(lead) || swap32(lead.magic) != RPM_MAGIC_LEAD) {
            std::cerr << "error: " << rpmPath.string() << " is not an RPM package\n";
            return false;
        }

        if (!parseHeader(file, true)) return false;
        if (!parseHeader(file, false)) return false;

        payloadOffset = file.tellg();
        return true;
    }

bool RpmPackage::parseHeader(std::ifstream& file, bool isSignature) {
        uint8_t magic[4] = {0};
        file.read(reinterpret_cast<char*>(magic), 4);
        if (magic[0] != 0x8E || magic[1] != 0xAD || magic[2] != 0xE8 || magic[3] != 0x01) {
            return false;
        }

        file.seekg(4, std::ios::cur);
        uint32_t nindex = 0, nbytes = 0;
        file.read(reinterpret_cast<char*>(&nindex), 4);
        file.read(reinterpret_cast<char*>(&nbytes), 4);
        nindex = swap32(nindex);
        nbytes = swap32(nbytes);

        if (nindex > 100000 || nbytes > 100000000) {
            return false;
        }

        std::vector<RpmIndexEntryRaw> entries(nindex);
        if (nindex > 0) {
            file.read(reinterpret_cast<char*>(entries.data()), nindex * sizeof(RpmIndexEntryRaw));
            if (file.gcount() != static_cast<std::streamsize>(nindex * sizeof(RpmIndexEntryRaw))) {
                return false;
            }
        }

        std::vector<char> store(nbytes);
        if (nbytes > 0) {
            file.read(store.data(), nbytes);
            if (file.gcount() != static_cast<std::streamsize>(nbytes)) {
                return false;
            }
        }

        auto getString = [&](uint32_t off) -> std::string {
            if (off >= nbytes) return "";
            size_t len = 0;
            while (off + len < nbytes && store[off + len] != '\0') ++len;
            return std::string(store.data() + off, len);
        };

        auto getStringArray = [&](uint32_t off, uint32_t count) -> std::vector<std::string> {
            std::vector<std::string> arr;
            size_t curr = off;
            for (uint32_t i = 0; i < count && curr < nbytes; ++i) {
                size_t len = 0;
                while (curr + len < nbytes && store[curr + len] != '\0') ++len;
                arr.emplace_back(store.data() + curr, len);
                curr += len + 1;
            }
            return arr;
        };

        auto getUint32Array = [&](uint32_t off, uint32_t count) -> std::vector<uint32_t> {
            std::vector<uint32_t> arr;
            if (off + (size_t)count * 4 > nbytes) return arr;
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t v = 0;
                std::memcpy(&v, store.data() + off + i * 4, 4);
                arr.push_back(swap32(v));
            }
            return arr;
        };

        auto getUint16Array = [&](uint32_t off, uint32_t count) -> std::vector<uint16_t> {
            std::vector<uint16_t> arr;
            if (off + (size_t)count * 2 > nbytes) return arr;
            for (uint32_t i = 0; i < count; ++i) {
                uint16_t v = 0;
                std::memcpy(&v, store.data() + off + i * 2, 2);
                arr.push_back(swap16(v));
            }
            return arr;
        };

        if (isSignature) {
            for (const auto& e : entries) {
                uint32_t tag = swap32(e.tag);
                uint32_t offset = swap32(e.offset);
                if (tag == RPMTAG_SHA256HEADER) {
                    meta.headerSha256 = getString(offset);
                }
            }
            std::streampos pos = file.tellg();
            if (pos % 8 != 0) file.seekg(8 - (pos % 8), std::ios::cur);
            return true;
        }

        std::vector<std::string> dirNames, baseNames, fileDigests, oldFileNames;
        std::vector<std::string> reqNames, reqVersions, provNames, provVersions;
        std::vector<uint32_t> dirIndexes, fileSizes, reqFlags, provFlags;
        std::vector<uint32_t> fileModes;

        for (const auto& entry : entries) {
            uint32_t tag = swap32(entry.tag);
            uint32_t offset = swap32(entry.offset);
            uint32_t count = swap32(entry.count);
            uint32_t type = swap32(entry.type);

            if (offset >= nbytes) continue;

            switch (tag) {
                case RPMTAG_NAME:              meta.name = getString(offset); break;
                case RPMTAG_VERSION:           meta.version = getString(offset); break;
                case RPMTAG_RELEASE:           meta.release = getString(offset); break;
                case RPMTAG_EPOCH: {
                    if (type == RPM_STRING_TYPE || type == RPM_I18NSTRING_TYPE) {
                        meta.epoch = getString(offset);
                    } else if (type == RPM_INT16_TYPE && offset + 2 <= nbytes) {
                        uint16_t ep = 0;
                        std::memcpy(&ep, store.data() + offset, 2);
                        meta.epoch = std::to_string(swap16(ep));
                    } else if (offset + 4 <= nbytes) {
                        uint32_t ep = 0;
                        std::memcpy(&ep, store.data() + offset, 4);
                        meta.epoch = std::to_string(swap32(ep));
                    }
                    break;
                }
                case RPMTAG_SUMMARY:           meta.summary = getString(offset); break;
                case RPMTAG_DESCRIPTION:       meta.description = getString(offset); break;
                case RPMTAG_VENDOR:            meta.vendor = getString(offset); break;
                case RPMTAG_LICENSE:           meta.license = getString(offset); break;
                case RPMTAG_GROUP:             meta.group = getString(offset); break;
                case RPMTAG_URL:               meta.url = getString(offset); break;
                case RPMTAG_OS:                meta.os = getString(offset); break;
                case RPMTAG_ARCH:              meta.arch = getString(offset); break;
                case RPMTAG_PAYLOADCOMPRESSOR: meta.payloadCompressor = getString(offset); break;
                case RPMTAG_PREIN:             meta.preInScript = getString(offset); break;
                case RPMTAG_POSTIN:            meta.postInScript = getString(offset); break;
                case RPMTAG_PREUN:             meta.preUnScript = getString(offset); break;
                case RPMTAG_POSTUN:            meta.postUnScript = getString(offset); break;
                case RPMTAG_SIZE: {
                    if (offset + 4 <= nbytes) {
                        uint32_t sz = 0;
                        std::memcpy(&sz, store.data() + offset, 4);
                        meta.size = swap32(sz);
                    }
                    break;
                }
                case RPMTAG_BUILDTIME: {
                    if (offset + 4 <= nbytes) {
                        uint32_t bt = 0;
                        std::memcpy(&bt, store.data() + offset, 4);
                        meta.buildTime = swap32(bt);
                    }
                    break;
                }
                case RPMTAG_DIRNAMES:          dirNames = getStringArray(offset, count); break;
                case RPMTAG_BASENAMES:         baseNames = getStringArray(offset, count); break;
                case RPMTAG_DIRINDEXES:        dirIndexes = getUint32Array(offset, count); break;
                case RPMTAG_FILESIZES:         fileSizes = getUint32Array(offset, count); break;
                case RPMTAG_FILEMODES: {
                    if (type == RPM_INT32_TYPE) {
                        fileModes = getUint32Array(offset, count);
                    } else {
                        std::vector<uint16_t> m16 = getUint16Array(offset, count);
                        fileModes.assign(m16.begin(), m16.end());
                    }
                    break;
                }
                case RPMTAG_FILEDIGESTS:       fileDigests = getStringArray(offset, count); break;
                case RPMTAG_OLDFILENAMES:      oldFileNames = getStringArray(offset, count); break;
                case RPMTAG_REQUIRENAME:       reqNames = getStringArray(offset, count); break;
                case RPMTAG_REQUIREVERSION:    reqVersions = getStringArray(offset, count); break;
                case RPMTAG_REQUIREFLAGS:      reqFlags = getUint32Array(offset, count); break;
                case RPMTAG_PROVIDENAME:       provNames = getStringArray(offset, count); break;
                case RPMTAG_PROVIDEVERSION:    provVersions = getStringArray(offset, count); break;
                case RPMTAG_PROVIDEFLAGS:      provFlags = getUint32Array(offset, count); break;
                default: break;
            }
        }

        // Modern Triple-Tag Path Reconstruction
        if (!baseNames.empty() && !dirNames.empty() && !dirIndexes.empty()) {
            for (size_t i = 0; i < baseNames.size(); ++i) {
                FileRecord rec;
                uint32_t dIdx = (i < dirIndexes.size()) ? dirIndexes[i] : 0;
                std::string dir = (dIdx < dirNames.size()) ? dirNames[dIdx] : "";
                rec.path = dir + baseNames[i];
                rec.size = (i < fileSizes.size()) ? fileSizes[i] : 0;
                rec.mode = (i < fileModes.size()) ? fileModes[i] : 0644;
                rec.sha256 = (i < fileDigests.size()) ? fileDigests[i] : "";
                meta.files.push_back(rec);
            }
        } else if (!oldFileNames.empty()) {
            for (size_t i = 0; i < oldFileNames.size(); ++i) {
                FileRecord rec;
                rec.path = oldFileNames[i];
                rec.size = (i < fileSizes.size()) ? fileSizes[i] : 0;
                rec.mode = (i < fileModes.size()) ? fileModes[i] : 0644;
                rec.sha256 = (i < fileDigests.size()) ? fileDigests[i] : "";
                meta.files.push_back(rec);
            }
        }

        for (size_t i = 0; i < reqNames.size(); ++i) {
            Dependency dep;
            dep.name = reqNames[i];
            dep.version = (i < reqVersions.size()) ? reqVersions[i] : "";
            dep.flags = (i < reqFlags.size()) ? reqFlags[i] : 0;
            meta.requirements.push_back(dep);
        }

        for (size_t i = 0; i < provNames.size(); ++i) {
            Dependency dep;
            dep.name = provNames[i];
            dep.version = (i < provVersions.size()) ? provVersions[i] : "";
            dep.flags = (i < provFlags.size()) ? provFlags[i] : 0;
            meta.provides.push_back(dep);
        }

        return true;
    }
