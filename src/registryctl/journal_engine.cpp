#include "journal_engine.hpp"
#include "json.hpp"
#include "undo_record.hpp"

void JournalEngine::WriteUndoLog(const std::string& filename, const std::vector<UndoRecord>& records) {
        Json::Array arr;
        for (const auto& r : records) {
            Json::Object obj;
            obj["op"] = Json::Value(r.op);
            obj["path"] = Json::Value(r.path);
            obj["value"] = r.valueName;
            obj["type"] = r.typeStr;
            obj["raw_hex"] = r.rawDataHex;
            obj["had_previous"] = r.hadPreviousValue;
            arr.push_back(Json::Value(obj));
        }
        Json::Object root;
        root["version"] = "1.1";
        root["generator"] = "registryctl";
        root["record_count"] = static_cast<int>(records.size());
        root["actions"] = arr;

        std::ofstream file(filename);
        if (file.is_open()) {
            file << Json::Stringify(Json::Value(root), 0);
        }
    }

bool JournalEngine::LoadUndoLog(const std::string& filename, std::vector<UndoRecord>& records, std::string& err) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            err = "Unable to open undo journal file: " + filename;
            return false;
        }
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Json::Parser parser(content);
        Json::Value root = parser.ParseValue();
        if (!root.IsObject() || !root.HasKey("actions")) {
            err = "Invalid or corrupted undo journal schema.";
            return false;
        }
        for (const auto& item : root["actions"].AsArray()) {
            UndoRecord rec;
            rec.op = item["op"].AsString();
            rec.path = item["path"].AsString();
            rec.valueName = item["value"].AsString();
            rec.typeStr = item["type"].AsString();
            rec.rawDataHex = item["raw_hex"].AsString();
            rec.hadPreviousValue = item["had_previous"].AsBool();
            records.push_back(rec);
        }
        return true;
    }
