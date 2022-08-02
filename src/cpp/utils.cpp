#include "utils.h"

bool matchingBrackets(std::string str) {
    std::string brackets;
    for(char c : str) {
        if(c == '(' || c == '[') brackets += c;
        else if(c == ')' || c == ']') {
            if(brackets.empty()) return false;
            if(brackets.back() == '(' && c == ')') brackets.pop_back();
            else if(brackets.back() == '[' && c == ']') brackets.pop_back();
            else return false;
        }
    }
    return brackets.size() == 0;
}

void string_replace(std::string str, std::string search, std::string replace) {
    size_t pos = 0;
    while ((pos = str.find(search, pos)) != std::string::npos) {
        str.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

std::size_t find_not_in_brackets(std::string str, std::string find) {
    std::size_t loc = 0;
    int numBrackets = 0;
    for(char c: str) {
        if(c == '(' || c == '[') numBrackets++;
        else if(c == ')' || c == ']') numBrackets--;
        else if(c == find[0] && numBrackets == 0) {
            std::size_t pos = str.find(find, loc);
            if(pos == std::string::npos) return std::string::npos;
            if(pos == loc) return loc;
        }
        loc++;
    }
    return std::string::npos;
}

std::vector<std::string> split(std::string str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}
