#include <stdlib.h>
#include <llvm/IR/Module.h>
#include <boost/algorithm/string.hpp>
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Path.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"

#include "../include/codegen.h"
#include "../include/parser.h"
#include "../include/parse_node.h"
#include "../include/symbols.h"

using namespace llvm;

SageCompiler::SageCompiler(SageAST* ast, std::shared_ptr<llvm::LLVMContext> context) {
    this->ast = ast;
    this->visitor = SageCodeGenVisitor(context);
}

SageCompiler::~SageCompiler() {
    delete ast;
    llvm::llvm_shutdown();
}

SageAST* SageCompiler::parse_codefile(string target_file) {
    // validate that file is valid
    if (target_file.find('.') == string::npos) {
        printf("cannot target files that have no file extension.\n");
        exit(1);
    }

    vector<string> delimited;
    boost::split(delimited, target_file, boost::is_any_of("."));

    if (delimited[1] != "sage") {
        printf("cannot target non sage source files.\n");
        exit(1);
    }

    SageParser parser = SageParser(target_file);
    AbstractParseNode* parsetree = parser.parse_program(false);
    if (parsetree == nullptr) {
        printf("parsetree root is null. parsing failed.\n");
        return nullptr;
    }
    return parsetree;
}

llvm::Module* SageCompiler::compile_module() {
    if (ast->get_host_nodetype() == PN_BLOCK) {
        vector<string> filenames;

        auto program_block = dynamic_cast<BlockParseNode*>(ast);
        for (auto child : program_block->children) {
            if (child->get_nodetype() == PN_INCLUDE) {
                string codefile = child->get_token().lexeme;
                codefile.erase(std::remove(codefile.begin(), codefile.end(), '"'), codefile.end());
                codefile = codefile + ".sage";
                filenames.push_back(codefile);
            }
        }

        for (string filename : filenames) {
            string file = "sage_modules/" + filename;
            SageAST* file_ast = parse_codefile(file);

            auto program_block = dynamic_cast<BlockParseNode*>(file_ast);
            this->visitor.visit_program(program_block);
        }

        auto main_block = dynamic_cast<BlockParseNode*>(ast);
        this->visitor.visit_program(main_block);

        return this->visitor.get_module();
    }

    // ERROR!! AST root must be a PN BLOCK node
    return nullptr;
}

llvm::Module* SageCompiler::compile() {
    if (ast->get_host_nodetype() == PN_BLOCK) {
        auto program_block = dynamic_cast<BlockParseNode*>(ast);
        this->visitor.visit_program(program_block);
        return this->visitor.get_module();
    }

    printf("Expected parsetree root to be a block node.\n");
    return nullptr;
}

void SageCompiler::optimize(llvm::Module* module, int level) {} // TODO:

successful SageCompiler::generate_output(llvm::Module* module, const std::string& output_file) {
    // Initialize all targets
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    // Get target machine
    auto target_triple = "x86_64-pc-linux-gnu";
    module->setTargetTriple(target_triple);

    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, error);

    if (!target) {
        printf("Target not found.\n");
        return false;
    }

    auto CPU = "x86-64";
    auto features = "";
    llvm::TargetOptions options;
    auto RM = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Static);
    auto target_machine = target->createTargetMachine(target_triple, CPU, features, options, RM);

    // Set up module layout and triple
    module->setDataLayout(target_machine->createDataLayout());
    module->setTargetTriple(target_triple);

    // Create object file
    std::error_code EC;
    llvm::raw_fd_ostream dest(output_file, EC, llvm::sys::fs::OF_None);
    if (EC) {
        printf("Could not open file.\n");
        return false;
    }

    llvm::legacy::PassManager pass;
    auto file_type = llvm::CGFT_ObjectFile;
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
        llvm::errs() << "TargetMachine can't emit a file of this type\n";
        return false;
    }

    if (module == nullptr) {
        printf("MODULE IS NULL\n");
    }

    // for debug purposes
    module->print(llvm::outs(), nullptr);

    std::string verify_error;
    if (llvm::verifyModule(*module, &llvm::errs())) {
        printf("Module verification failed\n");
        return false;
    }

    pass.run(*module);
    dest.flush();

    std::string link_cmd = "clang -no-pie sage.out -o sage.run";
    system(link_cmd.c_str());

    return true;
}

