#include <stdlib.h>
/*#include <llvm/IR/Module.h>*/
#include <boost/algorithm/string.hpp>
// #include "llvm/IR/LLVMContext.h"
// #include "llvm/Support/TargetSelect.h"
// #include "llvm/Target/TargetMachine.h"
// #include "llvm/Target/TargetOptions.h"
// #include "llvm/MC/TargetRegistry.h"
// #include "llvm/Support/FileSystem.h"
// #include "llvm/Support/Host.h"
// #include "llvm/Support/raw_ostream.h"
// #include "llvm/Support/Program.h"
// #include "llvm/Support/Path.h"
// #include "llvm/IR/LegacyPassManager.h"
// #include "llvm/IR/Verifier.h"

#include "../include/codegen.h"
#include "../include/parser.h"
#include "../include/node_manager.h"
#include "../include/symbols.h"
#include "../include/sage_bytecode.h"

using namespace std;
/*using namespace llvm;*/

SageCompiler::SageCompiler() {}

SageCompiler::SageCompiler(string mainfile)
    : ast(NULL_INDEX),
      debug(COMPILATION),
      node_manager(new NodeManager()),
      parser(SageParser(node_manager, mainfile)),
      interpreter(new SageInterpreter(4046)),
      analyzer(new SageAnalyzer(node_manager)),
      symbol_table(SageSymbolTable()) {
    // visitor(SageCodeGenVisitor(node_manager, interpreter, analyzer)) {

    symbol_table.initialize();

    current_procedure.push(0);
    procedures.push_back(vector<command>());
}

SageCompiler::~SageCompiler() {
    delete node_manager;
    delete interpreter;
    delete analyzer;
    /*llvm::llvm_shutdown();*/
}

NodeIndex SageCompiler::parse_codefile(string target_file) {
    parser.filename = target_file;
    NodeIndex parsetree = parser.parse_program(false);
    if (parsetree == NULL_INDEX) {
        printf("parsetree root is null. parsing failed.\n");
        return NULL_INDEX;
    }
    return parsetree;
}

bool SageCompiler::compile(NodeIndex ast) {
    if (ast == NULL_INDEX || node_manager->get_host_nodetype(ast) != PN_BLOCK) {
        return false;
    }

    // TODO: need to figure out eventually how to work with packages and have the linker link previously compiled sage object files to the current project
    for (auto child : node_manager->get_children(ast)) {
        if (node_manager->get_nodetype(child) == PN_INCLUDE) {
            string codefile = node_manager->get_lexeme(child);
            codefile.erase(std::remove(codefile.begin(), codefile.end(), '"'), codefile.end());
            codefile = codefile + ".sage";
            codefile = "sage_modules/" + codefile;
            NodeIndex file_ast = parse_codefile(codefile);
            this->visitor.visit_program(file_ast);
        }
    }

    this->visitor.visit_program(ast);

    size_t total_size = 0;
    for (const auto& vec : visitor.procedures) {
        total_size += vec.size();
    }
    visitor.total_bytecode.reserve(total_size);

    for (const auto& vec : visitor.procedures) {
        visitor.total_bytecode.insert(visitor.total_bytecode.end(), vec.begin(), vec.end());
    }

    return true;
}

// successful SageCompiler::emit_and_link_llvm(llvm::Module* module, const std::string& output_file) {
//     llvm::InitializeAllTargetInfos();
//     llvm::InitializeAllTargets();
//     llvm::InitializeAllTargetMCs();
//     llvm::InitializeAllAsmParsers();
//     llvm::InitializeAllAsmPrinters();
// 
//     // Get target machine
//     auto target_triple = "x86_64-pc-linux-gnu";
//     module->setTargetTriple(target_triple);
// 
//     std::string error;
//     const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, error);
// 
//     if (target == nullptr) {
//         printf("Target not found.\n");
//         return false;
//     }
// 
//     auto CPU = "x86-64";
//     auto features = "";
//     llvm::TargetOptions options;
//     auto RM = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Static);
//     auto target_machine = target->createTargetMachine(target_triple, CPU, features, options, RM);
// 
//     // Set up module layout and triple
//     module->setDataLayout(target_machine->createDataLayout());
//     module->setTargetTriple(target_triple);
// 
//     // Create object file
//     std::error_code EC;
//     llvm::raw_fd_ostream dest(output_file, EC, llvm::sys::fs::OF_None);
//     if (EC) {
//         printf("Could not open file.\n");
//         return false;
//     }
// 
//     llvm::legacy::PassManager pass;
//     auto file_type = llvm::CGFT_ObjectFile;
//     if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
//         llvm::errs() << "TargetMachine can't emit a file of this type\n";
//         return false;
//     }
// 
//     if (module == nullptr) {
//         printf("MODULE IS NULL\n");
//     }
// 
//     if (debug == COMPILATION || debug == ALL) {
//         // for debug purposes
//         module->print(llvm::outs(), nullptr);
//     }
// 
//     std::string verify_error;
//     if (llvm::verifyModule(*module, &llvm::errs())) {
//         printf("Module verification failed\n");
//         return false;
//     }
// 
//     pass.run(*module);
//     dest.flush();
// 
//     std::string link_cmd = "clang -no-pie sage.out -o sage.run";
//     system(link_cmd.c_str());
// 
//     return true;
// }

void SageCompiler::begin_compilation(string mainfile) {
    if (!check_filename_valid(mainfile)) {
        printf("Main program filename is not valid, Make sure it ends in '.sage'.\n");
        return;
    }

    NodeIndex program_parsetree = parser.parse_program(debug == PARSING || debug == ALL);
    if (program_parsetree == -1) {
        printf("Program parsetree root was null. Parsing failed.\n");
        return;
    }

    if (debug >= COMPILATION) {
        node_manager->showtree(program_parsetree);
    }

    // 1. generate program identifier dependencies
    auto dependencies = analyzer->detect_identifier_dependencies();
    for (const auto& [scopename, dep]: dependencies) {
        if (!dep->dependencies_are_valid()) {
            // REPORT ERROR!!!!
            // TODO: add way for missing dependencies to be named in the err messages
            printf("undefined variable reference was found.\n");
        }
    }

    // 2. program static analysis
    analyzer->perform_static_analysis(program_parsetree);

    // 3. program compilation
    bool success = compile(program_parsetree);
    if (!success) {
        printf("Compilation failed.\n");
        return;
    }

    ///////////////
    interpreter->load_program(runtime_bytecode);
    interpreter->execute();
    interpreter->close();
    // ^^^^^^^^TEMP!!!

    // 4. Compiling to target instructions (if sagevm not the target)

    // 5. Linking
    /*bool success = emit_and_link_llvm(module, "sage.out"); */

    if (success) {
        printf("Compilation finished successfully.\n");
    }else {
        printf("Compilation finished unsuccessfully.\n");
    }
}

bool SageCompiler::check_filename_valid(string filename) {
    // validate that file is valid
    if (filename.find('.') == string::npos) {
        printf("cannot target files that have no file extension.\n");
        return false;
    }

    vector<string> delimited;
    boost::split(delimited, filename, boost::is_any_of("."));

    if (delimited[1] != "sage") {
        printf("cannot target non sage source files.\n");
        return false;
    }

    return true;
}

