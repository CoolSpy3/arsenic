#include "compiler.h"
#include "transformer.h"
#include "tclap/CmdLine.h"
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
    while (std::getline(file, line)) compileLine(ctx, remove_comments(line), getLine, compiledCode, definitions, file);
    file.close();
}

void preprocessFile(
    std::shared_ptr<Context> ctx,
    std::string fileName,
    std::map<std::string, Variable> &variables,
    std::vector<std::string> &definitions
) {
    std::ifstream file(fileName);
    std::string line;
    std::function<std::unique_ptr<std::string>()> getLine = [&]() {
        return std::unique_ptr<std::string>(std::getline(file, line) ? new std::string(remove_comments(line)) : nullptr);
    };
    while (std::getline(file, line)) {
        line = remove_comments(line);
        trim(line);
        std::smatch match;
        if(std::regex_match(line, match, std::regex("const\\s+([^\\s]+)\\s*=\\s*(.+)"))) {
            std::string varName = match[1];
            std::string varValue = match[2];
            variables.emplace(varName, constVar(varName));
            definitions.push_back(string_format("%s_v%s equ %s", ctx->name.c_str(), varName.c_str(), varValue.c_str()));
            return;
        }
        if(std::regex_match(line, match, std::regex("global\\s+(byte|word|dword|qword)\\s+([^\\s=>]+)(?:\\s*[=>].*)"))) {
            std::string varSize = match[1];
            std::string varName = match[2];
            int size = getVarSize(varSize);
            variables.emplace(varName, globalVar(varName, size));
            definitions.push_back(string_format("%s_v%s %s 0", ctx->name.c_str(), varName.c_str(), getGlobalSize(size).c_str()));
            return;
        }
    }
    file.clear();
    file.seekg(0);
    std::map<std::string, Variable> nVariables = preprocessFunction(ctx, 0, getLine, file);
    variables.insert(nVariables.begin(), nVariables.end());
    file.close();
}

void getIncludes(std::string file, std::vector<std::string> &includes)
{
    std::ifstream f(file);
    std::string line;
    while (std::getline(f, line)) {
        line = remove_comments(line);
        std::smatch match;
        if (std::regex_search(line, match, std::regex("#include\\s+\"(.*)\""))) {
            includes.push_back(match[1]);
        }
    }
    f.close();
}

void writeQMacros(std::ostream &os) {
    os << "%macro pushaq 0\n";
    os << "push rax\n";
    os << "push rbx\n";
    os << "push rcx\n";
    os << "push rdx\n";
    os << "push r8\n";
    os << "push r9\n";
    os << "push r10\n";
    os << "push r11\n";
    os << "push r12\n";
    os << "push r13\n";
    os << "push r14\n";
    os << "push r15\n";
    os << "push rsi\n";
    os << "push rdi\n";
    os << "%endmacro\n";
    os << "%macro popaq 0\n";
    os << "pop rdi\n";
    os << "pop rsi\n";
    os << "pop r15\n";
    os << "pop r14\n";
    os << "pop r13\n";
    os << "pop r12\n";
    os << "pop r11\n";
    os << "pop r10\n";
    os << "pop r9\n";
    os << "pop r8\n";
    os << "pop rdx\n";
    os << "pop rcx\n";
    os << "pop rbx\n";
    os << "pop rax\n";
    os << "%endmacro\n";
}

int main(int argc, char **argv)
{
    std::vector<std::string> includePath;
    std::string makefilePath;
    bool makePhony = false;
    std::string makeTarget;
    std::string symbolFile;
    std::string outputFile;
    std::vector<std::string> inputFiles;
    try
    {
        TCLAP::CmdLine cmd("Compiles Arsenic code to x86_64 assembly", ' ', "1");

        TCLAP::MultiArg<std::string> includePathArg("I", "include", "Specifies additional directories to search for includes", false, "path", cmd);
        TCLAP::ValueArg<std::string> makefileArg("M", "makefile", "Path to a file where the required dependencies for this file will be stored", false, "", "path", cmd);
        TCLAP::SwitchArg makePhonyArg("P", "phony", "If present with the -MF flag, the compiler will add phony tasks to the dependencies file for all required files", cmd);
        TCLAP::ValueArg<std::string> makeTargetArg("T", "target", "Changes the make dependency target. If unspecified, it will be the name of the output file", false, "", "name", cmd);
        TCLAP::ValueArg<std::string> symbolOutputArg("S", "symbols", "Specifies an output file to write symbol locations", false, "", "path", cmd);
        TCLAP::ValueArg<std::string> outputArg("o", "outfile", "The path to the output file", false, "", "path", cmd);
        TCLAP::UnlabeledMultiArg<std::string> inputArg("input", "The input file(s)", true, "path", cmd);

        cmd.parse(argc, argv);

        includePath = includePathArg.getValue();
        makefilePath = makefileArg.getValue();
        makePhony = makePhonyArg.getValue();
        makeTarget = makeTargetArg.getValue();
        symbolFile = symbolOutputArg.getValue();
        outputFile = outputArg.getValue();
        inputFiles = inputArg.getValue();
    }
    catch(TCLAP::ArgException &e)
    {
        std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
        exit(1);
    }

    Context rootCtx = Context{"arsenic", defaultVars(), std::map<std::string, Struct_>(), std::map<std::string, std::string>(), nullptr, nullptr, 0, 1};
    rootCtx.root = std::make_shared<Context>(rootCtx);

    includePath.push_back(".");

    std::vector<std::filesystem::path> transformedIncludePath(includePath.size());

    std::transform(includePath.begin(), includePath.end(), transformedIncludePath.begin(), [](std::string path) {
        std::filesystem::path nPath = std::filesystem::path(path);
        if(!std::filesystem::exists(path)) {
            std::cerr << "Cannot find input file: " << path << std::endl;
            exit(1);
        }
        return nPath;
    });

    std::vector<std::string> includeSearchFiles(inputFiles);

    while(!includeSearchFiles.empty()) {
        std::vector<std::string> includes;
        for(std::string file: includeSearchFiles) getIncludes(file, includes);
        std::transform(includes.begin(), includes.end(), includes.begin(), [&includePath = transformedIncludePath](std::string file) {
            for(std::filesystem::path dir: includePath) {
                std::filesystem::path fullPath = dir / file;
                if(std::filesystem::exists(fullPath)) return fullPath.string();
            }
            std::cerr << "Cannot find included file: " << file << std::endl;
            exit(1);
        });
        includes.erase(std::unique(includes.begin(), includes.end()), includes.end());
        inputFiles.insert(inputFiles.end(), includes.begin(), includes.end());
        includeSearchFiles = includes;
    }

    inputFiles.erase(std::unique(inputFiles.begin(), inputFiles.end()), inputFiles.end());

    if(!makefilePath.empty()) {
        std::ofstream makefile(makefilePath);
        if(makeTarget.empty()) makeTarget = std::filesystem::path(outputFile).filename().string();
        makefile << makeTarget << ":";
        for(std::string file: inputFiles) makefile << " " << file;
        makefile << "\n";
        if(makePhony) {
            for(std::string file: inputFiles) makefile << file << ":\n";
        }
        makefile.close();
    }

    std::vector<std::string> compiledCode, definitions;

    for(std::string file: inputFiles) preprocessFile(rootCtx.root, file, rootCtx.variables, definitions);

    for(std::string file: inputFiles) compileFile(rootCtx.root, file, compiledCode, definitions);

    while(transform_code(compiledCode));

    if(!outputFile.empty()) {
        std::ofstream os(outputFile);

        if(!symbolFile.empty()) os << string_format("[map symbols %s]\n", symbolFile.c_str());

        os << "[bits 64]\n";
        os << "DEFAULT REL\n";

        writeQMacros(os);

        os << "arsenic:\n";

        os << string_format("enter %d, 0\n", stackSize(rootCtx.variables));

        os << "pushaq\n";
        os << "pushfq\n";

        for (std::string line : compiledCode) os << line << "\n";

        os << "popfq\n";
        os << "popaq\n";

        os << "leave\n";

        os << "ret\n";

        for (std::string line : definitions) os << line << "\n";

        os.close();
    }

    return 0;
}
