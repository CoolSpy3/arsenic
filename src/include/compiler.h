#pragma once
#include <functional>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>
#include "utils.h"

struct Context_;

typedef Context_ *Context;

struct Context_ {
    std::string name;
    std::vector<std::string> variables;
    Context parent, root;
};

void compileLine(
    Context ctx,
    std::string line,
    std::function<std::unique_ptr<std::string>()> getLine,
    std::vector<std::string>& compiledCode,
    std::vector<std::string>& definitions
);
