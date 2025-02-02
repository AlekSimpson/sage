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

    // Lexer lex = Lexer(target_file);
    // Token* tok;
    // for (int i = 0; i < 2; ++i) {
    //     tok = lex.get_token();
    // }

    // tok->print();

    Parser parser = Parser(target_file);
    AbstractParseNode* parsetree = parser.parse_program(true);

    parsetree->showtree("");

    // FIX: Returning the parse nodes from the parse functions inside the parser is not a good design. We should come up with some alternative spot to store these generated nodes

    delete parsetree;
    delete parser.lexer;

    return 0;
}
