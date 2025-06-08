#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <unordered_map>
#include <uuid/uuid.h>
#include <memory>

#include "../include/symbols.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"

using namespace llvm;

SageCodeGenVisitor::SageCodeGenVisitor() {}

SageCodeGenVisitor::SageCodeGenVisitor(
    NodeManager* node_man, std::shared_ptr<llvm::LLVMContext> context,
    SageInterpreter* _vm
) {
    llvm_context = context;
    builder = make_unique<llvm::IRBuilder<>>(*llvm_context);
    main_module = make_unique<llvm::Module>("main_module", *llvm_context);
    symbol_table = SageSymbolTable();
    symbol_table.initialize(main_module.get(), *llvm_context);
    node_manager = node_man;
    vm = _vm;
}

llvm::Module* SageCodeGenVisitor::get_module() {
    return main_module.get();
}

void SageCodeGenVisitor::visit_program(NodeIndex node) {
    for (auto child : node_manager->get_children(node)) {
        switch (node_manager->get_nodetype(child)) {
            case PN_FUNCDEF:
                break;
            case PN_FUNCDEC:
                break;
            case PN_STRUCT:
                break
            case PN_INCLUDE:
                break
            case PN_RUN_DIRECTIVE:
                break;
            default:
                // ERROR
                break;
        }
    }
}

void SageCodeGenVisitor::visit_create_function_return(optional<uuid_t > valueid = std::nullopt) {
    symbol_table.current_function_has_returned = true;
    if (valueid.has_value()) {
        return;
    }

    // return with no value
}

void SageCodeGenVisitor::visit_function_declaration(NodeIndex node) {
    // binary node
    //  - unary     <-- function name
    //  - binary
    //      - block <-- function parameters
    //      - unary <-- function return type
    
    if (node_manager->get_host_nodetype(node_manager->get_right(node)) != PN_BINARY) {
        // ERROR!
        return;
    }
    NodeIndex rightnode = node_manager->get_right(node);

    vector<llvm::Type*> parameter_types;
    vector<NodeIndex> parameters = node_manager->get_children(node_manager->get_left(rightnode));

    NodeIndex temp;
    for (NodeIndex parameter : parameters) {
        temp = node_manager->get_right(parameter);
        // llvm::Type* ir_type = symbol_table.resolve_llvm_type(node_manager, temp);
        parameter_types.push_back(ir_type);
    }

    llvm::Type* return_type = symbol_table.resolve_llvm_type(node_manager, node_manager->get_right(rightnode));
    string function_name = node_manager->get_lexeme(node_manager->get_left(node));
    bool is_vararg = (node_manager->get_nodetype(parameters.at(parameters.size()-1)) == PN_VARARG);
    if (is_vararg) {
        parameter_types.pop_back();
    }

    // llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, parameter_types, is_vararg);
    // llvm::Value* llvmvalue = llvm::Function::Create(
    //     function_type,
    //     llvm::Function::ExternalLinkage, 
    //     function_name,
    //     main_module.get() 
    // );
    // VisitorValue value = VisitorValue(llvmvalue, SageIntValue(0, 8))

    // add to symbol table
    if (symbol_table.declare_symbol(function_name, function_type, value)) {
        // ERROR!!
        return;
    }
}

void SageCodeGenVisitor::visit_function_declaration(NodeIndex node) {
    symbol_table.push_scope();
    symbol_table.current_function_has_returned = false;

    // TODO: add function to symbol table
    auto right_host = node_manager->get_host_nodetype(node_manager->get_right(node));
    if (right_host != PN_TRINARY) {
        // ERROR!!
        return;
    }
    NodeIndex trinary_node = node_manager->get_right(node);
    
    vector<llvm::Type*> parameter_types;
    vector<string> parameter_names;
    vector<NodeIndex> parameters = node_manager->get_children(node_manager->get_left(trinary_node));
    for (NodeIndex parameter : parameters) {
        if (node_manager->get_host_nodetype(parameter) != PN_BINARY) {
            // ERROR!! SOMETHING IS WRONG PARAMETERS SHOULD NEVER BE STORED AS ANYTHING OTHER THAN BINARY NODES
            return;
        }

        auto right_side = node_manager->get_right(parameter);
        if (node_manager->get_host_nodetype(right_side) != PN_UNARY) {
            // ERROR!! TYPE CANNOT BE REPRESENTED BY ANYTHING OTHER THAN UNARY FOR NOW
            return;
        }
        auto param_type = symbol_table.resolve_llvm_type(node_manager, right_side);

        // add to function scope
        string parameter_name = node_manager->get_lexeme(node_manager->get_left(parameter));
        parameter_names.push_back(parameter_name);
        bool already_exists = symbol_table.declare_symbol(parameter_name, param_type, nullptr);
        if (already_exists) {
            // ERROR!!
            return;
        }

        // add to function signature
        parameter_types.push_back(param_type);
    }

    if (node_manager->get_host_nodetype(node_manager->get_middle(trinary_node)) != PN_UNARY) {
        // ERROR!! TYPES ONLY REPRESENTED BY UNARY NODES RIGHT NOW
        return;
    }

    llvm::Type* return_type = symbol_table.resolve_llvm_type(node_manager, node_manager->get_middle(trinary_node));
    llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, parameter_types, false);

    string function_name = node_manager->get_lexeme(node_manager->get_left(node));
    llvm::Function* function_ir = llvm::Function::Create(
        function_type, 
        llvm::Function::ExternalLinkage, 
        function_name,
        main_module.get()
    );
    llvm::BasicBlock* function_block = llvm::BasicBlock::Create(*llvm_context, "entry", function_ir);
    builder->SetInsertPoint(function_block);

    auto right_of_trinary = node_manager->get_right(trinary_node);
    if (node_manager->get_host_nodetype(right_of_trinary) != PN_BLOCK) {
        // ERROR!! RIGHT NODE OF FUNC DEF NODE SHOULD ONLY EVER BE A BLOCK NODE
        return;
    }

    build_function_with_block(
        parameter_names,
        function_name,
    );

    visit_codeblock(right_of_trinary);

    if (!symbol_table.current_function_has_returned) {
        if (return_type != llvm::Type::getVoidTy(*llvm_context)) {
            // error: function was defined with no return statement
            symbol_table.pop_scope();
            return;
        }

        build_return();
    }

    symbol_table.pop_scope();
}






