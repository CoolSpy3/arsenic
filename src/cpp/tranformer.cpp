#include "transformer.h"

std::string matOps = "mov|add|sub|mul|div|and|or|xor|not|shl|shr|rol|ror";
std::string regs = "e?(?:[a-d](?:l|h|x)|(?:si|di|bp|sp)l?|ss|cs|ds|es|fs|gs)";

std::string get_full_reg(std::string reg) {
    if(reg[0] == 'e') return reg;
    if(reg[1] == 'h') return string_format("e%cx", reg[0]);
    if(reg.size() == 3 && reg[2] == 'l') return "e" + reg.substr(0, reg.size()-1);
    return "e" + reg;
}

std::string get_mask(std::string reg) {
    if(reg[0] == 'e') return "0xFFFFFFFF";
    if(reg.back() == 'l' || reg.back() == 'h') return "0xFF";
    return "0xFFFF";
}

int transform_file(std::string fileName) {
    std::vector<std::string> lines;
    std::ifstream reader(fileName);
    std::string line;
    while (std::getline(reader, line)) lines.push_back(line);
    reader.close();

    std::vector<std::string> transformedLines;
    int numTransformations = 0;
    bool optimizationEnabled = true;
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

        if(std::regex_match(line, std::regex(string_format("(%s)\\s+(.+),\\s*(%s)", matOps, regs)))) {
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
                transformedLines.push_back(string_format("mov %s, dword %s", dst.c_str(), src.c_str()));
                numTransformations++;
                continue;
            }

            if(op == "mov" && src[0] != 'e' && std::regex_match(src, std::regex(regs))) {
                transformedLines.push_back(string_format("mov %s, %s", dst.c_str(), get_full_reg(src).c_str()));
                transformedLines.push_back(string_format("and %s, %s", dst.c_str(), get_mask(src).c_str()));
                numTransformations++;
                continue;
            }

            if(op == "mov" && dst[0] != 'e' && std::regex_match(dst, std::regex(regs))) {
                transformedLines.push_back(string_format("mov %s, %s", get_full_reg(dst).c_str(), src.c_str()));
                transformedLines.push_back(string_format("and %s, %s", get_full_reg(dst).c_str(), get_mask(dst).c_str()));
                numTransformations++;
                continue;
            }
        }
        transformedLines.push_back(line);
    }

    std::ofstream writer(fileName);
    for(std::string line : transformedLines) writer << line << std::endl;
    writer.close();
    return 0;
}
