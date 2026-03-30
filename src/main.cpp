#include <string>
#include <boost/algorithm/string.hpp>
#include <filesystem>

#include "../include/error_logger.h"
#include "../include/codegen.h"
#include "../include/bytecode_builder.h"

namespace fs = std::filesystem;

bool check_filename_valid(const string &filename) {
    Token null_token = Token();

    if (!fs::exists(filename)) {
        // exists
        ErrorLogger::get().log_error_unsafe(
            null_token,
            sen(filename, "does not exist."),
            USER);
        return false;
    }

    if (!fs::is_regular_file(filename)) {
        // it's a file
        ErrorLogger::get().log_error_unsafe(
           null_token,
           sen(filename, "is not a valid file."),
           USER);
        return false;
    }

    // validate that file sage code file
    if (filename.find('.') == string::npos) {
        ErrorLogger::get().log_error_unsafe(filename, -1, "cannot target files that have no file extension.", USER);
        return false;
    }

    vector<string> delimited;
    boost::split(delimited, filename, boost::is_any_of("."));

    if (delimited[1] != "sage") {
        ErrorLogger::get().log_error_unsafe(filename, -1, "cannot target non-sage source files.", USER);
        return false;
    }

    return true;
}

CompilerOptions parse_compiler_flags(int argc, char **argv) {
    CompilerOptions options("");
    Token null_token = Token();

    if (argc <= 1) {
        ErrorLogger::get().log_error_unsafe(
            null_token,
            "User did not provide compiler with source file. Please run compiler command with path to target source file.",
            USER);
        return options;
    }

    const int ABBREVIATED_OUTPUT_FILE = get_procedure_frame_id(string("-o"));
    const int OUTPUT_FILE = get_procedure_frame_id(string("--output-file"));
    const int COMPILER_DEBUG_FLAG = get_procedure_frame_id(string("--debug"));
    const int ABBREVIATED_COMPILER_DEBUG_FLAG = get_procedure_frame_id(string("-d"));
    const int COMPILATION_TARGET = get_procedure_frame_id(string("--compilation-target"));
    const int ABBREVIATED_COMPILATION_TARGET = get_procedure_frame_id(string("-ct"));
    const int EMIT_BYTECODE = get_procedure_frame_id(string("--emit-bytecode"));
    const int ABBREVIATED_EMIT_BYTECODE = get_procedure_frame_id(string("-e"));
    const int PRINT_BYTECODE = get_procedure_frame_id(string("--print-bytecode"));
    const int ABBREVIATED_PRINT_BYTECODE = get_procedure_frame_id(string("-p"));

    char *current_option;
    bool skip_next_iteration = false;
    for (int i = 1; i < argc; ++i) {
        if (skip_next_iteration) {
            skip_next_iteration = false;
            continue;
        }
        current_option = argv[i];

        if (current_option[0] == '-') {
            // parsing compiler option
            auto flag_hash = get_procedure_frame_id(string(current_option));

            if (flag_hash == ABBREVIATED_OUTPUT_FILE || flag_hash == OUTPUT_FILE) {
                options.output_file = argv[i + 1];
            }else if (flag_hash == COMPILER_DEBUG_FLAG ||flag_hash == ABBREVIATED_COMPILER_DEBUG_FLAG) {
                if (i+1 >= argc) {
                    ErrorLogger::get().log_error_unsafe(
                        null_token,
                        "--debug or -d compiler flag requires debug level input argument after.",
                        USER);
                    return options;
                }

                if (string(argv[i+1]) == "lexing") {
                    options.debug = LEXING;
                }else if (string(argv[i+1]) == "parsing") {
                    options.debug = PARSING;
                }else if (string(argv[i+1]) == "compilation") {
                    options.debug = COMPILATION;
                }else if (string(argv[i+1]) == "all") {
                    options.debug = ALL;
                }else {
                    ErrorLogger::get().log_error_unsafe(
                       null_token,
                       sen("Unrecgonized compiler flag:", string(current_option)),
                       USER);
                    break;
                }
                skip_next_iteration = true;
            }else if (flag_hash == ABBREVIATED_COMPILATION_TARGET || flag_hash == COMPILATION_TARGET) {
                if (string(current_option) == "sagevm") {
                    options.compilation_target = SAGE_VM;
                }else {
                    ErrorLogger::get().log_error_unsafe(
                        null_token,
                        sen(string(argv[i+1]), " is not implemented yet."),
                        GENERAL);
                    break;
                }
            }else if (flag_hash == ABBREVIATED_EMIT_BYTECODE || flag_hash == EMIT_BYTECODE) {
                options.emit_bytecode = true;
            }else if (flag_hash == ABBREVIATED_PRINT_BYTECODE || flag_hash == PRINT_BYTECODE) {
                options.debug_print_bytecode = true;
            }else {
                ErrorLogger::get().log_error_unsafe(
                    null_token,
                    sen("Unrecgonized compiler flag:", string(current_option)),
                    USER);
                break;
            }

            continue;
        }

        // only valid compiler option that doesn't start with a '-' is the input source file
        string filename(current_option);
        if (check_filename_valid(filename) && options.input_file.empty()) {
            options.input_file = filename;
        }
    }

    return options;
}

int main(int argc, char **argv) {
    // TODO: STOP using vector, its so annoying, we need to make our own to suite our use cases

    auto options = parse_compiler_flags(argc, argv);
    if (ErrorLogger::get().has_errors()) {
        ErrorLogger::get().report_errors();
        return 1;
    }

    SageCompiler compiler = SageCompiler(options);
    compiler.compile_file(compiler.options.input_file);

    if (ErrorLogger::get().has_errors()) {
        ErrorLogger::get().report_errors();
        return 1;
    }

    return 0;
}
