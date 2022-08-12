#include "compiler.h"

Struct_ findStruct(std::shared_ptr<Context> ctx, std::string name) {
    do {
        if(ctx->structs.count(name)) return ctx->structs.find(name)->second;
        ctx = ctx->parent;
    } while(ctx);
    std::cerr << "Error: struct " << name << " not found" << std::endl;
    exit(1);
}

bool reg_match(std::string reg, char match) {
    return std::regex_match(reg, std::regex(string_format("r?%c(l|h|x)", match)));
}

#define CHECK_SPECIAL_VARS(name) name == "args" ? ".arg" : name == "return" ? ".ret" : name

void resolve_argument_a(
    std::shared_ptr<Context> ctx,
    std::string var,
    std::string reg,
    std::vector<std::string>& compiledCode,
    int numParents = 0
) {
    std::smatch structMatch;
    if(std::regex_match(var, structMatch, std::regex("\\s*\\(\\s*\\(\\s*struct \\s*(\\w+)\\s*\\)\\s*(\\w+)\\s*\\)\\.(\\w+)\\s*"))) {
        std::string structName = structMatch[1];
        std::string structVar = structMatch[2];
        std::string structMember = structMatch[3];
        Struct_ struct_ = findStruct(ctx, structName);
        int memberOffset = struct_.members.find(structMember)->second;
        resolve_argument_a(ctx, structVar, reg, compiledCode);
        compiledCode.push_back(string_format("add %s, %d", reg.c_str(), memberOffset));
        return;
    }
    var = CHECK_SPECIAL_VARS(var);
    std::map<std::string, Variable>::iterator it = ctx->variables.find(var);
    if(it == ctx->variables.end()) {
        if(ctx->parent) {
            resolve_argument_a(ctx->parent, var, reg, compiledCode, numParents + 1);
        } else {
            std::cerr << "Error: variable " << var << " not found" << std::endl;
            exit(1);
        }
    } else {
        it->second.getAddr(ctx, var, reg, compiledCode, numParents, std::distance(ctx->variables.begin(), it));
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
    std::smatch match;
    if(std::regex_match(var, match, std::regex("\\(\\s*\\(\\s*struct \\s*(.+)\\s*\\)\\s*(\\w+)\\s*\\)\\.(\\w+)"))) {
        std::string structName = match[1];
        std::string structVar = match[2];
        std::string structMember = match[3];
        Struct_ struct_ = findStruct(ctx, structName);
        int memberOffset = struct_.members.find(structMember)->second;
        resolve_argument_a(ctx, structVar, reg, compiledCode);
        compiledCode.push_back(string_format("add %s, %d", reg.c_str(), memberOffset));
        compiledCode.push_back(string_format("mov %s, [%s]", reg.c_str()));
        return;
    }
    if(std::regex_match(var, match, std::regex("faddr\\(\\s*(\\w+)\\s*\\)"))) {
        std::string functionName = match[1];

        std::shared_ptr<Context> searchCtx = ctx;
        std::map<std::string, std::string>::iterator functionLabelIttr;
        do {
            if((functionLabelIttr = searchCtx->functions.find(functionName)) != searchCtx->functions.end()) break;
            searchCtx = searchCtx->parent;
        } while(searchCtx);

        std::string functionLabel = functionLabelIttr->second;
        compiledCode.push_back(string_format("lea %s, [%s]", reg.c_str(), functionLabel.c_str()));
        return;
    }
    if(('0' <= var[0] && var[0] <= '9') || var[0] == '\'') {
        compiledCode.push_back(string_format("mov %s, %s", reg.c_str(), var.c_str()));
        return;
    }
    if(var[0] == '(' || var[0] == '[') {
        resolve_argument(ctx, var.substr(1, var.size() - 2), reg, compiledCode);
        return;
    }
    var = CHECK_SPECIAL_VARS(var);
    std::map<std::string, Variable>::iterator it = ctx->variables.find(var);
    if(it == ctx->variables.end()) {
        if(ctx->parent) {
            resolve_argument_i(ctx->parent, var, reg, compiledCode, numParents + 1);
        } else {
            std::cerr << "Error: variable " << var << " not found" << std::endl;
            exit(1);
        }
    } else {
        it->second.getValue(ctx, var, reg, compiledCode, numParents, std::distance(ctx->variables.begin(), it));
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
        if(!reg_match(reg, 'a')) compiledCode.push_back("push rax");
        std::string elems = var.substr(1, var.size() - 2);
        int numElems = std::count(elems.begin(), elems.end(), ',') + 1;
        compiledCode.push_back(string_format("mov rax, %d ", 8 * numElems));
        compiledCode.push_back("call malloc");
        for(int i = 0; i < numElems; i++) {
            int elemLen = elems.find(',');
            std::string elem = elems.substr(0, elemLen);
            elems = elems.substr(elemLen + 1);
            resolve_argument(ctx, elem, "rbx", compiledCode);
            compiledCode.push_back(string_format("mov [rax+%d], rbx", 8 * i));
        }
        if(!reg_match(reg, 'a')) {
            compiledCode.push_back(string_format("mov %s, rax", reg.c_str()));
            compiledCode.push_back("pop rax");
        }
        return;
    }
    if(var[0] == '"') {
        int len = var.size() - 2 + 1;
        int id = definitions.size();
        std::string svar = string_format("arsenic_s%d", id);
        definitions.push_back(string_format("%s db %s, 0", svar.c_str(), var.c_str()));
        compiledCode.push_back("push rsi");
        compiledCode.push_back("push rdi");
        compiledCode.push_back("push rcx");
        if(!reg_match(reg, 'a')) compiledCode.push_back("push rax");

        compiledCode.push_back(string_format("mov rsi, %s", svar.c_str()));
        compiledCode.push_back(string_format("mov rax, %d", len));
        compiledCode.push_back("call malloc");
        compiledCode.push_back("mov rdi, rax");
        compiledCode.push_back(string_format("mov rcx, %d", len));

        compiledCode.push_back("rep movsb");

        compiledCode.push_back("pop rcx");
        compiledCode.push_back("pop rdi");
        compiledCode.push_back("pop rsi");

        compiledCode.push_back(string_format("mov %s, rdi", reg.c_str()));
        if(!reg_match(reg, 'a')) compiledCode.push_back("pop rax");
        return;
    }

    std::smatch emptyArrayMatch;
    if(std::regex_match(var, emptyArrayMatch, std::regex("(.+){}"))) {
        if(!reg_match(reg, 'a')) compiledCode.push_back("push rax");
        resolve_argument(ctx, emptyArrayMatch[1], "rax", compiledCode);
        compiledCode.push_back("call malloc");
        compiledCode.push_back(string_format("mov %s, rax", reg.c_str()));
        if(!reg_match(reg, 'a')) compiledCode.push_back("pop rax");
        return;
    }

    if(!reg_match(reg, 'a')) compiledCode.push_back("push rax");
    compiledCode.push_back("mov rax, 8");
    compiledCode.push_back("call malloc");
    compiledCode.push_back("push rbx");
    resolve_argument(ctx, var, "rbx", compiledCode);
    compiledCode.push_back("mov [rax], rbx");
    compiledCode.push_back("pop rbx");

    if(!reg_match(reg, 'a')) {
        compiledCode.push_back(string_format("mov %s, rax", reg.c_str()));
        compiledCode.push_back("pop rax");
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
    if(!reg_match(reg, 'a')) compiledCode.push_back("push rax");
    if(!reg_match(reg, 'b')) compiledCode.push_back("push rbx");
    resolve_argument(ctx, left, "rax", compiledCode);
    resolve_argument(ctx, right, "rbx", compiledCode);
    printAsm(compiledCode);
    compiledCode.push_back(string_format("mov %s, rax", reg.c_str()));
    if(!reg_match(reg, 'b')) compiledCode.push_back("pop rbx");
    if(!reg_match(reg, 'a')) compiledCode.push_back("pop rax");
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
        compiledCode.push_back("or rax, rbx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "^", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("xor rax, rbx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "&", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("and rax, rbx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "!=", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp rax, rbx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr rax, 14"); // 6 + 8 = 14
        compiledCode.push_back("not rax");
        compiledCode.push_back("and rax, 1");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "==", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp rax, rbx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr rax, 14"); // 6 + 8 = 14
        compiledCode.push_back("and rax, 1");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, ">=", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp rax, rbx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr rax, 8");
        compiledCode.push_back("mov rbx, rax");
        compiledCode.push_back("shr rax, 6"); // 6 + 8 = 14
        compiledCode.push_back("not rax");
        compiledCode.push_back("and rax, 1");
        compiledCode.push_back("and rbx, 1");
        compiledCode.push_back("or rax, rbx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "<=", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp rax, rbx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr rax, 8");
        compiledCode.push_back("mov rbx, rax");
        compiledCode.push_back("shr rax, 6"); // 6 + 8 = 14
        compiledCode.push_back("and rax, 1");
        compiledCode.push_back("and rbx, 1");
        compiledCode.push_back("or rax, rbx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, ">", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp rax, rbx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr rax, 8");
        compiledCode.push_back("mov rbx, rax");
        compiledCode.push_back("shr rax, 6"); // 6 + 8 = 14
        compiledCode.push_back("not rax");
        compiledCode.push_back("and rax, 1");
        compiledCode.push_back("not rbx");
        compiledCode.push_back("and rbx, 1");
        compiledCode.push_back("and rax, rbx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "<", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("cmp rax, rbx");
        compiledCode.push_back("lahf");
        compiledCode.push_back("shr rax, 8");
        compiledCode.push_back("and rax, 1");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, ">>", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("shr rax, rbx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "<<", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("shl rax, rbx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "-", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("sub rax, rbx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "+", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("add rax, rbx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "%", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("push rdx");
        compiledCode.push_back("div rbx");
        compiledCode.push_back("mov rax, rdx");
        compiledCode.push_back("pop rdx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "/", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("push rdx");
        compiledCode.push_back("div rbx");
        compiledCode.push_back("pop rdx");
    }, compiledCode)) return;

    if(resolve_argument_o(ctx, var, reg, "*", [](std::vector<std::string> &compiledCode) {
        compiledCode.push_back("push rdx");
        compiledCode.push_back("mul rbx");
        compiledCode.push_back("pop rdx");
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

std::map<std::string, Variable> preprocessFunction(
    std::shared_ptr<Context> ctx,
    int indentation,
    std::function<std::unique_ptr<std::string>()> getLine,
    std::ifstream &file
) {
    std::map<std::string, Variable> variables = defaultVars();
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
            std::string name = varMatch[1];
            if(name[0] <= '9') continue;
            if(name == "return" || name == "args") continue;
            Context superCtx = *ctx;
            if(superCtx.parent == nullptr) {
                variables.emplace(name, var(name));
                break;
            } else {
                while(superCtx.parent) {
                    if(superCtx.variables.count(name)) break;
                    if(superCtx.parent == nullptr) {
                        variables.emplace(name, var(name));
                        break;
                    }
                    superCtx = *superCtx.parent;
                }
            }
        } else if (std::regex_match(line, varMatch, std::regex("\\[[^\\s]+\\]"))) {
            std::string name = varMatch[1];
            if(name[0] <= '9') continue;
            if(name == "return" || name == "args") continue;
            Context superCtx = *ctx;
            while(superCtx.parent) {
                if(superCtx.variables.count(name)) break;
                if(superCtx.parent == nullptr) {
                    variables.emplace(name, var(name));
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

std::function<void(std::shared_ptr<Context>, std::string, std::string, std::vector<std::string>&, int, int)> getArgAddr =
    [](std::shared_ptr<Context> ctx, std::string var, std::string reg, std::vector<std::string>& compiledCode, int numParents, int pos) {
        compiledCode.push_back(string_format("mov %s, rbp", reg.c_str()));
        compiledCode.push_back(string_format("sub %s, %d", reg.c_str(),  8 * (ctx->depth + 2)));
    };
std::function<void(std::shared_ptr<Context>, std::string, std::string, std::vector<std::string>&, int, int)> getArgValue =
    [](std::shared_ptr<Context> ctx, std::string var, std::string reg, std::vector<std::string>& compiledCode, int numParents, int pos) {
        compiledCode.push_back(string_format("mov %s, [rbp-%d]", reg.c_str(),  8 * (ctx->depth + 2)));
    };
std::function<void(std::shared_ptr<Context>, std::string, std::string, std::vector<std::string>&, int, int)> getRetAddr =
    [](std::shared_ptr<Context> ctx, std::string var, std::string reg, std::vector<std::string>& compiledCode, int numParents, int pos) {
        compiledCode.push_back(string_format("mov %s, [rbp-%d]", reg.c_str(), 8 * ctx->depth));
        compiledCode.push_back(string_format("sub %s, rbp-%d", reg.c_str(), 8 * (ctx->depth + 1 + 1)));
    };
std::function<void(std::shared_ptr<Context>, std::string, std::string, std::vector<std::string>&, int, int)> getRetValue =
    [](std::shared_ptr<Context> ctx, std::string var, std::string reg, std::vector<std::string>& compiledCode, int numParents, int pos) {
        compiledCode.push_back(string_format("mov %s, [rbp-%d]", reg.c_str(), 8 * (ctx->depth + 2 + 1)));
    };

std::map<std::string, Variable> defaultVars() {
    std::map<std::string, Variable> vars;
    vars.emplace(".arg", Variable{".arg", getArgAddr, getArgValue});
    vars.emplace(".ret", Variable{".ret", getRetAddr, getRetValue});
    return vars;
}

std::function<void(std::shared_ptr<Context>, std::string, std::string, std::vector<std::string>&, int, int)> getDefaultAddr =
    [](std::shared_ptr<Context> ctx, std::string var, std::string reg, std::vector<std::string>& compiledCode, int numParents, int pos) {
        if(numParents != 0) {
            compiledCode.push_back(string_format("mov %s, [rbp-%d]", reg.c_str(), 8 * (ctx->depth + 1)));
        } else {
            compiledCode.push_back(string_format("mov %s, rbp", reg.c_str()));
        }
        compiledCode.push_back(string_format("sub %s, %d", reg.c_str(),  8 * (ctx->depth + 2 + pos)));
    };
std::function<void(std::shared_ptr<Context>, std::string, std::string, std::vector<std::string>&, int, int)> getDefaultValue =
    [](std::shared_ptr<Context> ctx, std::string var, std::string reg, std::vector<std::string>& compiledCode, int numParents, int pos) {
        if(numParents != 0) {
            compiledCode.push_back(string_format("mov %s, [rbp-%d]", reg.c_str(), 8 * (ctx->depth + 1)));
            compiledCode.push_back(string_format("mov %s, [%s-%d]", reg.c_str(), reg.c_str(), 8 * (ctx->depth + 2 + pos)));
        } else {
            compiledCode.push_back(string_format("mov %s, [rbp-%d]", reg.c_str(), 8 * (ctx->depth + 2 + pos)));
        }
    };

Variable var(std::string name) {
    return Variable{name, getDefaultAddr, getDefaultValue};
}

std::function<void(std::shared_ptr<Context>, std::string, std::string, std::vector<std::string>&, int, int)> getConstAddr =
    [](std::shared_ptr<Context> ctx, std::string var, std::string reg, std::vector<std::string>& compiledCode, int numParents, int pos) {
        compiledCode.push_back(string_format("mov %s, 0", reg.c_str()));
    };
std::function<void(std::shared_ptr<Context>, std::string, std::string, std::vector<std::string>&, int, int)> getConstValue =
    [](std::shared_ptr<Context> ctx, std::string var, std::string reg, std::vector<std::string>& compiledCode, int numParents, int pos) {
        compiledCode.push_back(string_format("mov %s, %s_v%s", reg.c_str(), ctx->name.c_str(), var.c_str()));
    };

Variable constVar(std::string name) {
    return Variable{name, getConstAddr, getConstAddr, false};
}

std::function<void(std::shared_ptr<Context>, std::string, std::string, std::vector<std::string>&, int, int)> getGlobalAddr =
    [](std::shared_ptr<Context> ctx, std::string var, std::string reg, std::vector<std::string>& compiledCode, int numParents, int pos) {
        compiledCode.push_back(string_format("lea %s, [%s_v%s]", reg.c_str(), ctx->name.c_str(), var.c_str()));
    };
std::function<void(std::shared_ptr<Context>, std::string, std::string, std::vector<std::string>&, int, int)> getGlobalValue =
    [](std::shared_ptr<Context> ctx, std::string var, std::string reg, std::vector<std::string>& compiledCode, int numParents, int pos) {
        compiledCode.push_back(string_format("mov %s, [%s_v%s]", reg.c_str(), ctx->name.c_str(), var.c_str()));
    };

Variable globalVar(std::string name) {
    return Variable{name, getGlobalAddr, getGlobalValue};
}

int stackVars(std::map<std::string, Variable> vars) {
    int stackVars = 0;
    for(std::pair<std::string, Variable> var : vars) if(var.second.onStack) stackVars++;
    return stackVars;
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
    std::smatch match;
    if(std::regex_match(line, match, std::regex("asm\\s*(?:([^\\s]+)\\s*):"))) {
        std::string functionName, functionLabel;
        if(match.size() > 1) {
            functionName = match[1];
            std::string functionLabel = string_format("%s_f%s", ctx->name.c_str(), string_replace(functionName, std::string("_"), std::string("__")).c_str());
            ctx->functions.emplace(functionName, functionLabel);
            compiledCode.push_back(string_format("jmp %s_e", functionLabel.c_str()));
            compiledCode.push_back(string_format("%s:", functionLabel.c_str()));
        }
        for(;;) {
            std::unique_ptr<std::string> linePtr = getLine();
            if(!linePtr) break;
            line = *linePtr.get();
            if(trim_copy(line).empty()) continue;
            if(indentation >= calculateIndentation(line)) break;
            std::smatch match;
            if(std::regex_search(line, match, std::regex("\\{(.*),\\s*(rax|rbx|rcx|rdx)}"))) {
                resolve_argument(ctx, match[1], match[2], compiledCode);
                std::regex_replace(line, std::regex("\\{.*,\\s*(rax|rbx|rcx|rdx)\\}"), "$1");
            }
            if(std::regex_search(line, match, std::regex("\\[(.*),\\s*(rax|rbx|rcx|rdx)]"))) {
                resolve_argument_a(ctx, match[1], match[2], compiledCode);
                std::regex_replace(line, std::regex("\\[[^\\[\\]]*,\\s*(rax|rbx|rcx|rdx)\\]"), "$1");
            }
            trim(line);
            if(line == "O0") compiledCode.push_back(";arsenic_o0");
            else if(line == "O1") compiledCode.push_back(";arsenic_o1");
            else compiledCode.push_back(line);
        }
        if(!functionLabel.empty()) compiledCode.push_back(string_format("%s_e:", functionLabel.c_str()));
        if(file.good()) compileLine(ctx, line, getLine, compiledCode, definitions, file);
        return;
    }
    if(std::regex_match(line, match, std::regex("([^\\s]+)\\s*=\\s*(.+)"))) {
        compiledCode.push_back("push rax");
        compiledCode.push_back("push rbx");

        resolve_argument_a(ctx, match[1], "rax", compiledCode);
        resolve_argument(ctx, match[2], "rbx", compiledCode);

        compiledCode.push_back("mov [rax], rbx");

        compiledCode.push_back("pop rbx");
        compiledCode.push_back("pop rax");

        if(match[1] == "return") {
            compiledCode.push_back("leave");
            compiledCode.push_back("ret");
        }
        return;
    }
    if(std::regex_match(line, match, std::regex("([^\\s]+)\\s*>\\s*(.+)"))) {
        compiledCode.push_back("push rax");
        compiledCode.push_back("push rbx");

        resolve_argument_a(ctx, match[1], "rax", compiledCode);
        resolve_argument_p(ctx, match[2], "rbx", compiledCode, definitions);

        compiledCode.push_back("mov [rax], rbx");

        compiledCode.push_back("pop rbx");
        compiledCode.push_back("pop rax");

        if(match[1] == "return") {
            compiledCode.push_back("leave");
            compiledCode.push_back("ret");
        }
        return;
    }
    if(std::regex_match(line, match, std::regex("([^\\s]+)\\s*<\\s*(.+)"))) {
        compiledCode.push_back("push rax");
        compiledCode.push_back("push rbx");

        resolve_argument_i(ctx, match[1], "rax", compiledCode);
        resolve_argument(ctx, match[2], "rbx", compiledCode);

        compiledCode.push_back("mov [rax], rbx");

        compiledCode.push_back("pop rbx");
        compiledCode.push_back("pop rax");
        return;
    }

    if(std::regex_match(line, match, std::regex("([^\\s]+)\\s*:"))) {
        std::string functionName = match[1];
        std::string functionLabel = string_format("%s_f%s", ctx->name.c_str(), string_replace(functionName, std::string("_"), std::string("__")).c_str());

        ctx->functions.emplace(functionName, functionLabel);

        std::map<std::string, Variable> functionVars = preprocessFunction(ctx, indentation, getLine, file);

        std::shared_ptr<Context> nCtx = std::make_shared<Context>(Context{functionLabel, functionVars, std::map<std::string, Struct_>(), std::map<std::string, std::string>(), ctx, ctx->root, ctx->depth + 1});

        compiledCode.push_back(string_format("jmp %s_e", functionLabel.c_str()));
        compiledCode.push_back(string_format("%s:", functionLabel.c_str()));
        compiledCode.push_back(string_format("enter %d, %d", 8 * stackVars(functionVars), ctx->depth));
        compiledCode.push_back(string_format("mov [rbp-%d], ebx", 8 * (ctx->depth + 2)));
        for(;;) {
            std::unique_ptr<std::string> linePtr = getLine();
            if(!linePtr) break;
            line = *linePtr.get();
            if(trim_copy(line).empty()) continue;
            if(indentation >= calculateIndentation(line)) break;
            compileLine(nCtx, line, getLine, compiledCode, definitions, file);
        }
        if(compiledCode.back() != "ret") {
            compiledCode.push_back("leave");
            compiledCode.push_back("ret");
        }
        compiledCode.push_back(string_format("%s_e:", functionLabel.c_str()));
        if(file.good()) compileLine(ctx, line, getLine, compiledCode, definitions, file);
        return;
    }

    if(line == "return") {
        compiledCode.push_back("leave");
        compiledCode.push_back("ret");
        return;
    }

    if(std::regex_match(line, match, std::regex("delete\\s+([^\\s]+)"))) {
        compiledCode.push_back("push rax");
        resolve_argument_i(ctx, match[1], "rax", compiledCode);
        compiledCode.push_back("call free");
        compiledCode.push_back("pop rax");
        return;
    }

    if(std::regex_match(line, match, std::regex("if\\s+([^\\s].+)\\s*:"))) {
        std::string ifLabel = allocateLabel(string_format("%s_cif", ctx->name.c_str()), ctx);
        compiledCode.push_back(string_format("%s:", ifLabel.c_str()));
        compiledCode.push_back("push rax");
        resolve_argument(ctx, match[1], "rax", compiledCode);
        compiledCode.push_back("cmp rax, 0");
        compiledCode.push_back("pop rax");
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
        return;
    }

    if(std::regex_match(line, match, std::regex("while\\s+([^\\s].+)\\s*:"))) {
        std::string whileLabel = allocateLabel(string_format("%s_cwhile", ctx->name.c_str()), ctx);
        compiledCode.push_back(string_format("%s:", whileLabel.c_str()));
        compiledCode.push_back("push rax");
        resolve_argument(ctx, match[1], "rax", compiledCode);
        compiledCode.push_back("cmp rax, 0");
        compiledCode.push_back("pop rax");
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
        return;
    }

    if(std::regex_match(line, match, std::regex("([^\\s]+)\\s*\\((.*)\\)"))) {
        std::string functionName = match[1];

        std::string functionLabel;

        std::smatch varFuncMatch;
        if(std::regex_match(functionName, varFuncMatch, std::regex("\\(\\s*\\(\\s*func\\s*\\)\\s*(.*)\\)"))) {
            resolve_argument_i(ctx, varFuncMatch[1], "rdx", compiledCode);
        } else {
            std::shared_ptr<Context> searchCtx = ctx;
            std::map<std::string, std::string>::iterator functionLabelIttr;
            do {
                if((functionLabelIttr = searchCtx->functions.find(functionName)) != searchCtx->functions.end()) break;
                searchCtx = searchCtx->parent;
            } while(searchCtx);

            functionLabel = functionLabelIttr->second;
        }

        std::string rawArgs = match[2];

        std::vector<std::string> args = split(rawArgs, ',');
        if(args.size() > 0) {
            std::transform(args.begin(), args.end(), args.begin(), [](std::string arg) {
                return trim_copy(arg);
            });
            compiledCode.push_back(string_format("sub rsp, %d", 8 * args.size()));
            compiledCode.push_back("mov rbx, rsp");
            compiledCode.push_back("sub rbx, 4");
            for(std::size_t i = 0; i < args.size(); i++) {
                resolve_argument(ctx, args[i], "rax", compiledCode);
                compiledCode.push_back(string_format("mov [rbx + %d], rax",  8 * (args.size() - i - 1)));
            }
        }
        compiledCode.push_back(string_format("call %s", functionLabel.empty() ? "rdx" : functionLabel.c_str()));
        if(args.size() > 0) compiledCode.push_back(string_format("add rsp, %d", 8 * args.size()));
        return;
    }

    if(line == "O0") {
        compiledCode.push_back(";arsenic_o0");
        return;
    }
    if(line == "O1") {
        compiledCode.push_back(";arsenic_o1");
        return;
    }

    if(line.rfind("struct ", 0) == 0) {
        std::vector<std::string> tokens = split(line, ' ');

        std::string structName = tokens[1];
        Struct_ struct_;

        ctx->structs.emplace(structName, struct_);

        int size = 0;

        for(std::size_t i = 2; i < tokens.size(); i++) {
            std::string name = tokens[i];

            if(name == "struct") {
                i++;
                structName = tokens[i];
                i++;
                name = tokens[i];
                struct_.members.emplace(name, size);
                size += findStruct(ctx, structName).size;
            } else {
                struct_.members.emplace(name, size);
                size += 4;
            }
        }

        struct_.size = size;
        return;
    }
}
