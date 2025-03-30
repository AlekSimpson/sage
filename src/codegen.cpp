#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <unordered_map>
#include <memory>

#include "../include/parse_node.h"
#include "../include/symbols.h"
#include "../include/codegen.h"

using namespace llvm;

SageCodeGenVisitor::SageCodeGenVisitor() {
    llvm_context = make_unique<llvm::LLVMContext>();
    builder = make_unique<llvm::IRBuilder<>>(*llvm_context);
    main_module = make_unique<llvm::Module>("main_module", *llvm_context);
    symbol_table = SageSymbolTable(*llvm_context);
}

llvm::Module* SageCodeGenVisitor::get_module() {
    return main_module.get();
}

// this is the first visitor called to compile a whole code module
llvm::Value* SageCodeGenVisitor::visit_program(BlockParseNode* node) {
    BinaryParseNode* binarynode;
    TrinaryParseNode* trinarynode;
    for (AbstractParseNode* child : node->children) {
        switch(child->get_nodetype()){
            case PN_FUNCDEF:
                if (child->get_host_nodetype() != PN_TRINARY) {
                    // ERROR!! CHILD SHOULD BE A TRINARY NODE
                    return nullptr;
                }
                trinarynode = dynamic_cast<TrinaryParseNode*>(child);
                visit_function_definition(trinarynode);
                break;
            case PN_FUNCDEC:
                if (child->get_host_nodetype() != PN_BINARY) {
                    // ERROR!! CHILD SHOULD BE A BINARY NODE
                    return nullptr;
                }
                binarynode = dynamic_cast<BinaryParseNode*>(child);
                visit_function_declaration(binarynode);
                break;
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

    return nullptr;
}

void SageCodeGenVisitor::visitor_create_function_return(llvm::Value* value) {
    symbol_table.current_function_has_returned = true;
    if (value != nullptr) {
        builder->CreateRet(value);
        return;
    }

    builder->CreateRetVoid();
}

llvm::Value* SageCodeGenVisitor::visit_function_declaration(BinaryParseNode* node) {
    symbol_table.push_scope();
    
    vector<llvm::Type*> parameter_types;
    vector<AbstractParseNode*> parameters = node->left->get_child_node();
    for (AbstractParseNode* abstract_parameter : parameters) {
        if (abstract_parameter->get_host_nodetype() != PN_BINARY) {
            // ERROR!! PARAMETERS ARE NEVER STORED AS ANYTHING OTHER THAN BINARY NODES
            return nullptr;
        }
        BinaryParseNode* parameter = dynamic_cast<BinaryParseNode*>(abstract_parameter);

        if (parameter->right->get_host_nodetype() != PN_UNARY) {
            // ERROR!!
            return nullptr;
        }
        UnaryParseNode* unarynode = dynamic_cast<UnaryParseNode*>(parameter->right);
        auto param_type = symbol_table.resolve_sage_type(unarynode);

        // add to function scope
        string parameter_name = parameter->left->get_token().lexeme;
        bool already_exists = symbol_table.declare_symbol(parameter_name, create_symbol(parameter_name, nullptr, param_type), false);
        if (already_exists) {
            // ERROR!!
            return nullptr;
        }

        // add to function signature
        parameter_types.push_back(param_type);
    }

    if (node->right->get_host_nodetype() != PN_UNARY) {
        // ERROR!! 
        return nullptr;
    }
    UnaryParseNode* unarynode = dynamic_cast<UnaryParseNode*>(node->right);
    llvm::Type* return_type = symbol_table.resolve_sage_type(unarynode);
    llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, parameter_types, false);
    llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, node->get_token().lexeme, main_module.get());

    symbol_table.pop_scope();

    return nullptr;
}

llvm::Value* SageCodeGenVisitor::visit_function_definition(TrinaryParseNode* node) {
    symbol_table.push_scope();
    symbol_table.current_function_has_returned = false;
    
    vector<llvm::Type*> parameter_types;
    vector<AbstractParseNode*> parameters = node->left->get_child_node();
    for (AbstractParseNode* abstract_parameter : parameters) {
        if (abstract_parameter->get_host_nodetype() != PN_BINARY) {
            // ERROR!! SOMETHING IS WRONG PARAMETERS SHOULD NEVER BE STORED AS ANYTHING OTHER THAN BINARY NODES
            return nullptr;
        }
        BinaryParseNode* parameter = dynamic_cast<BinaryParseNode*>(abstract_parameter);

        if (parameter->right->get_host_nodetype() != PN_UNARY) {
            // ERRROR!! TYPE CANNOT BE REPRESENTED BY ANYTHING OTHER THAN UNARY FOR NOW
            return nullptr;
        }
        UnaryParseNode* unarynode = dynamic_cast<UnaryParseNode*>(parameter->right);
        auto param_type = symbol_table.resolve_sage_type(unarynode);

        // add to function scope
        string parameter_name = parameter->left->get_token().lexeme;
        bool already_exists = symbol_table.declare_symbol(parameter_name, create_symbol(parameter_name, nullptr, param_type), false);
        if (already_exists) {
            // ERROR!!
            return nullptr;
        }

        // add to function signature
        parameter_types.push_back(param_type);
    }

    if (node->middle->get_host_nodetype() != PN_UNARY) {
        // ERROR!! TYPES ONLY REPRESENTED BY UNARY NODES RIGHT NOW
        return nullptr;
    }
    auto unarytemp = dynamic_cast<UnaryParseNode*>(node->middle);
    llvm::Type* return_type = symbol_table.resolve_sage_type(unarytemp);
    llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, parameter_types, false);
    llvm::Function* function_ir = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, node->get_token().lexeme, *main_module);
    llvm::BasicBlock* function_block = llvm::BasicBlock::Create(*llvm_context, "entry", function_ir);
    builder->SetInsertPoint(function_block);

    if (node->right->get_host_nodetype() != PN_BLOCK) {
        // ERROR!! RIGHT NODE OF FUNC DEF NODE SHOULD ONLY EVER BE A BLOCK NODE
        return nullptr;
    }
    auto function_body = dynamic_cast<BlockParseNode*>(node->right);
    visit_codeblock(function_body);

    if (!symbol_table.current_function_has_returned) {
        if (return_type != llvm::Type::getVoidTy(*llvm_context)) {
            // error: function was defined with no return statement
            symbol_table.pop_scope();
            return nullptr;
        }

        builder->CreateRetVoid();
    }

    symbol_table.pop_scope();

    return nullptr;
}

llvm::Value* SageCodeGenVisitor::visit_codeblock(BlockParseNode* node) {
    TrinaryParseNode* trinaryfill;
    BinaryParseNode* binaryfill;
    for (AbstractParseNode* child : node->children) {
        switch(child->get_nodetype()){
            case PN_FUNCDEF:
                trinaryfill = dynamic_cast<TrinaryParseNode*>(child);
                visit_function_definition(trinaryfill);
                break;
            case PN_FUNCDEC:
                binaryfill = dynamic_cast<BinaryParseNode*>(child);
                visit_function_declaration(binaryfill);
                break;
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

    return nullptr;
}

llvm::Value* SageCodeGenVisitor::visit_unary_expr(UnaryParseNode* node) {
    LLVMSymbol* symbol_value;
    
    switch(node->rep_nodetype) {
        case PN_VAR_REF: // PN_VAR_REF should do the same thing as PN_IDENTIFIER
        case PN_IDENTIFIER:
            symbol_value = symbol_table.lookup_symbol(node->get_token().lexeme);
            if (symbol_value == nullptr) {
                // ERROR!!
                return nullptr;
            }

            return symbol_value->value;
        case PN_KEYWORD:
            // leaving this print statement here to see if we even actually need this case, it might never actually be called in practice
            printf("VISITING UNARY PN_KEYWORD TYPE; UNIMPLEMENTED!!");
            break;
        case PN_ELSE_BRANCH:
            break;
        case PN_VAR_DEC:
            return visit_variable_decl(node);
        case PN_STRUCT:
            // leaving this print statement here to see if we even actually need this case, it might never actually be called in practice
            printf("VISITING UNARY PN_STRUCT TYPE; UNIMPLEMENTED!!");
            break;
        case PN_FUNCCALL:
            break;
        case PN_VARARG:
            break;
        case PN_NUMBER:
            // NOTE: we don't actually know if this is supposed to be an int32, it could be something, will have to see if this causes errors in testing
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*llvm_context), stoi(node->get_token().lexeme));
        case PN_FLOAT:
            return llvm::ConstantFP::get(llvm::Type::getFloatTy(*llvm_context), stof(node->get_token().lexeme));
        case PN_STRING:
            // return llvm::ConstantDataArray(*llvm_context, node->get_token().lexeme, true);
            return llvm::ConstantDataArray::getString(
                *llvm_context,
                node->get_token().lexeme,
                true // sets whether to add a null terminator or not
            );
        case PN_LIST:
            // TODO: later
            break;
        default:
            // ERROR!!
            break;
    }

    return nullptr;
}

llvm::Value* SageCodeGenVisitor::visit_trinary_expr(TrinaryParseNode* node) {
    switch(node->rep_nodetype) {
        case PN_FOR:
            break;
        case PN_FUNCDEF:
            return visit_function_definition(node);
        case PN_ASSIGN:
            return visit_variable_decl(node);
        default:
            break;
    }

    return nullptr;
}

void SageCodeGenVisitor::process_assignment(BinaryParseNode* node) {
    // NOTE: in the future to support heap memory reassignment we should maybe have something in the symbol table that indicates whether a value lives on the heap or not
    // so that we can inform this code section with what IR generation to use
    
    auto LHS = node->left;
    LLVMSymbol* assign_value = symbol_table.lookup_symbol(LHS->get_token().lexeme);
    if (assign_value == nullptr) {
        // ERROR!!
        return;
    }
    
    llvm::Value* RHS = visit_expression(node);
    
    builder->CreateStore(RHS, assign_value->value);
}

llvm::Value* SageCodeGenVisitor::visit_binary_expr(BinaryParseNode* node) {
    switch(node->rep_nodetype) {
        case PN_BINARY:
            return visit_expression(node);
        case PN_FUNCDEC:
            return visit_function_declaration(node);
        case PN_ASSIGN:
            // stack variable reassignment
            process_assignment(node);
            break;
        case PN_VAR_DEC: 
            // allocating space on stack without assigning value
            return visit_variable_decl(node);
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
            return nullptr; // ERRRO!! COMPLEX REPRESENTED TYPES DONT EXIST YET
            // return resolve_sage_type(llvm_context.get(), node);
        default:
            // error
            break;
    }

    return nullptr;
}

llvm::Value* SageCodeGenVisitor::visit_variable_decl(AbstractParseNode* node) {
    auto concrete_node_type = node->get_host_nodetype();
    if (concrete_node_type == PN_BINARY) {
        BinaryParseNode* binary_node = dynamic_cast<BinaryParseNode*>(node);

        // left is variable identifier
        string variable_name = binary_node->left->get_token().lexeme;
        
        // right is type identifier
        if (binary_node->right->get_host_nodetype() != PN_UNARY) {
            // ERROR!! FOR NOW NON UNARY REPRESENTED TYPES DONT EXIST
            return nullptr;
        }
        auto rightnode = dynamic_cast<UnaryParseNode*>(binary_node->right);
        auto type_ident = symbol_table.resolve_sage_type(rightnode);

        // update symbol table
        successful success = symbol_table.declare_symbol(variable_name, create_symbol(variable_name, nullptr, type_ident), false);
        if (!success) {
            // ERROR!!
            return nullptr;
        }

        llvm::AllocaInst* allocation = builder->CreateAlloca(
            type_ident,            // Type to allocate
            nullptr,               // Number of elements (nullptr = 1)
            variable_name          // Name of the allocation
        );

        return allocation;

    }else if (concrete_node_type == PN_TRINARY) {
        TrinaryParseNode* trinary_node = dynamic_cast<TrinaryParseNode*>(node);

        // left is variable identifier
        string variable_name = trinary_node->left->get_token().lexeme;
        
        // right is type identifier
        if (trinary_node->middle->get_host_nodetype() != PN_UNARY) {
            // ERROR!! FOR NOW NON UNARY REPRESENTED TYPES DONT EXIST
            return nullptr;
        }
        auto middlenode = dynamic_cast<UnaryParseNode*>(trinary_node->middle);
        auto type_ident = symbol_table.resolve_sage_type(middlenode);

        // update symbol table
        if (symbol_table.lookup_symbol(variable_name) != nullptr) {
            // ERROR!! symbol already exists
            return nullptr;
        }

        llvm::AllocaInst* allocation = builder->CreateAlloca(
            type_ident,            // Type to allocate
            nullptr,               // Number of elements (nullptr = 1)
            variable_name          // Name of the allocation
        );

        auto RHS = visit_expression(trinary_node->right);

        // don't need to check the success of this call because we already know this symbol doesn't already exist
        symbol_table.declare_symbol(variable_name, create_symbol(variable_name, allocation, type_ident), false);

        auto final = builder->CreateStore(RHS, allocation);

        return final;
    }

    // ERROR!!
    return nullptr;
}

llvm::Value* SageCodeGenVisitor::process_expression(BinaryParseNode* node) {
    auto LHS_node = node->left;
    auto RHS_node = node->right;
    
    auto LHS_ir = visit_expression(LHS_node);
    auto RHS_ir = visit_expression(RHS_node);
    
    // parse operator IR
    auto operator_tok = node->token;
    
    // EQUALITY, LT, GT, GTE, LTE
    // ADD, SUB, MUL, DIV, EXP
    // AND, OR, BIT_AND, BIT_OR
    switch(operator_tok.token_type) {
        case TT_EQUALITY: // TODO:
            break;
        case TT_LT: // TODO:
            break;
        case TT_GT: // TODO:
            break;
        case TT_GTE: // TODO:
            break;
        case TT_LTE: // TODO:
            break;
        case TT_ADD:
            return builder->CreateAnd(LHS_ir, RHS_ir, "addtmp");
        case TT_SUB:
            return builder->CreateSub(LHS_ir, RHS_ir, "subtmp");
        case TT_MUL:
            return builder->CreateMul(LHS_ir, RHS_ir, "multmp");
        case TT_DIV:
            return builder->CreateSDiv(LHS_ir, RHS_ir, "divtmp");
        case TT_EXP: // TODO:
            break;
        case TT_AND:
            return builder->CreateAnd(LHS_ir, RHS_ir, "andtmp");
        case TT_OR:
            return builder->CreateOr(LHS_ir, RHS_ir, "ortmp");
        case TT_BIT_AND: // TODO:
            break;
        case TT_BIT_OR: // TODO:
            break;
        default:
            // ERROR!!
            break;
    }

    return nullptr;
}

llvm::Value* SageCodeGenVisitor::visit_expression(AbstractParseNode* node) {
    BinaryParseNode* binarynode;
    UnaryParseNode* unarynode;

    switch (node->get_nodetype()) {
        case PN_BINARY:
            binarynode = dynamic_cast<BinaryParseNode*>(node);
            return process_expression(binarynode);

        case PN_UNARY:
            unarynode = dynamic_cast<UnaryParseNode*>(node);
            return visit_unary_expr(unarynode);

        case PN_RANGE:
            break; // TODO: later
 
        default:
            // ERROR!!
            break;
    }

    return nullptr;
}
