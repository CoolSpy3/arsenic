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
        if(std::regex_match(line, match, std::regex("global\\s+([^\\s]+)"))) {
            std::string varName = match[1];
            variables.emplace(varName, globalVar(varName));
            definitions.push_back(string_format("%s_v%s dq 0", ctx->name.c_str(), varName.c_str()));
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
    os << "%macro pushaq 0" << std::endl;
    os << "push rax" << std::endl;
    os << "push rbx" << std::endl;
    os << "push rcx" << std::endl;
    os << "push rdx" << std::endl;
    os << "push rsi" << std::endl;
    os << "push rdi" << std::endl;
    os << "%endmacro" << std::endl;
    os << "%macro popaq 0" << std::endl;
    os << "pop rdi" << std::endl;
    os << "pop rsi" << std::endl;
    os << "pop rdx" << std::endl;
    os << "pop rcx" << std::endl;
    os << "pop rbx" << std::endl;
    os << "pop rax" << std::endl;
    os << "%endmacro" << std::endl;
}

int main(int argc, char **argv)
{
    std::vector<std::string> includePath;
    std::string makefilePath;
    bool makePhony = false;
    std::string makeTarget;
    std::string outputFile;
    std::vector<std::string> inputFiles;
    try
    {
        TCLAP::CmdLine cmd("Compiles Arsenic code to x86_64 assembly", ' ', "1");

        TCLAP::MultiArg<std::string> includePathArg("I", "include", "Specifies additional directories to search for includes", false, "path", cmd);
        TCLAP::ValueArg<std::string> makefileArg("M", "makefile", "Path to a file where the required dependencies for this file will be stored", false, "", "path", cmd);
        TCLAP::SwitchArg makePhonyArg("P", "phony", "If present with the -MF flag, the compiler will add phony tasks to the dependencies file for all required files", cmd);
        TCLAP::ValueArg<std::string> makeTargetArg("T", "target", "Changes the make dependency target. If unspecified, it will be the name of the output file", false, "", "name", cmd);
        TCLAP::ValueArg<std::string> outputArg("o", "outfile", "The path to the output file", false, "", "path", cmd);
        TCLAP::UnlabeledMultiArg<std::string> inputArg("input", "The input file(s)", true, "path", cmd);

        cmd.parse(argc, argv);

        includePath = includePathArg.getValue();
        makefilePath = makefileArg.getValue();
        makePhony = makePhonyArg.getValue();
        makeTarget = makeTargetArg.getValue();
        outputFile = outputArg.getValue();
        inputFiles = inputArg.getValue();
    }
    catch(TCLAP::ArgException &e)
    {
        std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
        exit(1);
    }

    Context rootCtx = Context{"arsenic", defaultVars(), std::map<std::string, Struct_>(), std::map<std::string, std::string>(), nullptr, nullptr, 0};
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
        makefile << std::endl;
        if(makePhony) {
            for(std::string file: inputFiles) makefile << file << ":" << std::endl;
        }
        makefile.close();
    }

    std::vector<std::string> compiledCode, definitions;

    for(std::string file: inputFiles) preprocessFile(rootCtx.root, file, rootCtx.variables, definitions);

    for(std::string file: inputFiles) compileFile(rootCtx.root, file, compiledCode, definitions);

    while(transform_code(compiledCode));

    if(!outputFile.empty()) {
        std::ofstream os(outputFile);

        os << "[bits 64]" << std::endl;
        os << "DEFAULT REL" << std::endl;

        writeQMacros(os);

        os << "arsenic:" << std::endl;

        os << "pushaq" << std::endl;
        os << "pushfq" << std::endl;

        os << string_format("enter %d, 0", 8 * stackVars(rootCtx.variables)) << std::endl;

        for (std::string line : compiledCode) os << line << std::endl;

        os << "leave" << std::endl;

        os << "popfq" << std::endl;
        os << "popaq" << std::endl;
        os << "ret" << std::endl;

        for (std::string line : definitions) os << line << std::endl;

        os.close();
    }

    return 0;
}
