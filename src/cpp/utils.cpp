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
