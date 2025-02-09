#include <stdio.h>
#include <cstdlib>
#include <boost/algorithm/string.hpp>
// #include "../include/lexer.h"
#include "../include/parser.h"
using namespace std;

int main(int argc, char** argv) {
    if (argc <= 1) {
        printf("no targets specified (temporary: in the future targets will be specified in start.sage)");
        exit(1);
    }

    string target_file = string(argv[1]);

    // validate that file is valid
    if (target_file.find('.') == std::string::npos) {
        printf("cannot target files that have no file extension.\n");
        exit(1);
    }

    vector<string> delimited;
    boost::split(delimited, argv[1], boost::is_any_of("."));

    if (delimited[1] != "sage") {
        printf("cannot target non sage source files.\n");
        exit(1);
    }

    Parser parser = Parser(target_file);
    AbstractParseNode* parsetree = parser.parse_program(false);
    if (parsetree == nullptr) {
        printf("parsetree root is null. parsing failed.\n");
        return 1;
    }

    parsetree->showtree("");

    delete parsetree;
    delete parser.lexer;

    return 0;
}
