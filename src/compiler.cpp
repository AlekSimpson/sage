#include <stdlib.h>
#include <llvm/IR/Module.h>
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
        // llvm::errs() << "Target not found: " << error << "\n";
        printf("Target not found.\n");
        return false;
    }

    auto CPU = "x86-64";
    auto features = "";
    llvm::TargetOptions options;
    auto RM = llvm::Optional<llvm::Reloc::Model>();
    auto target_machine = target->createTargetMachine(target_triple, CPU, features, options, RM);

    // Set up module layout and triple
    module->setDataLayout(target_machine->createDataLayout());
    module->setTargetTriple(target_triple);

    // Create object file
    std::error_code EC;
    llvm::raw_fd_ostream dest(output_file, EC, llvm::sys::fs::OF_None);
    if (EC) {
        // llvm::errs() << "Could not open file: " << EC.message() << "\n";
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

    printf("About to run pass manager on module at %p\n", (void*)module);
    printf("Module name: %s\n", module->getName().str().c_str());
    printf("Module has %zu functions\n", module->size());

    std::string verify_error;
    if (llvm::verifyModule(*module, &llvm::errs())) {
        printf("Module verification failed\n");
        return false;
    }

    // for debug purposes
    module->print(llvm::outs(), nullptr);

    pass.run(*module);
    dest.flush();

    std::string link_cmd = "clang -o sage.run sage.out";
    int result = system(link_cmd.c_str());

    return true;
}

