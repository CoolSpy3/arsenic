#pragma once
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <vector>
#include "utils.h"

struct Context_;

struct Context {
    std::string name;
    std::vector<std::string> variables, functions;
    std::shared_ptr<Context> parent, root;
};

void compileLine(
    std::shared_ptr<Context> ctx,
    std::string line,
    std::function<std::unique_ptr<std::string>()> getLine,
    std::vector<std::string>& compiledCode,
    std::vector<std::string>& definitions,
    std::ifstream& file
);
