#include "compiler.h"

#define CHECK_SPECIAL_VARS(var) var == "args" ? ".arg" : var == "return" ? ".ret" : var

bool reg_match(std::string reg, char match) {
    return std::regex_match(reg, std::regex(string_format("e?%c(l|h|x)", match)));
}

void resolve_argument_a(
    std::shared_ptr<Context> ctx,
    std::string var,
    std::string reg,
    std::vector<std::string>& compiledCode,
    int numParents = 0
) {
    var = CHECK_SPECIAL_VARS(var);
    std::vector<std::string>::iterator it = std::find(ctx->variables.begin(), ctx->variables.end(), var);
    if(it == ctx->variables.end()) {
        if(ctx->parent) {
            resolve_argument_a(ctx->parent, var, reg, compiledCode, numParents + 1);
        } else {
            std::cerr << "Error: variable " << var << " not found" << std::endl;
            exit(1);
        }
    } else {
        if(!reg_match(reg, 'a')) compiledCode.push_back("push eax");
        compiledCode.push_back("mov eax, [ebp]");
        if(var == ".ret") compiledCode.push_back("mov eax, [eax]");
        for(int i = 0; i < numParents; i++) compiledCode.push_back("mov eax, [eax]");
        compiledCode.push_back(string_format("add eax, %d", 4 * std::distance(ctx->variables.begin(), it)));
        compiledCode.push_back(string_format("mov %s, eax", reg.c_str()));
        if(!reg_match(reg, 'a')) compiledCode.push_back("pop eax");
    }
}

void resolve_argument_i(
    std::shared_ptr<Context> ctx,
    std::string var,
    std::string reg,
    std::vector<std::string> &compiledCode,
    int numParents = 0
) {
    trim(var);
    if(('0' <= var[0] && var[0] <= '9') || var[0] == '\'') {
        compiledCode.push_back(string_format("mov %s, %s", reg.c_str(), var.c_str()));
        return;
    }
    if(var[0] == '(' || var[0] == '[') {
        resolve_argument(ctx, var.substr(1, var.size() - 2), reg, compiledCode);
        return;
    }
    var = CHECK_SPECIAL_VARS(var);
    std::vector<std::string>::iterator it = std::find(ctx->variables.begin(), ctx->variables.end(), var);
    if(it == ctx->variables.end()) {
        if(ctx->parent) {
            resolve_argument_i(ctx->parent, var, reg, compiledCode, numParents + 1);
        } else {
            std::cerr << "Error: variable " << var << " not found" << std::endl;
            exit(1);
        }
    } else {
        if(!reg_match(reg, 'a')) compiledCode.push_back("push eax");
        compiledCode.push_back("mov eax, [ebp]");
        if(var == ".ret") compiledCode.push_back("mov eax, [eax]");
        for(int i = 0; i < numParents; i++) compiledCode.push_back("mov eax, [eax]");
        compiledCode.push_back(string_format("mov eax, [eax+%d]", 4 * std::distance(ctx->variables.begin(), it)));
        compiledCode.push_back(string_format("mov %s, eax", reg.c_str()));
        if(!reg_match(reg, 'a')) compiledCode.push_back("pop eax");
    }
}

#undef CHECK_SPECIAL_VARS

void resolve_argument_p(
    std::shared_ptr<Context> ctx,
    std::string var,
    std::string reg,
    std::vector<std::string> &compiledCode,
    std::vector<std::string> &definitions,
    int numParents = 0
) {
    trim(var);
    if(var[0] == '{') {
        if(!reg_match(reg, 'a')) compiledCode.push_back("push eax");
        std::string elems = var.substr(1, var.size() - 2);
        int numElems = std::count(elems.begin(), elems.end(), ',') + 1;
        compiledCode.push_back(string_format("mov eax, %d ", 4 * numElems));
        compiledCode.push_back("call malloc");
        for(int i = 0; i < numElems; i++) {
            int elemLen = elems.find(',');
            std::string elem = elems.substr(0, elemLen);
            elems = elems.substr(elemLen + 1);
            resolve_argument(ctx, elem, "ebx", compiledCode);
            compiledCode.push_back(string_format("mov [eax+%d], ebx", 4 * i));
        }
        if(!reg_match(reg, 'a')) {
            compiledCode.push_back(string_format("mov %s, eax", reg.c_str()));
            compiledCode.push_back("pop eax");
        }
        return;
    }
    if(var[0] == '"') {
        int len = var.size() - 2 + 1;
        int id = definitions.size();
        std::string svar = string_format("arsenic_s%d", id);
        definitions.push_back(string_format("%s db %s, 0", svar.c_str(), var.c_str()));
        compiledCode.push_back("push esi");
        compiledCode.push_back("push edi");
        compiledCode.push_back("push ecx");
        if(!reg_match(reg, 'a')) compiledCode.push_back("push eax");

        compiledCode.push_back(string_format("mov esi, %s", svar.c_str()));
        compiledCode.push_back(string_format("mov eax, %d", len));
        compiledCode.push_back("call malloc");
        compiledCode.push_back("mov edi, eax");
        compiledCode.push_back(string_format("mov ecx, %d", len));

        compiledCode.push_back("rep movsb");

        compiledCode.push_back("pop ecx");
        compiledCode.push_back("pop edi");
        compiledCode.push_back("pop esi");

        compiledCode.push_back(string_format("mov %s, edi", reg.c_str()));
        if(!reg_match(reg, 'a')) compiledCode.push_back("pop eax");
        return;
    }

    std::smatch emptyArrayMatch;
    if(std::regex_match(var, emptyArrayMatch, std::regex("(.+){}"))) {
        if(!reg_match(reg, 'a')) compiledCode.push_back("push eax");
        resolve_argument(ctx, emptyArrayMatch[1], "eax", compiledCode);
        compiledCode.push_back("call malloc");
        compiledCode.push_back(string_format("mov %s, eax", reg.c_str()));
        if(!reg_match(reg, 'a')) compiledCode.push_back("pop eax");
        return;
    }

    if(!reg_match(reg, 'a')) compiledCode.push_back("push eax");
    compiledCode.push_back("mov eax, 4");
    compiledCode.push_back("call malloc");
    compiledCode.push_back("push ebx");
    resolve_argument(ctx, var, "ebx", compiledCode);
    compiledCode.push_back("mov [eax], ebx");
    compiledCode.push_back("pop ebx");

    if(!reg_match(reg, 'a')) {
        compiledCode.push_back(string_format("mov %s, eax", reg.c_str()));
        compiledCode.push_back("pop eax");
    }
}

bool resolve_argument_o(
    std::shared_ptr<Context> ctx,
    std::string var,
    std::string reg,
    std::string operation,
    std::function<void(std::vector<std::string>&)> printAsm,
    std::vector<std::string> &compiledCode
) {
    trim(var);
    std::size_t idx;
    std::string scanVar = var;
    if(ends_with(scanVar, "++") || ends_with(scanVar, "--")) {
        scanVar = scanVar.substr(0, scanVar.size() - 2);
    }
    if(scanVar.rfind("++", 0) == 0 || scanVar.rfind("--", 0) == 0) {
        scanVar = scanVar.substr(2);
        idx = find_not_in_brackets(scanVar, operation) + 2;
    } else {
        idx = find_not_in_brackets(scanVar, operation);
    }
    if(idx == std::string::npos) return false;
    std::string left = var.substr(0, idx);
    std::string right = var.substr(idx + operation.size());
    if(!reg_match(reg, 'a')) compiledCode.push_back("push eax");
    if(!reg_match(reg, 'b')) compiledCode.push_back("push ebx");
    resolve_argument(ctx, left, "eax", compiledCode);
    resolve_argument(ctx, right, "ebx", compiledCode);
    printAsm(compiledCode);
    compiledCode.push_back(string_format("mov %s, eax", reg.c_str()));
    if(!reg_match(reg, 'b')) compiledCode.push_back("pop ebx");
    if(!reg_match(reg, 'a')) compiledCode.push_back("pop eax");
    return true;
}

void resolve_argument(
    std::shared_ptr<Context> ctx,
    std::string var,
    std::string reg,
    std::vector<std::string> &compiledCode
) {
    trim(var);

    // Grouping

    #define PARSE_GROUPING(charsBefore, charsAfter) \
        resolve_argument(ctx, var.substr(charsBefore, var.size() - (charsAfter + charsBefore)), reg.c_str(), compiledCode); \
        if(var[0] == '[') compiledCode.push_back(string_format("mov %s, [%s]", reg.c_str(), reg.c_str()));

    // Postfix

    if(ends_with(var, "++") || ends_with(var, "--")) {
        PARSE_GROUPING(0, 2);
        if(ends_with(var, "++")) compiledCode.push_back(string_format("inc %s", reg.c_str()));
        else compiledCode.push_back(string_format("dec %s", reg.c_str()));
        return;
    }

    // Prefix

    if(var.rfind("++", 0) == 0 || var.rfind("--", 0) == 0) {
        if(((var[2] == '(' && var.back() == ')') || (var[2] == '[' && var.back() == ']')) && matchingBrackets(var.substr(2, var.size() - 4))) {
            PARSE_GROUPING(3, 1);
            if(var.rfind("++", 0) == 0) compiledCode.push_back(string_format("inc %s", reg.c_str()));
            else compiledCode.push_back(string_format("dec %s", reg.c_str()));
            return;
        }
    }
    if(var[0] == '~' || var[0] == '!') {
        if(((var[1] == '(' && var.back() == ')') || (var[1] == '[' && var.back() == ']')) && matchingBrackets(var.substr(1, var.size() - 3))) {
            PARSE_GROUPING(2, 1);
            string_format("not %s", reg.c_str());
            return;
        }
    }

    if(((var[0] == '(' && var.back() == ')') || (var[0] == '[' && var.back() == ']')) && matchingBrackets(var.substr(1, var.size() - 2))) {
        PARSE_GROUPING(1, 1);
        return;
    }

    #undef PARSE_GROUPING

    // List in order of reverse precedence

    if(resolve_argument_o(ctx, var, reg, "|", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("or eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "^", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("xor eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "&", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("and eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "!=", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp eax, ebx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr eax, 14"); // 6 + 8 = 14
        compiledCode.push_back("not eax");
        compiledCode.push_back("and eax, 1");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "==", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp eax, ebx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr eax, 14"); // 6 + 8 = 14
        compiledCode.push_back("and eax, 1");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, ">=", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp eax, ebx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr eax, 8");
        compiledCode.push_back("mov ebx, eax");
        compiledCode.push_back("shr eax, 6"); // 6 + 8 = 14
        compiledCode.push_back("not eax");
        compiledCode.push_back("and eax, 1");
        compiledCode.push_back("and ebx, 1");
        compiledCode.push_back("or eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "<=", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp eax, ebx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr eax, 8");
        compiledCode.push_back("mov ebx, eax");
        compiledCode.push_back("shr eax, 6"); // 6 + 8 = 14
        compiledCode.push_back("and eax, 1");
        compiledCode.push_back("and ebx, 1");
        compiledCode.push_back("or eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, ">", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp eax, ebx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr eax, 8");
        compiledCode.push_back("mov ebx, eax");
        compiledCode.push_back("shr eax, 6"); // 6 + 8 = 14
        compiledCode.push_back("not eax");
        compiledCode.push_back("and eax, 1");
        compiledCode.push_back("not ebx");
        compiledCode.push_back("and ebx, 1");
        compiledCode.push_back("and eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "<", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp eax, ebx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr eax, 8");
        compiledCode.push_back("and eax, 1");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, ">>", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("shr eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "<<", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("shl eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "-", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("sub eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "+", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("add eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "%", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("push edx");
        compiledCode.push_back("div ebx");
        compiledCode.push_back("mov eax, edx");
        compiledCode.push_back("pop edx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "/", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("push edx");
        compiledCode.push_back("div ebx");
        compiledCode.push_back("pop edx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "*", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("push edx");
        compiledCode.push_back("mul ebx");
        compiledCode.push_back("pop edx");
    }, compiledCode)) return;

    if(var.rfind("++", 0) == 0 || var.rfind("--", 0) == 0) {
        resolve_argument_i(ctx, var.substr(2), reg, compiledCode);
        if(var.rfind("++", 0) == 0) compiledCode.push_back(string_format("inc %s", reg));
        else compiledCode.push_back(string_format("dec %s", reg.c_str()));
        return;
    }

    if(ends_with(var, "++") || ends_with(var, "--")) {
        resolve_argument_i(ctx, var.substr(0, var.size() - 2), reg, compiledCode);
        if(ends_with(var, "++")) compiledCode.push_back(string_format("inc %s", reg));
        else compiledCode.push_back(string_format("dec %s", reg.c_str()));
        return;
    }

    resolve_argument_i(ctx, var, reg, compiledCode);
}

int calculateIndentation(std::string line)
{
    int indentation = 0;
    for (char c : line)
    {
        if (c == ' ') indentation++;
        else if (c == '\t') indentation += 4;
        else break;
    }
    return indentation;
}

void writeFunctionExit(std::vector<std::string> &compiledCode)
{
    if(compiledCode.back() == "ret") return;
    compiledCode.push_back("push eax");
    compiledCode.push_back("mov eax, ebp");
    compiledCode.push_back("call free");
    compiledCode.push_back("pop eax");
    compiledCode.push_back("pop ebp");
    compiledCode.push_back("ret");
}

std::vector<std::string> preprocessFunction(
    std::shared_ptr<Context> ctx,
    int indentation,
    std::function<std::unique_ptr<std::string>()> getLine,
    std::ifstream &file
) {
    std::vector<std::string> variables;
    variables.push_back(".spr");
    variables.push_back(".arg");
    variables.push_back(".ret");
    int posBkp = file.tellg();

    for(;;) {
        std::unique_ptr<std::string> linePtr = getLine();
        if(!linePtr) break;
        std::string line = *linePtr;
        if(indentation >= calculateIndentation(line)) break;
        if(trim_copy(line).empty()) continue;
        if(std::regex_match(line, std::regex("\\s*([^\\s]+)\\s*:"))) {
            int innerIndentation = calculateIndentation(line);
            int posBkp2 = file.tellg();
            while(linePtr && !linePtr->empty() && calculateIndentation(*linePtr) > innerIndentation) {
                posBkp2 = file.tellg();
                linePtr = getLine();
            }
            file.clear();
            file.seekg(posBkp2);
            continue;
        }
        trim(line);
        std::smatch varMatch;
        if(std::regex_match(line, varMatch, std::regex("([^\\s]+)\\s*(=|<|>)\\s*.+"))) {
            std::string var = varMatch[1];
            if(var[0] <= '9') continue;
            if(var == "return" || var == "args") continue;
            Context superCtx = *ctx;
            if(superCtx.parent == nullptr) {
                variables.push_back(var);
                break;
            } else {
                while(superCtx.parent) {
                    if(std::count(superCtx.variables.begin(), superCtx.variables.end(), var)) break;
                    if(superCtx.parent == nullptr) {
                        variables.push_back(var);
                        break;
                    }
                    superCtx = *superCtx.parent;
                }
            }
        } else if (std::regex_match(line, varMatch, std::regex("\\[[^\\s]+\\]"))) {
            std::string var = varMatch[1];
            if(var[0] <= '9') continue;
            if(var == "return" || var == "args") continue;
            Context superCtx = *ctx;
            while(superCtx.parent) {
                if(std::count(superCtx.variables.begin(), superCtx.variables.end(), var)) break;
                if(superCtx.parent == nullptr) {
                    variables.push_back(var);
                    break;
                }
                superCtx = *superCtx.parent;
            }
        }
    }

    file.clear();
    file.seekg(posBkp);
    return variables;
}

std::string allocateLabel(
    std::string requestedName,
    std::shared_ptr<Context> ctx
) {
    int i = 0;
    while(ctx->functions.count(requestedName + std::to_string(i))) i++;
    return requestedName + std::to_string(i);
}

void compileLine(
    std::shared_ptr<Context> ctx,
    std::string line,
    std::function<std::unique_ptr<std::string>()> getLine,
    std::vector<std::string>& compiledCode,
    std::vector<std::string>& definitions,
    std::ifstream &file
) {
    int indentation = calculateIndentation(line);
    trim(line);
    if(line.empty()) return;
    if(std::regex_match(line, std::regex("asm\\s*:"))) {
        for(;;) {
            std::unique_ptr<std::string> linePtr = getLine();
            if(!linePtr) break;
            line = *linePtr.get();
            if(trim_copy(line).empty()) continue;
            if(indentation >= calculateIndentation(line)) break;
            std::smatch match;
            if(std::regex_search(line, match, std::regex("\\{(.*),\\s*(eax|ebx|ecx|edx)}"))) {
                resolve_argument(ctx, match[1], match[2], compiledCode);
                std::regex_replace(line, std::regex("\\{.*,\\s*(eax|ebx|ecx|edx)\\}"), "$1");
            }
            if(std::regex_search(line, match, std::regex("\\[(.*),\\s*(eax|ebx|ecx|edx)]"))) {
                resolve_argument_a(ctx, match[1], match[2], compiledCode);
                std::regex_replace(line, std::regex("\\[[^\\[\\]]*,\\s*(eax|ebx|ecx|edx)\\]"), "$1");
            }
            trim(line);
            if(line == "O0") compiledCode.push_back(";arsenic_o0");
            else if(line == "O1") compiledCode.push_back(";arsenic_o1");
            else compiledCode.push_back(line);
        }
        if(file.good()) compileLine(ctx, line, getLine, compiledCode, definitions, file);
        return;
    }
    std::smatch match;
    if(std::regex_match(line, match, std::regex("([^\\s]+)\\s*=\\s*(.+)"))) {
        compiledCode.push_back("push eax");
        compiledCode.push_back("push ebx");

        resolve_argument_a(ctx, match[1], "eax", compiledCode);
        resolve_argument(ctx, match[2], "ebx", compiledCode);

        compiledCode.push_back("mov [eax], ebx");

        compiledCode.push_back("pop ebx");
        compiledCode.push_back("pop eax");

        if(match[1] == "return") writeFunctionExit(compiledCode);
    }
    if(std::regex_match(line, match, std::regex("([^\\s]+)\\s*>\\s*(.+)"))) {
        compiledCode.push_back("push eax");
        compiledCode.push_back("push ebx");

        resolve_argument_a(ctx, match[1], "eax", compiledCode);
        resolve_argument_p(ctx, match[2], "ebx", compiledCode, definitions);

        compiledCode.push_back("mov [eax], ebx");

        compiledCode.push_back("pop ebx");
        compiledCode.push_back("pop eax");

        if(match[1] == "return") writeFunctionExit(compiledCode);
    }
    if(std::regex_match(line, match, std::regex("([^\\s]+)\\s*<\\s*(.+)"))) {
        compiledCode.push_back("push eax");
        compiledCode.push_back("push ebx");

        resolve_argument_i(ctx, match[1], "eax", compiledCode);
        resolve_argument(ctx, match[2], "ebx", compiledCode);

        compiledCode.push_back("mov [eax], ebx");

        compiledCode.push_back("pop ebx");
        compiledCode.push_back("pop eax");
    }

    if(std::regex_match(line, match, std::regex("([^\\s]+)\\s*:"))) {
        std::string functionName = match[1];
        string_replace(functionName, std::string("_"), std::string("__"));
        std::string functionLabel = string_format("%s_f%s", ctx->name.c_str(), functionName.c_str());

        ctx->functions.emplace(functionName, functionLabel);

        std::vector<std::string> functionVars = preprocessFunction(ctx, indentation, getLine, file);

        std::shared_ptr<Context> nCtx = std::make_shared<Context>(Context{functionLabel, functionVars, std::vector<std::string>(), std::map<std::string, std::string>(), ctx, ctx->root});

        compiledCode.push_back(string_format("jmp %s_e", functionLabel.c_str()));
        compiledCode.push_back(string_format("%s:", functionLabel.c_str()));
        compiledCode.push_back("push ebp");
        compiledCode.push_back("push eax");
        compiledCode.push_back(string_format("mov eax, %d", 4 * functionVars.size()));
        compiledCode.push_back("call malloc");
        compiledCode.push_back("mov [eax], ebp");
        compiledCode.push_back("mov [eax+4], ebx");
        compiledCode.push_back("mov ebp, eax");
        compiledCode.push_back("pop eax");
        for(;;) {
            std::unique_ptr<std::string> linePtr = getLine();
            if(!linePtr) break;
            line = *linePtr.get();
            if(trim_copy(line).empty()) continue;
            if(indentation >= calculateIndentation(line)) break;
            compileLine(nCtx, line, getLine, compiledCode, definitions, file);
        }
        writeFunctionExit(compiledCode);
        compiledCode.push_back(string_format("%s_e:", functionLabel.c_str()));
        if(file.good()) compileLine(ctx, line, getLine, compiledCode, definitions, file);
    }

    if(std::regex_match(line, match, std::regex("delete\\s+([^\\s]+)"))) {
        compiledCode.push_back("push eax");
        resolve_argument_i(ctx, match[1], "eax", compiledCode);
        compiledCode.push_back("call free");
        compiledCode.push_back("pop eax");
    }

    if(std::regex_match(line, match, std::regex("if\\s+([^\\s].+)\\s*:"))) {
        std::string ifLabel = allocateLabel(string_format("%s_cif", ctx->name.c_str()), ctx);
        compiledCode.push_back(string_format("%s:", ifLabel.c_str()));
        compiledCode.push_back("push eax");
        resolve_argument(ctx, match[1], "eax", compiledCode);
        compiledCode.push_back("cmp eax, 0");
        compiledCode.push_back("pop eax");
        compiledCode.push_back(string_format("jz %s", string_format("%s_cel", ifLabel.c_str())));
        for(;;) {
            std::unique_ptr<std::string> linePtr = getLine();
            if(!linePtr) break;
            line = *linePtr.get();
            if(trim_copy(line).empty()) continue;
            if(indentation >= calculateIndentation(line)) break;
            compileLine(ctx, line, getLine, compiledCode, definitions, file);
        }
        if(std::regex_match(line, std::regex("else\\s*:"))) {
            compiledCode.push_back(string_format("jmp %s_e:", ifLabel.c_str()));
            compiledCode.push_back(string_format("%s_cel:", ifLabel.c_str()));
            for(;;) {
                std::unique_ptr<std::string> linePtr = getLine();
                if(!linePtr) break;
                line = *linePtr.get();
                if(trim_copy(line).empty()) continue;
                if(indentation >= calculateIndentation(line)) break;
                compileLine(ctx, line, getLine, compiledCode, definitions, file);
            }
        } else compiledCode.push_back(string_format("%s_cel:", ifLabel.c_str()));
        compiledCode.push_back(string_format("%s_e:", ifLabel.c_str()));
        if(file.good()) compileLine(ctx, line, getLine, compiledCode, definitions, file);
    }

    if(std::regex_match(line, match, std::regex("while\\s+([^\\s].+)\\s*:"))) {
        std::string whileLabel = allocateLabel(string_format("%s_cwhile", ctx->name.c_str()), ctx);
        compiledCode.push_back(string_format("%s:", whileLabel.c_str()));
        compiledCode.push_back("push eax");
        resolve_argument(ctx, match[1], "eax", compiledCode);
        compiledCode.push_back("cmp eax, 0");
        compiledCode.push_back("pop eax");
        compiledCode.push_back(string_format("jz %s", string_format("%s_e", whileLabel.c_str())));
        for(;;) {
            std::unique_ptr<std::string> linePtr = getLine();
            if(!linePtr) break;
            line = *linePtr.get();
            if(trim_copy(line).empty()) continue;
            if(indentation >= calculateIndentation(line)) break;
            compileLine(ctx, line, getLine, compiledCode, definitions, file);
            compiledCode.push_back(string_format("jmp %s", whileLabel.c_str()));
        }
        compiledCode.push_back(string_format("%s_e:", whileLabel.c_str()));
        if(file.good()) compileLine(ctx, line, getLine, compiledCode, definitions, file);
    }

    if(std::regex_match(line, match, std::regex("([^\\s]+)\\s*\\((.*)\\)"))) {
        std::string functionName = match[1];

        std::shared_ptr<Context> searchCtx = ctx;
        std::map<std::string, std::string>::iterator functionLabelIttr;
        do {
            if((functionLabelIttr = searchCtx->functions.find(functionName)) != searchCtx->functions.end()) break;
            searchCtx = searchCtx->parent;
        } while(searchCtx->parent);

        std::string functionLabel = functionLabelIttr->second;

        std::string rawArgs = match[2];

        std::vector<std::string> args = split(rawArgs, ',');
        if(args.size() > 0) {
            std::transform(args.begin(), args.end(), args.begin(), [](std::string arg) {
                return trim_copy(arg);
            });
            compiledCode.push_back("push ebx");
            compiledCode.push_back("push eax");
            compiledCode.push_back(string_format("mov eax, %d", 4 * args.size()));
            compiledCode.push_back("call malloc");
            for(std::size_t i = 0; i < args.size(); i++) {
                resolve_argument(ctx, args[i], "ebx", compiledCode);
                compiledCode.push_back(string_format("mov [eax + %d], ebx", 4 * i));
            }
            compiledCode.push_back("mov ebx, eax");
            compiledCode.push_back("pop eax");
        }
        compiledCode.push_back(string_format("call %s", functionLabel.c_str()));
        if(args.size() > 0) {
            compiledCode.push_back("push eax");
            compiledCode.push_back("mov ebx, eax");
            compiledCode.push_back("call free");
            compiledCode.push_back("pop eax");
            compiledCode.push_back("pop ebx");
        }
    }

    if(line == "O0") compiledCode.push_back(";arsenic_o0");
    if(line == "O1") compiledCode.push_back(";arsenic_o1");
}
