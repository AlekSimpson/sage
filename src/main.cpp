#include <string>

#include "../include/error_logger.h"
#include "../include/codegen.h"

int main(int argc, char **argv) {
    // TODO: implement compiler flags here in this file
    // TODO: STOP using vector, its so annoying, we need to make our own to suite our use cases
    if (argc <= 1) {
        ErrorLogger::get().log_error("null", -1, "no targets specified", GENERAL);
        return 1;
    }

    string target_file = string(argv[1]);

    SageCompiler compiler = SageCompiler(target_file);
    compiler.begin_compilation(target_file);

    if (ErrorLogger::get().has_errors()) {
        ErrorLogger::get().report_errors();
        return 1;
    }

    return 0;
}
