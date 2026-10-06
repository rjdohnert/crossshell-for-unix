#include "json.hpp"
#include "output_formatter.hpp"
#include "registry_record.hpp"
#include "string_conversion.hpp"

void OutputFormatter::PrintDiff(const std::string& path, const std::string& value, const RegistryRecord& oldRec, const std::string& newType, const std::string& newData, bool willDelete ) {
        std::cout << "\n\n";
        std::cout << " DIFF PREVIEW: " << path << (value.empty() ? " (Default)" : " -> " + value) << "\n";
        std::cout << "\n";
        if (willDelete) {
            std::cout << " [-] ACTION: VALUE OR SUBKEY WILL BE DELETED\n";
            if (oldRec.exists) {
                std::cout << " [-] Current Type : " << oldRec.GetTypeString() << "\n";
                std::cout << " [-] Current Value: " << oldRec.GetFormattedData() << "\n";
            }
        } else {
            if (oldRec.exists) {
                std::cout << " [~] ACTION: VALUE WILL BE OVERWRITTEN\n";
                std::cout << " [-] Old Type : " << oldRec.GetTypeString() << "\n";
                std::cout << " [-] Old Value: " << oldRec.GetFormattedData() << "\n";
            } else {
                std::cout << " [+] ACTION: VALUE WILL BE CREATED\n";
            }
            std::cout << " [+] New Type : " << newType << "\n";
            std::cout << " [+] New Value: " << newData << "\n";
        }
        std::cout << "-------------------------------------------------------\n\n";
    }

void OutputFormatter::PrintJson(const RegistryRecord& rec) {
        Json::Object obj;
        obj["path"] = rec.rootKey + "\\" + rec.subKey;
        obj["value"] = rec.valueName;
        obj["exists"] = rec.exists;
        obj["type"] = rec.GetTypeString();
        obj["data"] = rec.GetFormattedData();
        obj["raw_hex"] = Utils::BytesToHex(rec.rawData);
        std::cout << Json::Stringify(Json::Value(obj), 0) << "\n";
    }
