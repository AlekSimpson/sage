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
    if (!compiler.check_filename_valid(target_file)) {
        compiler.compiler_exit("Main program filename is not valid. Make sure it ends in '.sage'", 1);
    }

    // SageParser parser = SageParser(compiler->node_manager, target_file);
    NodeIndex parsetree = compiler.parser.parse_program(false);
    if (parsetree == -1) {
        printf("parsetree root is null. parsing failed.\n");
        return 1;
    }

    bool success = false;

    // compiler->node_manager->showtree(parsetree);

    auto module = compiler.compile_module(parsetree);
    success = compiler.generate_output(module, "sage.out");

    if (success) {
        printf("Compilation finished successfully.\n");
    }else {
        printf("Compilation finished unsuccessfully. It's ok try again :)\n");
    }

    return 0;
}



