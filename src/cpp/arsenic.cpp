#include "compiler.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>

void compileFile(
    Context ctx,
    std::string fileName,
    std::vector<std::string> &compiledCode,
    std::vector<std::string> &definitions)
{
    std::ifstream file(fileName);
    std::string line;
    std::function<std::unique_ptr<std::string>()> getLine = [&]() {
        return std::make_unique<std::string>(std::getline(file, line) ? line : NULL);
    };
    while (std::getline(file, line)) compileLine(ctx, line, getLine, compiledCode, definitions);
    file.close();
}

void recursivelyCompileDirectory(
    Context ctx,
    std::string dir,
    std::vector<std::string> &compiledCode,
    std::vector<std::string> &definitions
) {
    for (auto &entry : std::filesystem::directory_iterator(dir))
    {
        if (entry.is_directory())
        {
            recursivelyCompileDirectory(ctx, entry.path().string(), compiledCode, definitions);
        }
        else
        {
            compileFile(ctx, entry.path().string(), compiledCode, definitions);
        }
    }
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

    Context rootCtx = new Context_{"arsenic_", std::vector<std::string>(), nullptr, nullptr};
    rootCtx->root = rootCtx;

    std::vector<std::string> compiledCode, definitions;
    recursivelyCompileDirectory(rootCtx, dir, compiledCode, definitions);
    compiledCode.insert(compiledCode.begin(), definitions.begin(), definitions.end());

    std::ofstream os(out);

    os << "pushad" << std::endl;
    os << "pushfd" << std::endl;

    for (std::string line : compiledCode)
    {
        if(std::regex_match(line, std::regex("mov (?<register>e?([a-d](l|h|x)|si|di)),\\s*\\k<register>"))) continue;
        os << line << std::endl;
    }

    os << "popfd" << std::endl;
    os << "popad" << std::endl;

    os.close();

    delete rootCtx;

    return 0;
}