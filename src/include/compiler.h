#pragma once
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>
#include "utils.h"

struct Context;

struct Variable {
    std::string name;
    std::function<void(std::shared_ptr<Context>, std::string, std::string, std::vector<std::string>&, int, int)> getAddr, getValue;
    bool onStack = true;
};

struct Struct_ {
    std::map<std::string, int> members;
    int size;
};

struct Context {
    std::string name;
    std::map<std::string, Variable> variables;
    std::map<std::string, Struct_> structs;
    std::map<std::string, std::string> functions;
    std::shared_ptr<Context> parent, root;
    int depth;
};

void resolve_argument_a(
    std::shared_ptr<Context> ctx,
    std::string var,
    std::string reg,
    std::vector<std::string>& compiledCode,
    int numParents
);

void resolve_argument_i(
    std::shared_ptr<Context> ctx,
    std::string var,
    std::string reg,
    std::vector<std::string> &compiledCode,
    int numParents
);

void resolve_argument_p(
    std::shared_ptr<Context> ctx,
    std::string var,
    std::string reg,
    std::vector<std::string> &compiledCode,
    std::vector<std::string> &definitions,
    int numParents
);

bool resolve_argument_o(
    std::shared_ptr<Context> ctx,
    std::string var,
    std::string reg,
    std::string operation,
    std::function<void(std::vector<std::string>&)> printAsm,
    std::vector<std::string> &compiledCode
);

void resolve_argument(
    std::shared_ptr<Context> ctx,
    std::string var,
    std::string reg,
    std::vector<std::string> &compiledCode
);

std::map<std::string, Variable> preprocessFunction(
    std::shared_ptr<Context> ctx,
    int indentation,
    std::function<std::unique_ptr<std::string>()> getLine,
    std::ifstream &file
);

std::string allocateLabel(
    std::string requestedName,
    std::shared_ptr<Context> ctx
);

std::map<std::string, Variable> defaultVars();

Variable var(std::string name);

Variable constVar(std::string name);

Variable globalVar(std::string name);

int stackVars(std::map<std::string, Variable> vars);

void compileLine(
    std::shared_ptr<Context> ctx,
    std::string line,
    std::function<std::unique_ptr<std::string>()> getLine,
    std::vector<std::string>& compiledCode,
    std::vector<std::string>& definitions,
    std::ifstream& file
);
