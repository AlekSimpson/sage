#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <unordered_map>
#include <memory>

#include "../include/symbols.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"

using namespace llvm;


SageCodeGenVisitor::SageCodeGenVisitor() {}

SageCodeGenVisitor::SageCodeGenVisitor(NodeManager* node_man, std::shared_ptr<llvm::LLVMContext> context) {
    llvm_context = context;
    builder = make_unique<llvm::IRBuilder<>>(*llvm_context);
    main_module = make_unique<llvm::Module>("main_module", *llvm_context);
    symbol_table = SageSymbolTable();
    symbol_table.initialize(main_module.get(), *llvm_context);
    node_manager = node_man;
}

llvm::Module* SageCodeGenVisitor::get_module() {
    return main_module.get();
}

// this is the first visitor called to compile a whole code module
llvm::Value* SageCodeGenVisitor::visit_program(NodeIndex node) {
    for (NodeIndex child : node_manager->get_children(node)) {
        switch(node_manager->get_nodetype(child)){
            case PN_FUNCDEF:
                if (node_manager->get_host_nodetype(child) != PN_BINARY) {
                    // ERROR!! CHILD SHOULD BE A BINARY NODE
                    return nullptr;
                }
                visit_function_definition(child);
                break;
            case PN_FUNCDEC:
                if (node_manager->get_host_nodetype(child) != PN_BINARY) {
                    // ERROR!! CHILD SHOULD BE A BINARY NODE
                    return nullptr;
                }
                visit_function_declaration(child);
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

llvm::Value* SageCodeGenVisitor::visit_function_declaration(NodeIndex node) {
    // binary node
    //  - unary     <-- function name
    //  - binary
    //      - block <-- function parameters
    //      - unary <-- function return type

    if (node_manager->get_host_nodetype(node_manager->get_right(node)) != PN_BINARY) {
        // ERROR!!
        return nullptr;
    }
    NodeIndex rightnode = node_manager->get_right(node);

    vector<llvm::Type*> parameter_types;
    vector<NodeIndex> parameters = node_manager->get_children(node_manager->get_left(rightnode));

    NodeIndex temp;
    for (NodeIndex parameter : parameters) {
        temp = node_manager->get_right(parameter);
        llvm::Type* ir_type = symbol_table.resolve_sage_type(node_manager, temp);
        parameter_types.push_back(ir_type);
    }

    llvm::Type* return_type = symbol_table.resolve_sage_type(node_manager, node_manager->get_right(rightnode));
    string function_name = node_manager->get_lexeme(node_manager->get_left(node));
    bool is_vararg = (node_manager->get_nodetype(parameters.at(parameters.size()-1)) == PN_VARARG);
    if (is_vararg) {
        parameter_types.pop_back();
    }

    llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, parameter_types, is_vararg);
    llvm::Value* llvmvalue = llvm::Function::Create(
        function_type, 
        llvm::Function::ExternalLinkage, 
        function_name, 
        main_module.get()
    );

    // add to symbol table
    if (symbol_table.declare_symbol(function_name, create_symbol(function_name, llvmvalue, function_type), false)) {
        // ERROR!!
    }

    return nullptr;
}

llvm::Value* SageCodeGenVisitor::visit_function_definition(NodeIndex node) {
    symbol_table.push_scope();
    symbol_table.current_function_has_returned = false;

    // TODO: add function to symbol table
    auto right_host = node_manager->get_host_nodetype(node_manager->get_right(node));
    if (right_host != PN_TRINARY) {
        // ERROR!!
        return nullptr;
    }
    NodeIndex trinary_node = node_manager->get_right(node);
    
    vector<llvm::Type*> parameter_types;
    vector<NodeIndex> parameters = node_manager->get_children(node_manager->get_left(trinary_node));
    for (NodeIndex parameter : parameters) {
        if (node_manager->get_host_nodetype(parameter) != PN_BINARY) {
            // ERROR!! SOMETHING IS WRONG PARAMETERS SHOULD NEVER BE STORED AS ANYTHING OTHER THAN BINARY NODES
            return nullptr;
        }
        // BinaryParseNode* parameter = dynamic_cast<BinaryParseNode*>(abstract_parameter);

        auto right_side = node_manager->get_right(parameter);
        if (node_manager->get_host_nodetype(right_side) != PN_UNARY) {
            // ERROR!! TYPE CANNOT BE REPRESENTED BY ANYTHING OTHER THAN UNARY FOR NOW
            return nullptr;
        }
        auto param_type = symbol_table.resolve_sage_type(node_manager, right_side);

        // add to function scope
        string parameter_name = node_manager->get_lexeme(node_manager->get_left(parameter));
        bool already_exists = symbol_table.declare_symbol(parameter_name, create_symbol(parameter_name, nullptr, param_type), false);
        if (already_exists) {
            // ERROR!!
            return nullptr;
        }

        // add to function signature
        parameter_types.push_back(param_type);
    }

    if (node_manager->get_host_nodetype(node_manager->get_middle(trinary_node)) != PN_UNARY) {
        // ERROR!! TYPES ONLY REPRESENTED BY UNARY NODES RIGHT NOW
        return nullptr;
    }

    llvm::Type* return_type = symbol_table.resolve_sage_type(node_manager, node_manager->get_middle(trinary_node));
    llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, parameter_types, false);
    llvm::Function* function_ir = llvm::Function::Create(
        function_type, 
        llvm::Function::ExternalLinkage, 
        node_manager->get_lexeme(node_manager->get_left(node)),
        main_module.get()
    );
    llvm::BasicBlock* function_block = llvm::BasicBlock::Create(*llvm_context, "entry", function_ir);
    builder->SetInsertPoint(function_block);

    auto right_of_trinary = node_manager->get_right(trinary_node);
    if (node_manager->get_host_nodetype(right_of_trinary) != PN_BLOCK) {
        // ERROR!! RIGHT NODE OF FUNC DEF NODE SHOULD ONLY EVER BE A BLOCK NODE
        return nullptr;
    }

    visit_codeblock(right_of_trinary);

    if (!symbol_table.current_function_has_returned) {
        if (return_type != llvm::Type::getVoidTy(*llvm_context)) {
            // error: function was defined with no return statement
            symbol_table.pop_scope();
            return nullptr;
        }

        // TODO: have builder create correct corresponding return type using 'return_type'
        builder->CreateRetVoid();
    }

    symbol_table.pop_scope();

    return nullptr;
}

llvm::Value* SageCodeGenVisitor::visit_function_call(NodeIndex node) {
    NodeIndex args_node = node_manager->get_branch(node);
    string func_call_name = node_manager->get_lexeme(node);
    vector<llvm::Value*> args;

    // retrieve function from symbol table
    LLVMSymbol* symbol = symbol_table.lookup_symbol(func_call_name);
    if (symbol == nullptr) {
        // ERROR!!
        return nullptr;
    }

    int index = 0;
    for (NodeIndex arg : node_manager->get_children(args_node)) {
        // TODO:varargs breaks this kinda, fix later
        //if (index >= args_node->children.size()) {
        //    // ERROR!!
        //    return nullptr;
        //}

        // TODO: varargs breaks this kinda, fix later
        // auto irtype = symbol_table.derive_sage_type(unaryarg);
        //if (irtype != symbol->type->getParamType(index)) {
        //    // ERROR!!
        //    return nullptr;
        //}

        llvm::Value* arg_value = visit_unary_expr(arg);
        args.push_back(arg_value);

        index++;
    }

    string result_name = func_call_name + "_result";
    auto call_ir = builder->CreateCall(llvm::cast<llvm::Function>(symbol->value), args, result_name);

    return call_ir;
}

llvm::Value* SageCodeGenVisitor::visit_codeblock(NodeIndex node) {
    llvm::Value* retval;
    for (NodeIndex child : node_manager->get_children(node)) {
        switch (node_manager->get_host_nodetype(child)) {
            case PN_UNARY:
                retval = visit_unary_expr(child);
                break;
            case PN_BINARY:
                retval = visit_binary_expr(child);
                break;
            case PN_TRINARY:
                retval = visit_trinary_expr(child);
                break;
            default:
                printf("VISIT CODEBLOCK GETTING HERE\n");
                printf("%s\n", nodetype_to_string(node_manager->get_nodetype(child)).c_str());
                break;
        }
    }

    return retval;
}

llvm::Value* SageCodeGenVisitor::visit_unary_expr(NodeIndex node) {
    LLVMSymbol* symbol_value;
    llvm::Value* deref_stack_value;
    llvm::Value* ptrvalue;
    string node_lexeme = node_manager->get_lexeme(node);
    
    switch(node_manager->get_nodetype(node)) {
        case PN_VAR_REF: // PN_VAR_REF should do the same thing as PN_IDENTIFIER
        case PN_IDENTIFIER:
            symbol_value = symbol_table.lookup_symbol(node_lexeme);
            if (symbol_value == nullptr) {
                // ERROR!!
                return nullptr;
            }

            deref_stack_value = builder->CreateLoad(
                symbol_value->type,   // Type being loaded
                symbol_value->value,  // Pointer to load from
                node_lexeme           // Name for the loaded value
            );

            return deref_stack_value;
        case PN_KEYWORD:
            // leaving this print statement here to see if we even actually need this case, it might never actually be called in practice
            printf("VISITING UNARY PN_KEYWORD TYPE; UNIMPLEMENTED!!\n");
            break;
        case PN_ELSE_BRANCH:
            break;
        case PN_VAR_DEC:
            return visit_variable_decl(node);
        case PN_STRUCT:
            // leaving this print statement here to see if we even actually need this case, it might never actually be called in practice
            printf("VISITING UNARY PN_STRUCT TYPE; UNIMPLEMENTED!!\n");
            break;
        case PN_FUNCCALL:
            return visit_function_call(node);
        case PN_NUMBER:
            // NOTE: we don't actually know if this is supposed to be an int64, it could be something, will have to see if this causes errors in testing
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvm_context), stoi(node_lexeme));
        case PN_FLOAT:
            return llvm::ConstantFP::get(llvm::Type::getFloatTy(*llvm_context), stof(node_lexeme));
        case PN_STRING:
            // TODO: probably need a function that wil process the raw string lexeme's into string format rules and strip away \"\"
            node_lexeme.erase(std::remove(node_lexeme.begin(), node_lexeme.end(), '"'), node_lexeme.end());

            ptrvalue = builder->CreateGlobalStringPtr(
                node_lexeme,
                "str_const"
            );

            return ptrvalue;

        case PN_LIST:
            // TODO: later
            break;
        default:
            // ERROR!!
            break;
    }

    return nullptr;
}

llvm::Value* SageCodeGenVisitor::visit_trinary_expr(NodeIndex node) {
    switch(node_manager->get_nodetype(node)) {
        case PN_FOR:
            break;
        case PN_ASSIGN:
            return visit_variable_decl(node);
        default:
            break;
    }

    return nullptr;
}

void SageCodeGenVisitor::process_assignment(NodeIndex node) {
    // NOTE: in the future to support heap memory reassignment we should maybe have something in the symbol table that indicates whether a value lives on the heap or not
    // so that we can inform this code section with what IR generation to use
    
    NodeIndex LHS = node_manager->get_left(node);
    LLVMSymbol* assign_value = symbol_table.lookup_symbol(node_manager->get_lexeme(LHS));
    if (assign_value == nullptr) {
        // ERROR!!
        return;
    }
    
    llvm::Value* RHS = visit_expression(node);
    
    builder->CreateStore(RHS, assign_value->value);
}

llvm::Value* SageCodeGenVisitor::visit_binary_expr(NodeIndex node) {
    switch(node_manager->get_nodetype(node)) {
        case PN_BINARY:
            return visit_expression(node);
        case PN_FUNCDEF:
            return visit_function_definition(node);
        case PN_FUNCDEC:
            return visit_function_declaration(node);
        case PN_ASSIGN:
            // stack variable reassignment
            process_assignment(node);
            break;
        case PN_VAR_DEC: 
            // allocating space on stack without assigning value
            return visit_variable_decl(node);
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
        default:
            // error
            break;
    }

    return nullptr;
}

// TODO: split this into two functions trinary_decl and binary_decl
llvm::Value* SageCodeGenVisitor::visit_variable_decl(NodeIndex node) {
    auto concrete_node_type = node_manager->get_host_nodetype(node);
    if (concrete_node_type == PN_BINARY) {
        // left is variable identifier
        string variable_name = node_manager->get_lexeme(node_manager->get_left(node));
        
        // right is type identifier
        auto rightside = node_manager->get_right(node);
        if (node_manager->get_host_nodetype(rightside) != PN_UNARY) {
            // ERROR!! FOR NOW NON UNARY REPRESENTED TYPES DONT EXIST
            return nullptr;
        }
        auto type_ident = symbol_table.resolve_sage_type(node_manager, rightside);

        llvm::AllocaInst* allocation = builder->CreateAlloca(
            type_ident,            // Type to allocate
            nullptr,               // Number of elements (nullptr = 1)
            variable_name          // Name of the allocation
        );

        // update symbol table
        successful success = symbol_table.declare_symbol(variable_name, create_symbol(variable_name, allocation, type_ident), false);
        if (!success) {
            // ERROR!!
            return nullptr;
        }

        return allocation;

    }else if (concrete_node_type == PN_TRINARY) {
        // left is variable identifier
        string variable_name = node_manager->get_lexeme(node_manager->get_left(node));
        
        // middle is type identifier
        auto middle = node_manager->get_middle(node);
        if (node_manager->get_host_nodetype(middle) != PN_UNARY) {
            // ERROR!! FOR NOW NON UNARY REPRESENTED TYPES DONT EXIST
            return nullptr;
        }
        auto type_ident = symbol_table.resolve_sage_type(node_manager, middle);

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

        auto RHS = visit_expression(node_manager->get_right(node));

        // don't need to check the success of this call because we already know this symbol doesn't already exist
        symbol_table.declare_symbol(variable_name, create_symbol(variable_name, allocation, type_ident), false);

        auto final = builder->CreateStore(RHS, allocation);

        return final;
    }

    // ERROR!!
    return nullptr;
}

llvm::Value* SageCodeGenVisitor::process_expression(NodeIndex node) {
    auto LHS_node = node_manager->get_left(node);
    auto RHS_node = node_manager->get_right(node);

    llvm::Value* LHS_ir;
    llvm::Value* RHS_ir;

    if (node_manager->get_host_nodetype(LHS_node) == PN_UNARY) {
        LHS_ir = visit_unary_expr(LHS_node);
    }else {
        process_expression(LHS_node);
    }

    if (node_manager->get_host_nodetype(RHS_node) == PN_UNARY) {
        RHS_ir = visit_unary_expr(RHS_node);
    }else {
        process_expression(RHS_node);
    }
    
    // parse operator IR
    auto operator_tok = node_manager->get_token(node);
    
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
            return builder->CreateAdd(LHS_ir, RHS_ir, "addtmp");
        case TT_SUB:
            return builder->CreateSub(LHS_ir, RHS_ir, "subtmp");
        case TT_MUL:
            return builder->CreateMul(LHS_ir, RHS_ir, "multmp");
        case TT_DIV:
            return builder->CreateSDiv(LHS_ir, RHS_ir, "divtmp");
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

llvm::Value* SageCodeGenVisitor::visit_expression(NodeIndex node) {
    switch (node_manager->get_nodetype(node)) {
        case PN_BINARY:
            return process_expression(node);

        case PN_UNARY:
            return visit_unary_expr(node);

        case PN_RANGE:
            break; // TODO: later
 
        default:
            // ERROR!!
            break;
    }

    return nullptr;
}
