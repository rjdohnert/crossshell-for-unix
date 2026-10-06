#include "stream_tailer.hpp"
#include "tail_output.hpp"

bool StreamTailer::TailLines(std::istream& in, long long count, bool from_start) {
        if (from_start) {
            long long line_no = 1;
            std::string line;
            while (std::getline(in, line)) {
                if (line_no >= count) {
                    std::string out = line + "\n";
                    TailOutput::WriteBytes(out.data(), out.size());
                }
                line_no++;
            }
            return true;
        }

        if (count <= 0) return true;

        std::deque<std::string> ring;
        std::string line;
        while (std::getline(in, line)) {
            ring.push_back(line);
            if (static_cast<long long>(ring.size()) > count) {
                ring.pop_front();
            }
        }

        for (const auto& item : ring) {
            std::string out = item + "\n";
            TailOutput::WriteBytes(out.data(), out.size());
        }
        return true;
    }

bool StreamTailer::TailBytes(std::istream& in, long long count, bool from_start) {
        if (from_start) {
            long long byte_no = 1;
            char ch = 0;
            while (in.get(ch)) {
                if (byte_no >= count) {
                    TailOutput::WriteBytes(&ch, 1);
                }
                byte_no++;
            }
            return true;
        }

        if (count <= 0) return true;

        std::deque<char> ring;
        char ch = 0;
        while (in.get(ch)) {
            ring.push_back(ch);
            if (static_cast<long long>(ring.size()) > count) {
                ring.pop_front();
            }
        }

        std::vector<char> buffer(ring.begin(), ring.end());
        TailOutput::WriteBytes(buffer.data(), buffer.size());
        return true;
    }
