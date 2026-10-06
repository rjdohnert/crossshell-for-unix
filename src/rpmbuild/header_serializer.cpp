#include "header_serializer.hpp"
#include "rpm_format.hpp"

size_t HeaderSerializer::getAlignment(uint32_t type) {
        switch (type) {
            case RPM_INT16_TYPE: return 2;
            case RPM_INT32_TYPE: return 4;
            case RPM_INT64_TYPE: return 8;
            default: return 1;
        }
    }

void HeaderSerializer::addString(uint32_t tag, const std::string& str) {
        TagEntry e;
        e.tag = tag; e.type = RPM_STRING_TYPE; e.count = 1;
        e.data.assign(str.c_str(), str.c_str() + str.size() + 1);
        entries.push_back(e);
    }

void HeaderSerializer::addInt32(uint32_t tag, uint32_t val) {
        TagEntry e;
        e.tag = tag; e.type = RPM_INT32_TYPE; e.count = 1;
        uint32_t be = swap32(val);
        e.data.resize(4);
        std::memcpy(e.data.data(), &be, 4);
        entries.push_back(e);
    }

void HeaderSerializer::addUint16Array(uint32_t tag, const std::vector<uint16_t>& arr) {
        if (arr.empty()) return;
        TagEntry e;
        e.tag = tag; e.type = RPM_INT16_TYPE; e.count = (uint32_t)arr.size();
        e.data.resize(arr.size() * 2);
        for (size_t i = 0; i < arr.size(); ++i) {
            uint16_t be = swap16(arr[i]);
            std::memcpy(e.data.data() + (i * 2), &be, 2);
        }
        entries.push_back(e);
    }

void HeaderSerializer::addUint32Array(uint32_t tag, const std::vector<uint32_t>& arr) {
        if (arr.empty()) return;
        TagEntry e;
        e.tag = tag; e.type = RPM_INT32_TYPE; e.count = (uint32_t)arr.size();
        e.data.resize(arr.size() * 4);
        for (size_t i = 0; i < arr.size(); ++i) {
            uint32_t be = swap32(arr[i]);
            std::memcpy(e.data.data() + (i * 4), &be, 4);
        }
        entries.push_back(e);
    }

void HeaderSerializer::addStringArray(uint32_t tag, const std::vector<std::string>& arr) {
        if (arr.empty()) return;
        TagEntry e;
        e.tag = tag; e.type = RPM_STRING_ARRAY; e.count = (uint32_t)arr.size();
        for (const auto& s : arr) {
            e.data.insert(e.data.end(), s.c_str(), s.c_str() + s.size() + 1);
        }
        entries.push_back(e);
    }

std::vector<char> HeaderSerializer::serialize() {
        std::sort(entries.begin(), entries.end(), [](const TagEntry& a, const TagEntry& b) {
            return a.tag < b.tag;
        });

        // Compute aligned offsets and build aligned data store
        std::vector<char> dataStore;
        std::vector<RpmIndexEntryRaw> indexEntries;

        for (const auto& e : entries) {
            size_t align = getAlignment(e.type);
            size_t pad = (align - (dataStore.size() % align)) % align;
            for (size_t i = 0; i < pad; ++i) dataStore.push_back('\0');

            RpmIndexEntryRaw idx;
            idx.tag = swap32(e.tag);
            idx.type = swap32(e.type);
            idx.offset = swap32((uint32_t)dataStore.size());
            idx.count = swap32(e.count);
            indexEntries.push_back(idx);

            dataStore.insert(dataStore.end(), e.data.begin(), e.data.end());
        }

        std::vector<char> buffer;
        uint8_t magic[4] = {0x8e, 0xad, 0xe8, 0x01};
        buffer.insert(buffer.end(), magic, magic + 4);
        uint32_t reserved = 0;
        buffer.insert(buffer.end(), (char*)&reserved, (char*)&reserved + 4);

        uint32_t nindex = swap32((uint32_t)indexEntries.size());
        uint32_t nbytes = swap32((uint32_t)dataStore.size());

        buffer.insert(buffer.end(), (char*)&nindex, (char*)&nindex + 4);
        buffer.insert(buffer.end(), (char*)&nbytes, (char*)&nbytes + 4);

        for (const auto& idx : indexEntries) {
            buffer.insert(buffer.end(), (char*)&idx, (char*)&idx + sizeof(idx));
        }

        buffer.insert(buffer.end(), dataStore.begin(), dataStore.end());
        return buffer;
    }
