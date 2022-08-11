#include "transformer.h"

std::string matOps = "mov|add|sub|mul|div|and|or|xor|not|shl|shr|rol|ror";
std::string regs = "(r|e)?(?:[a-d](?:l|h|x)|(?:si|di|bp|sp)l?|ss|cs|ds|es|fs|gs)";

std::string get_full_reg(std::string reg) {
    if(reg[0] == 'r') return reg;
    if(reg[0] == 'e') return "r" + reg.substr(1);
    if(reg[1] == 'h') return string_format("r%cx", reg[0]);
    if(reg.size() == 3 && reg[2] == 'l') return "r" + reg.substr(0, reg.size()-1);
    return "r" + reg;
}

std::string get_mask(std::string reg) {
    if(reg[0] == 'r') return "0xFFFFFFFFFFFFFFFF";
    if(reg[0] == 'e') return "0xFFFFFFFF";
    if(reg.back() == 'l' || reg.back() == 'h') return "0xFF";
    return "0xFFFF";
}

bool lookahead(std::string line, std::vector<std::string> lines, int i) {
    for(std::size_t j = i; j < lines.size(); j++) {
        if(lines[j] == line) return true;
        if(lines[j].back() == ':' || lines[j] == "ret") return false;
    }
    return false;
}

int transform_code(std::vector<std::string> &lines) {
    if(lines.size() < 3) return 0;
    std::vector<std::string> transformedLines;
    int numTransformations = 0;
    bool optimizationEnabled = true;
    std::vector<std::string> removedPops;
    transformedLines.push_back(lines[0]);
    for(std::size_t i = 1; i < lines.size() - 1; i++) {
        std::string line = lines[i];
        if(!optimizationEnabled) {
            transformedLines.push_back(line);
            if(line == ";arsenic_o1") optimizationEnabled = true;
            continue;
        }
        if(line == ";arsenic_o0") {
            transformedLines.push_back(line);
            optimizationEnabled = false;
            continue;
        }

        std::string prev = lines[i - 1];
        std::string next = lines[i + 1];

        std::smatch match;

        if(std::regex_match(line, match, std::regex(string_format("(%s)\\s+(.+),\\s*(%s)", matOps.c_str(), regs.c_str())))) {
            std::string op = match[1];
            std::string dst = match[2];
            std::string src = match[3];

            if(op == "mov" && dst == src) {
                numTransformations++;
                continue;
            }

            // if(std::regex_match(src, std::regex(regs)) && std::regex_match(prev, match, std::regex(string_format("mov %s, (.+)", src.c_str())))) {
            //     transformedLines.push_back(string_format("%s %s, %s", op.c_str(), dst.c_str(), match[1].c_str()));
            // } // Could cause problems with cases where vars are expected to stay in `src`
            if(op == "mov" && dst[0] == '[' && src[0] <= '9') {
                transformedLines.push_back(string_format("mov %s, qword %s", dst.c_str(), src.c_str()));
                numTransformations++;
                continue;
            }

            if(op == "mov" && src[0] != 'r' && std::regex_match(src, std::regex(regs))) {
                transformedLines.push_back(string_format("mov %s, %s", dst.c_str(), get_full_reg(src).c_str()));
                transformedLines.push_back(string_format("and %s, %s", dst.c_str(), get_mask(src).c_str()));
                numTransformations++;
                continue;
            }

            if(op == "mov" && dst[0] != 'r' && std::regex_match(dst, std::regex(regs))) {
                transformedLines.push_back(string_format("mov %s, %s", get_full_reg(dst).c_str(), src.c_str()));
                transformedLines.push_back(string_format("and %s, %s", get_full_reg(dst).c_str(), get_mask(dst).c_str()));
                numTransformations++;
                continue;
            }
        }

        if(std::regex_match(line, match, std::regex(string_format("push (%s)", regs.c_str())))) {
            std::string reg = match[1];
            bool regModified = false;
            for(std::size_t j = i + 1; j < lines.size(); j++) {
                if(lines[j].back() == ':' || lines[j] == "ret") break;
                if(line == lines[j]) break;
                if(lines[j].find(reg) != std::string::npos || lines[j].rfind("int ", 0) == 0 || lines[j].rfind("call ", 0) == 0) {
                    regModified = true;
                    break;
                }
            }
            if(regModified) {
                transformedLines.push_back(line);
            } else if(lookahead("pop " , lines, i)) {
                removedPops.push_back("pop " + reg);
                numTransformations++;
            }
            continue;
        }

        if(removedPops.size() && removedPops.back() == line) {
            transformedLines.push_back(removedPops.back());
            removedPops.pop_back();
            numTransformations++;
            continue;
        }

        transformedLines.push_back(line);
    }
    transformedLines.push_back(lines[lines.size()-1]);

    if(removedPops.size()) {
        std::cerr << "Error: stack is likely corrupted after optimization! (Please debug the program)" << std::endl;
        exit(1);
    }

    lines = transformedLines;
    return numTransformations;
}
