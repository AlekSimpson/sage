#include <stdio.h>
#include <cstdlib>
#include <string>
#include <memory>
#include <boost/algorithm/string.hpp>

#include "../include/codegen.h"
#include "../include/parse_node.h"
#include "../include/parser.h"

int main(int argc, char** argv) {
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

    parsetree->showtree("");

    SageCodeGenVisitor main_visitor = SageCodeGenVisitor();
    printf("%s\n", nodetype_to_string(parsetree->get_nodetype()).c_str());
    // main_visitor.visit_program(parsetree);

    delete parsetree;

    return 0;
}

// #include <boost/uuid/uuid.hpp>
// #include <boost/uuid/uuid_generators.hpp>
// #include <boost/uuid/uuid_io.hpp>

// Create a random UUID generator
// boost::uuids::random_generator generator;

// Generate a UUID
// boost::uuids::uuid uuid = generator();



