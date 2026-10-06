#include "line_group.hpp"
#include "line_reader.hpp"
#include "stream_manager.hpp"
#include "uniq_engine.hpp"
#include "uniq_options.hpp"

UniqEngine::UniqEngine(UniqOptions opts)
        : options(std::move(opts)),
          comparator(options.ignoreCase, options.skipFields, options.skipChars, options.checkChars),
          delimiter(options.zeroTerminated ? '\0' : '\n') {}

int UniqEngine::execute() {
        StreamManager::configurePipes();

        std::unique_ptr<std::ifstream> fileIn;
        std::istream* inStream = &std::cin;

        if (!options.inputPath.empty() && options.inputPath != "-") {
            fileIn = std::make_unique<std::ifstream>(options.inputPath, std::ios::binary);
            if (!fileIn->is_open()) {
                std::cerr << "uniq: cannot open '" << options.inputPath << "': No such file\n";
                return 1;
            }
            inStream = fileIn.get();
        }

        std::unique_ptr<std::ofstream> fileOut;
        std::ostream* outStream = &std::cout;

        if (!options.outputPath.empty() && options.outputPath != "-") {
            fileOut = std::make_unique<std::ofstream>(options.outputPath, std::ios::binary);
            if (!fileOut->is_open()) {
                std::cerr << "uniq: cannot create '" << options.outputPath << "': Permission denied\n";
                return 1;
            }
            outStream = fileOut.get();
        }

        LineReader reader(*inStream, options.zeroTerminated);
        std::string currentLine;

        if (!reader.readNextLine(currentLine)) {
            return 0; // Empty input
        }

        LineGroup currentGroup;
        currentGroup.reset(std::move(currentLine));

        while (reader.readNextLine(currentLine)) {
            if (comparator.areEqual(currentGroup.representativeLine, currentLine)) {
                currentGroup.add(std::move(currentLine));
            } else {
                emitGroup(currentGroup, *outStream);
                currentGroup.reset(std::move(currentLine));
            }
        }

        emitGroup(currentGroup, *outStream);
        outStream->flush();

        return 0;
    }

void UniqEngine::emitGroup(const LineGroup& group, std::ostream& out) {
        if (options.groupMode != GroupMode::None) {
            if (!hasEmittedFirstGroup) {
                if (options.groupMode == GroupMode::Prepend || options.groupMode == GroupMode::Both) {
                    out << delimiter;
                }
            } else {
                if (options.groupMode == GroupMode::Separate || options.groupMode == GroupMode::Both) {
                    out << delimiter;
                }
            }
        }

        hasEmittedFirstGroup = true;

        if (options.allRepeated != AllRepeatedMode::None || (options.repeatedOnly && group.allLines.size() > 1 && !options.count && !options.uniqueOnly && options.allRepeated != AllRepeatedMode::None)) {
            if (group.count > 1) {
                if (options.allRepeated == AllRepeatedMode::Prepend || options.allRepeated == AllRepeatedMode::Separate) {
                    out << delimiter;
                }
                for (const auto& line : group.allLines) {
                    out << line << delimiter;
                }
            }
            return;
        }

        if (options.uniqueOnly && group.count > 1) return;
        if (options.repeatedOnly && group.count < 2) return;

        if (options.count) {
            out << std::setw(7) << group.count << " " << group.representativeLine << delimiter;
        } else {
            out << group.representativeLine << delimiter;
        }

        if (options.groupMode == GroupMode::Append || options.groupMode == GroupMode::Both) {
            out << delimiter;
        }
    }
