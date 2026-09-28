#include "parser.hpp"

#include <cctype>
#include <sstream>

std::vector<std::string> parse_command(const std::string& line) {
    std::vector<std::string> words;
    std::istringstream stream(line);
    std::string word;
    // operator>> sam preskace razmake, tabove i '\r', i cita jednu po jednu rijec
    while (stream >> word) {
        words.push_back(word);
    }
    return words;
}

std::string to_upper(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return text;
}
