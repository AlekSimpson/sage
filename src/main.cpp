#include <stdio.h>
#include <cstdlib>
#include <string>
#include <memory>
#include <boost/algorithm/string.hpp>

#include "llvm/Support/ManagedStatic.h"
#include "../include/codegen.h"
#include "../include/parse_node.h"
#include "../include/parser.h"

using namespace llvm;

int main(int argc, char** argv) {
    auto llvm_context = std::make_shared<llvm::LLVMContext>();

    if (argc <= 1) {
        printf("no targets specified (temporary: in the future targets will be specified in start.sage)");
        exit(1);
    }

    string target_file = string(argv[1]);

    // validate that file is valid
    if (target_file.find('.') == string::npos) {
        printf("cannot target files that have no file extension.\n");
        exit(1);
    }

    vector<string> delimited;
    boost::split(delimited, argv[1], boost::is_any_of("."));

    if (delimited[1] != "sage") {
        printf("cannot target non sage source files.\n");
        exit(1);
    }

    SageParser parser = SageParser(target_file);
    AbstractParseNode* parsetree = parser.parse_program(false);
    if (parsetree == nullptr) {
        printf("parsetree root is null. parsing failed.\n");
        return 1;
    }

    // parsetree->showtree("");

    bool success = false;
    SageCompiler compiler = SageCompiler(parsetree, llvm_context);
    auto module = compiler.compile_module();
    success = compiler.generate_output(module, "sage.out");

    if (success) {
        printf("Compilation finished successfully.\n");
    }else {
        printf("Compilation finished unsuccessfully. It's ok try again :)\n");
    }

    return 0;
}



