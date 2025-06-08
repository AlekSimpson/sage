#include <stdio.h>
#include <cstdlib>
#include <string>
#include <memory>
#include <boost/algorithm/string.hpp>

#include "llvm/Support/ManagedStatic.h"
#include "../include/codegen.h"
#include "../include/node_manager.h"
#include "../include/parser.h"
#include "../include/token.h"

using namespace llvm;

int main(int argc, char** argv) {
    auto llvm_context = std::make_shared<llvm::LLVMContext>();

    if (argc <= 1) {
        printf("no targets specified (temporary: in the future targets will be specified in start.sage)");
        exit(1);
    }

    string target_file = string(argv[1]);

    SageCompiler compiler = SageCompiler(target_file, llvm_context);
    compiler.begin_compilation(target_file);

    return 0;
}



