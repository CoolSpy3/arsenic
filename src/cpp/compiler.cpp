#include "compiler.h"

void resolve_argument_a(
    Context ctx,
    std::string var,
    std::string reg,
    std::vector<std::string>& compiledCode,
    int numParents = 0
) {
    std::vector<std::string>::iterator it = std::find(ctx->variables.begin(), ctx->variables.end(), var);
    if(it == ctx->variables.end()) {
        if(ctx->parent) {
            resolve_argument_a(ctx->parent, var, reg, compiledCode, numParents + 1);
        } else {
            std::cerr << "Error: variable " << var << " not found" << std::endl;
            exit(1);
        }
    } else {
        if(reg != "eax") compiledCode.push_back("push eax");
        compiledCode.push_back("mov eax, [esp-4]");
        for(int i = 0; i < numParents; i++) compiledCode.push_back("mov eax, [eax]");
        compiledCode.push_back(string_format("add eax, {}", 4 * std::distance(ctx->variables.begin(), it)));
        if(reg != "eax") compiledCode.push_back("pop eax");
    }
}

void resolve_argument(
    Context ctx,
    std::string var,
    std::string reg,
    std::vector<std::string> &compiledCode
);

void resolve_argument_i(
    Context ctx,
    std::string var,
    std::string reg,
    std::vector<std::string> &compiledCode,
    int numParents = 0
) {
    trim(var);
    if(('0' <= var[0] && var[0] <= '9') || var[0] == '\'') {
        compiledCode.push_back(string_format("mov eax, {}", var));
        return;
    }
    if(var[0] == '(' || var[0] == '[') {
        resolve_argument(ctx, var.substr(1, var.size() - 2), reg, compiledCode);
        return;
    }
    std::vector<std::string>::iterator it = std::find(ctx->variables.begin(), ctx->variables.end(), var);
    if(it == ctx->variables.end()) {
        if(ctx->parent) {
            resolve_argument_i(ctx->parent, var, reg, compiledCode, numParents + 1);
        } else {
            std::cerr << "Error: variable " << var << " not found" << std::endl;
            exit(1);
        }
    } else {
        if(reg != "eax") compiledCode.push_back("push eax");
        compiledCode.push_back("mov eax, [esp-4]");
        for(int i = 0; i < numParents; i++) compiledCode.push_back("mov eax, [eax]");
        compiledCode.push_back(string_format("mov eax, [eax+{}]", 4 * std::distance(ctx->variables.begin(), it)));
        if(reg != "eax") compiledCode.push_back("pop eax");
    }
}

void resolve_argument_p(
    Context ctx,
    std::string var,
    std::string reg,
    std::vector<std::string> &compiledCode,
    std::vector<std::string> &definitions,
    int numParents = 0
) {
    trim(var);
    if(var[0] == '{') {
        if(reg != "eax") compiledCode.push_back("push eax");
        std::string elems = var.substr(1, var.size() - 2);
        int numElems = std::count(elems.begin(), elems.end(), ',') + 1;
        compiledCode.push_back(string_format("mov eax, {}", 4 * numElems));
        compiledCode.push_back("call malloc");
        for(int i = 0; i < numElems; i++) {
            int elemLen = elems.find(',');
            std::string elem = elems.substr(0, elemLen);
            elems = elems.substr(elemLen + 1);
            resolve_argument(ctx, elem, "ebx", compiledCode);
            compiledCode.push_back(string_format("mov [eax+{}], ebx", 4 * i));
        }
        if(reg != "eax") {
            compiledCode.push_back(string_format("mov {}, eax", reg));
            compiledCode.push_back("pop eax");
        }
        return;
    }
    if(var[0] == '"') {
        int len = var.size() - 2 + 1;
        int id = definitions.size();
        std::string svar = string_format("arsenic_s{}", id);
        definitions.push_back(string_format("{} {}, 0", svar, var));
        compiledCode.push_back("push esi");
        compiledCode.push_back("push edi");
        compiledCode.push_back("push ecx");
        if(reg != "eax") compiledCode.push_back("push eax");

        compiledCode.push_back(string_format("mov esi, {}", svar));
        compiledCode.push_back(string_format("mov eax, {}", len));
        compiledCode.push_back("call malloc");
        compiledCode.push_back("mov edi, eax");
        compiledCode.push_back(string_format("mov ecx, {}", len));

        compiledCode.push_back("rep movsb");

        compiledCode.push_back("pop ecx");
        compiledCode.push_back("pop edi");
        compiledCode.push_back("pop esi");

        compiledCode.push_back(string_format("mov {}, edi", reg));
        if(reg == "eax") compiledCode.push_back("pop eax");
        return;
    }

    if(reg != "eax") compiledCode.push_back("push eax");
    compiledCode.push_back("mov eax, 4");
    compiledCode.push_back("call malloc");
    compiledCode.push_back("push ebx");
    resolve_argument(ctx, var, "ebx", compiledCode);
    compiledCode.push_back("mov [eax], ebx");
    compiledCode.push_back("pop ebx");

    if(reg != "eax") {
        compiledCode.push_back(string_format("mov {}, eax", reg));
        compiledCode.push_back("pop eax");
    }
}

bool resolve_argument_operation(
    Context ctx,
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
        idx = var.find(operation) + 2;
    } else {
        idx = var.find(operation);
    }
    if(idx == std::string::npos) return false;
    std::string left = var.substr(0, idx);
    std::string right = var.substr(idx + operation.size());
    if(reg != "eax") compiledCode.push_back("push eax");
    if(reg != "ebx") compiledCode.push_back("push ebx");
    resolve_argument(ctx, left, "eax", compiledCode);
    resolve_argument(ctx, right, "ebx", compiledCode);
    printAsm(compiledCode);
    compiledCode.push_back(string_format("mov {}, eax", reg));
    if(reg != "ebx") compiledCode.push_back("pop ebx");
    if(reg != "eax") compiledCode.push_back("pop eax");
    return true;
}

void resolve_argument(
    Context ctx,
    std::string var,
    std::string reg,
    std::vector<std::string> &compiledCode
) {
    trim(var);

    // Grouping

    #define PARSE_GROUPING(charsBefore, charsAfter) \
        resolve_argument(ctx, var.substr(charsBefore, var.size() - charsAfter), reg, compiledCode); \
        if(var[0] == '[') compiledCode.push_back(string_format("mov {}, [{}]", reg, reg));

    // Postfix

    if(ends_with(var, "++") || ends_with(var, "--")) {
        PARSE_GROUPING(0, 2);
        if(ends_with(var, "++")) compiledCode.push_back(string_format("inc {}", reg));
        else compiledCode.push_back(string_format("dec {}", reg));
        return;
    }

    // Prefix

    if(var.rfind("++", 0) == 0 || var.rfind("--", 0) == 0) {
        if(((var[2] == '(' && var.back() == ')') || (var[2] == '[' && var.back() == ']')) && matchingBrackets(var.substr(2, var.size() - 4))) {
            PARSE_GROUPING(2, 0);
            if(var.rfind("++", 0) == 0) compiledCode.push_back(string_format("inc {}", reg));
            else compiledCode.push_back(string_format("dec {}", reg));
            return;
        }
    }
    if(var[0] == '~' || var[0] == '!') {
        if(((var[1] == '(' && var.back() == ')') || (var[1] == '[' && var.back() == ']')) && matchingBrackets(var.substr(1, var.size() - 3))) {
            PARSE_GROUPING(1, 0);
            string_format("not {}", reg);
            return;
        }
    }

    if(((var[0] == '(' && var.back() == ')') || (var[0] == '[' && var.back() == ']')) && matchingBrackets(var.substr(1, var.size() - 2))) {
        PARSE_GROUPING(0, 0);
        return;
    }

    // List in order of reverse precedence

    if(resolve_argument_operation(ctx, var, reg, "|", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("or eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, "^", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("xor eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, "&", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("and eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, "!=", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp eax, ebx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr eax, 14"); // 6 + 8 = 14
        compiledCode.push_back("not eax");
        compiledCode.push_back("and eax, 1");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, "==", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp eax, ebx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr eax, 14"); // 6 + 8 = 14
        compiledCode.push_back("and eax, 1");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, ">=", [](std::vector<std::string> &compiledCode) {
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

    if(resolve_argument_operation(ctx, var, reg, "<=", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp eax, ebx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr eax, 8");
        compiledCode.push_back("mov ebx, eax");
        compiledCode.push_back("shr eax, 6"); // 6 + 8 = 14
        compiledCode.push_back("and eax, 1");
        compiledCode.push_back("and ebx, 1");
        compiledCode.push_back("or eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, ">", [](std::vector<std::string> &compiledCode) {
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

    if(resolve_argument_operation(ctx, var, reg, "<", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp eax, ebx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr eax, 8");
        compiledCode.push_back("and eax, 1");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, ">>", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("shr eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, "<<", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("shl eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, "-", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("sub eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, "+", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("add eax, ebx");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, "%", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("push edx");
        compiledCode.push_back("div ebx");
        compiledCode.push_back("mov eax, edx");
        compiledCode.push_back("pop edx");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, "/", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("push edx");
        compiledCode.push_back("div ebx");
        compiledCode.push_back("pop edx");
    }, compiledCode)) return;

    if(resolve_argument_operation(ctx, var, reg, "*", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("push edx");
        compiledCode.push_back("mul ebx");
        compiledCode.push_back("pop edx");
    }, compiledCode)) return;

    if(var.rfind("++", 0) == 0 || var.rfind("--", 0) == 0) {
        resolve_argument_i(ctx, var.substr(2), reg, compiledCode);
        if(var.rfind("++", 0) == 0) compiledCode.push_back(string_format("inc {}", reg));
        else compiledCode.push_back(string_format("dec {}", reg));
        return;
    }

    if(ends_with(var, "++") || ends_with(var, "--")) {
        resolve_argument_i(ctx, var.substr(0, var.size() - 2), reg, compiledCode);
        if(ends_with(var, "++")) compiledCode.push_back(string_format("inc {}", reg));
        else compiledCode.push_back(string_format("dec {}", reg));
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

void compileLine(
    Context ctx,
    std::string line,
    std::function<std::unique_ptr<std::string>()> getLine,
    std::vector<std::string>& compiledCode,
    std::vector<std::string>& definitions
) {
    int indentation = calculateIndentation(line);
    trim(line);
    if(line.empty() || line[0] == ';' || line == "end") return;
    if(std::regex_match(line, std::regex("asm\\s*:"))) {
        for(;;) {
            std::unique_ptr<std::string> linePtr = getLine();
            if(!linePtr) break;
            line = *linePtr.get();
            if(trim_copy(line).empty()) continue;
            if(indentation <= calculateIndentation(line)) break;
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
            compiledCode.push_back(line);
        }
        compileLine(ctx, line, getLine, compiledCode, definitions);
        return;
    }
}
