#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <unordered_map.h>
#include <memory>

#include "../include/parse_node.h"
#include "../include/codegen.h"

using namespace llvm;

llvm::Type* resolve_sage_type(LLVMContext* context, UnaryParseNode* type_node) {
    unordered_map<string, llvm::Type*> typemap = {
        {"bool", llvm::Type::getInt1Ty(*context)},
        {"char", llvm::Type::getInt8Ty(*context)},
        {"int", llvm::Type::getInt32Ty(*context)},
        {"i8", llvm::Type::getInt8Ty(*context)},
        {"i32", llvm::Type::getInt32Ty(*context)},
        {"i64", llvm::Type::getInt64Ty(*context)},
        {"float", llvm::Type::getFloatTy(*context)},
        {"f32", llvm::Type::getFloatTy(*context)},
        {"f64", llvm::Type::getDoubleTy(*context)},
        {"void", llvm::Type::getVoidTy(*context)}
    };

    return typemap[type_node->token->lexeme];
}

SageCodeGenVisitor::SageCodeGenVisitor() {
    llvm_context = make_unique<llvm::LLVMContext>();
    global_table = SageSymbolTable();
    builder = make_unique<llvm::IRBuilder<>>(*llvm_context);
    main_module = make_unique<llvm::Module>("main_module", llvm_context);
    symbol_table = CompilerContext();
}

// this is the first visitor called to compile a whole code module
llvm::Value* visit_program(BlockParseNode* node) {
    for (AbstractParseNode* child : node->children) {
        auto nodetype = node_to_string(child->rep_nodetype);
        switch(nodetype){
            case PN_FUNCDEF:
                return visit_function_definition(child);
            case PN_FUNCDEC:
                return visit_function_declaration(child);
            case PN_STRUCT:
                break;
            case PN_INCLUDE:
                break;
            case PN_RUN_DIRECTIVE:
                break;
            default:
                // error
                break;
        }
    }
}

void SageCodeGenVisitor::visitor_create_function_return(llvm::Value* value) {
    symbol_table.current_function_has_returned = true;
    if (value != nullptr) {
        builder.CreateRet(value);
        return;
    }

    builder.CreateRetVoid();
}

llvm::Value* SageCodeGenVisitor::visit_function_declaration(BinaryParseNode* node) {
    symbol_table.push_scope();
    
    vector<llvm::Type*> parameter_types;
    for (BinaryParseNode* parameter : node->left->children) {
        auto param_type = resolve_sage_type(llvm_context, parameter->right);

        // add to function scope
        string parameter_name = parameter->left->token->lexeme;
        symbol_table.declare_symbol(parameter_name, param_type);

        // add to function signature
        parameter_types.push_back(param_type);
    }

    llvm::Type* return_type = resolve_sage_type(llvm_context, node->right);
    llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, parameter_types, false);
    llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, node->token->lexeme, main_module);

    symbol_table.pop_scope();
}

llvm::Value* SageCodeGenVisitor::visit_function_definition(TrinaryParseNode* node) {
    symbol_table.push_scope();
    symbol_table.current_function_has_returned = false;
    
    vector<llvm::Type*> parameter_types;
    for (BinaryParseNode* parameter : node->left->children) {
        auto param_type = resolve_sage_type(llvm_context, parameter->right);

        // add to function scope
        string parameter_name = parameter->left->token->lexeme;
        symbol_table.declare_symbol(parameter_name, param_type);

        // add to function signature
        parameter_types.push_back(param_type);
    }

    llvm::Type* return_type = resolve_sage_type(llvm_context, node->middle);
    llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, parameter_types, false);
    llvm::Function* function_ir = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, node->token->lexeme, main_module);
    llvm::BasicBlock* function_block = llvm::BasicBlock::Create(llvm_context, "entry", function_ir);
    builder.SetInsertPoint(function_block);

    visit_codeblock(node->right);

    if (!symbol_table.current_function_has_returned) {
        if (return_type != llvm::Type::getVoidTy(*llvm_context)) {
            // error: function was defined with no return statement
            symbol_table.pop_scope();
            return nullptr;
        }

        builder.CreateRetVoid();
    }

    symbol_table.pop_scope();
}

llvm::Value* SageCodeGenVisitor::visit_codeblock(BlockParseNode* node) {
    for (AbstractParseNode* child : node->children) {
        auto nodetype = node_to_string(child->rep_nodetype);
        switch(nodetype){
            case PN_FUNCDEF:
                return visit_function_definition(child);
            case PN_FUNCDEC:
                return visit_function_declaration(child);
            case PN_STRUCT:
                break;
            case PN_IF:
                break;
            case PN_RUN_DIRECTIVE:
                break;
            default:
                // error
                break;
        }
    }
}

llvm::Value* SageCodeGenVisitor::visit_unary_expr(UnaryParseNode* node) {
    auto nodetype = node_to_string(node->rep_nodetype);
    
    switch(nodetype) {
        case PN_IDENTIFIER:
            break;
        case PN_KEYWORD:
            break;
        case PN_ELSE_BRANCH:
            break;
        case PN_VAR_DEC:
            break;
        case PN_STRUCT:
            break;
        case PN_TYPE:
            break;
        case PN_FUNCCALL:
            break;
        case PN_VARARG:
            break;
        case PN_NUMBER:
            break;
        case PN_FLOAT:
            break;
        case PN_VAR_REF:
            break;
        case PN_STRING:
            break;
        case PN_LIST:
            break;
        default: 
            break;
    }
}

llvm::Value* SageCodeGenVisitor::visit_trinary_expr(TrinaryParseNode* node) {
    auto nodetype = node_to_string(node->rep_nodetype);

    switch(nodetype) {
        case PN_FOR:
            break;
        case PN_FUNCDEF:
            break;
        case PN_ASSIGN:
            break;
        default:
            break;
    }
}

llvm::Value* SageCodeGenVisitor::visit_binary_expr(BinaryParseNode* node) {
    auto nodetype = node_to_string(node->rep_nodetype);

    switch(nodetype) {
        case PN_BINARY:
            break;
        case PN_FUNCDEC:
            break;
        case PN_ASSIGN:
            break;
        case PN_VAR_DEC:
            break;
        case PN_VARARG:
            break;
        case PN_IF_BRANCH:
            break;
        case PN_WHILE:
            break;
        case PN_RANGE:
            break;
        case PN_TYPE:
            break;
        default:
            // error
            break;
    }
}

