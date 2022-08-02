#include "compiler.h"
#include "transformer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>

std::string remove_comments(std::string str) {
    int lastIdx = str.length();
    bool quotes = false;
    for(std::size_t i = 0; i < str.length(); i++) {
        if(str[i] == '"' || str[i] == '\'') {
            quotes = !quotes;
        }
        if(quotes && str[i] == '\\') {
            i++;
            continue;
        }
        if(';' == str[i] && !quotes) {
            lastIdx = i;
            break;
        }
    }
    return str.substr(0, lastIdx);
}

void compileFile(
    std::shared_ptr<Context> ctx,
    std::string fileName,
    std::vector<std::string> &compiledCode,
    std::vector<std::string> &definitions
) {
    std::ifstream file(fileName);
    std::string line;
    std::function<std::unique_ptr<std::string>()> getLine = [&]() {
        return std::unique_ptr<std::string>(std::getline(file, line) ? new std::string(remove_comments(line)) : nullptr);
    };
    while (std::getline(file, line)) compileLine(ctx, line, getLine, compiledCode, definitions, file);
    file.close();
}

void recursivelyCompileDirectory(
    std::shared_ptr<Context> ctx,
    std::string dir,
    std::vector<std::string> &compiledCode,
    std::vector<std::string> &definitions
) {
    for (auto &entry : std::filesystem::directory_iterator(dir))
        if (entry.is_directory()) recursivelyCompileDirectory(ctx, entry.path().string(), compiledCode, definitions);
        else compileFile(ctx, entry.path().string(), compiledCode, definitions);
}

void preprocessFile(
    std::shared_ptr<Context> ctx,
    std::string fileName,
    std::vector<std::string> &variables
) {
    std::ifstream file(fileName);
    std::string line;
    std::function<std::unique_ptr<std::string>()> getLine = [&]() {
        return std::unique_ptr<std::string>(std::getline(file, line) ? new std::string(remove_comments(line)) : nullptr);
    };
    std::vector<std::string> nVariables;
    while (std::getline(file, line)) preprocessFunction(ctx, 0, getLine, file);
    variables.insert(variables.end(), nVariables.begin(), nVariables.end());
    file.close();
}

void recursivelyPreprocessDirectory(
    std::shared_ptr<Context> ctx,
    std::string dir,
    std::vector<std::string> &variables
) {
    for (auto &entry : std::filesystem::directory_iterator(dir))
        if (entry.is_directory()) recursivelyPreprocessDirectory(ctx, entry.path().string(), variables);
        else preprocessFile(ctx, entry.path().string(), variables);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cout << "Usage: arsenic <directory> <output file>" << std::endl;
        return 1;
    }
    std::string dir = argv[1];
    std::string out = argv[2];

    Context rootCtx = Context{"arsenic", std::vector<std::string>(), std::vector<std::string>(), std::map<std::string, std::string>(), nullptr, nullptr};
    rootCtx.root = std::make_shared<Context>(rootCtx);

    rootCtx.variables.push_back(".spr");
    rootCtx.variables.push_back(".arg");
    rootCtx.variables.push_back(".ret");

    recursivelyPreprocessDirectory(rootCtx.root, dir, rootCtx.variables);

    std::vector<std::string> compiledCode, definitions;

    recursivelyCompileDirectory(rootCtx.root, dir, compiledCode, definitions);
    compiledCode.insert(compiledCode.begin(), definitions.begin(), definitions.end());

    std::ofstream os(out);

    os << "arsenic:" << std::endl;

    os << "pushad" << std::endl;
    os << "pushfd" << std::endl;

    os << "push ebp" << std::endl;
    os << string_format("mov eax, %d", 4 * rootCtx.variables.size()) << std::endl;
    os << "call malloc" << std::endl;
    os << "mov ebp, eax" << std::endl;

    for (std::string line : compiledCode) os << line << std::endl;

    os << "mov eax, ebp" << std::endl;
    os << "call free" << std::endl;
    os << "pop ebp" << std::endl;

    os << "popfd" << std::endl;
    os << "popad" << std::endl;

    os.close();

    while(transform_file(out));

    return 0;
}
