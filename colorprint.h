#ifndef COLORPRINT_H
#define COLORPRINT_H

#include <iostream>
#include <string>
#include <vector>

class Painter {
public:
    Painter(std::ostream &out,
            const std::vector<std::string> &green_words,
            const std::vector<std::string> &red_words)
        : out_(out), green_(green_words), red_(red_words) {}

    void print(const std::string &text) {
        std::string result = text;

        // highlight red words
        for (const auto &w : red_) {
            size_t pos = 0;
            while ((pos = result.find(w, pos)) != std::string::npos) {
                std::string colored = "\033[31m" + w + "\033[0m";
                result.replace(pos, w.size(), colored);
                pos += colored.size();
            }
        }

        // highlight green words
        for (const auto &w : green_) {
            size_t pos = 0;
            while ((pos = result.find(w, pos)) != std::string::npos) {
                // skip if already inside an escape sequence
                if (pos > 0 && result[pos - 1] == 'm') {
                    pos += w.size();
                    continue;
                }
                std::string colored = "\033[32m" + w + "\033[0m";
                result.replace(pos, w.size(), colored);
                pos += colored.size();
            }
        }

        // highlight numbers (cyan)
        std::string final_result;
        for (size_t i = 0; i < result.size(); i++) {
            if (result[i] == '\033') {
                // skip escape sequence
                while (i < result.size() && result[i] != 'm') {
                    final_result += result[i++];
                }
                if (i < result.size()) final_result += result[i];
            } else if (result[i] == '-' && i + 1 < result.size() && isdigit(result[i + 1])) {
                // negative number
                final_result += "\033[36m-";
                i++;
                while (i < result.size() && isdigit(result[i])) {
                    final_result += result[i++];
                }
                final_result += "\033[0m";
                i--;
            } else if (isdigit(result[i])) {
                final_result += "\033[36m";
                while (i < result.size() && isdigit(result[i])) {
                    final_result += result[i++];
                }
                final_result += "\033[0m";
                i--;
            } else {
                final_result += result[i];
            }
        }

        out_ << final_result << std::endl;
    }

private:
    std::ostream &out_;
    std::vector<std::string> green_;
    std::vector<std::string> red_;
};

#endif
