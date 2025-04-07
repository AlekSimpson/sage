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


SageCodeGenVisitor::SageCodeGenVisitor() {}

SageCodeGenVisitor::SageCodeGenVisitor(std::shared_ptr<llvm::LLVMContext> context) {
    llvm_context = context;
    builder = make_unique<llvm::IRBuilder<>>(*llvm_context);
    main_module = make_unique<llvm::Module>("main_module", *llvm_context);
    symbol_table = SageSymbolTable();
    symbol_table.initialize(*llvm_context);
}

llvm::Module* SageCodeGenVisitor::get_module() {
    return main_module.get();
}

// this is the first visitor called to compile a whole code module
llvm::Value* SageCodeGenVisitor::visit_program(BlockParseNode* node) {
    BinaryParseNode* binarynode;
    for (AbstractParseNode* child : node->children) {
        switch(child->get_nodetype()){
            case PN_FUNCDEF:
                if (child->get_host_nodetype() != PN_BINARY) {
                    // ERROR!! CHILD SHOULD BE A BINARY NODE
                    return nullptr;
                }
                binarynode = dynamic_cast<BinaryParseNode*>(child);
                visit_function_definition(binarynode);
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
    // binary node
    //  - unary     <-- function name
    //  - binary
    //      - block <-- function parameters
    //      - unary <-- function return type

    if (node->right->get_host_nodetype() != PN_BINARY) {
        // ERROR!!
        return nullptr;
    }
    BinaryParseNode* rightnode = dynamic_cast<BinaryParseNode*>(node->right);

    vector<llvm::Type*> parameter_types;
    vector<AbstractParseNode*> parameters = rightnode->left->get_child_node();

    BinaryParseNode* binary_temp;
    UnaryParseNode* unarynode;
    for (auto parameter : parameters) {
        binary_temp = dynamic_cast<BinaryParseNode*>(parameter);
        unarynode = dynamic_cast<UnaryParseNode*>(binary_temp->right);
        llvm::Type* ir_type = symbol_table.resolve_sage_type(unarynode);
        parameter_types.push_back(ir_type);
    }
    unarynode = dynamic_cast<UnaryParseNode*>(rightnode->right);

    llvm::Type* return_type = symbol_table.resolve_sage_type(unarynode);
    string function_name = node->left->get_token().lexeme;
    bool is_vararg = (parameters.at(parameters.size()-1)->get_nodetype() == PN_VARARG);
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

llvm::Value* SageCodeGenVisitor::visit_function_definition(BinaryParseNode* node) {
    symbol_table.push_scope();
    symbol_table.current_function_has_returned = false;

    // TODO: add function to symbol table
    UnaryParseNode* function_name_node = dynamic_cast<UnaryParseNode*>(node->left);

    if (node->right->get_host_nodetype() != PN_TRINARY) {
        // ERROR!!
        return nullptr;
    }
    TrinaryParseNode* trinary_node = dynamic_cast<TrinaryParseNode*>(node->right);
    
    vector<llvm::Type*> parameter_types;
    vector<AbstractParseNode*> parameters = trinary_node->left->get_child_node();
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

    if (trinary_node->middle->get_host_nodetype() != PN_UNARY) {
        // ERROR!! TYPES ONLY REPRESENTED BY UNARY NODES RIGHT NOW
        return nullptr;
    }
    auto unarytemp = dynamic_cast<UnaryParseNode*>(trinary_node->middle);
    llvm::Type* return_type = symbol_table.resolve_sage_type(unarytemp);
    llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, parameter_types, false);
    llvm::Function* function_ir = llvm::Function::Create(
        function_type, 
        llvm::Function::ExternalLinkage, 
        function_name_node->get_token().lexeme,
        main_module.get()
    );
    llvm::BasicBlock* function_block = llvm::BasicBlock::Create(*llvm_context, "entry", function_ir);
    builder->SetInsertPoint(function_block);

    if (trinary_node->right->get_host_nodetype() != PN_BLOCK) {
        // ERROR!! RIGHT NODE OF FUNC DEF NODE SHOULD ONLY EVER BE A BLOCK NODE
        return nullptr;
    }
    auto function_body = dynamic_cast<BlockParseNode*>(trinary_node->right);
    visit_codeblock(function_body);

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

llvm::Value* SageCodeGenVisitor::visit_function_call(UnaryParseNode* node) {
    BlockParseNode* args_node = dynamic_cast<BlockParseNode*>(node->branch);
    string func_call_name = node->token.lexeme;
    vector<llvm::Value*> args;

    // retrieve function from symbol table
    LLVMSymbol* symbol = symbol_table.lookup_symbol(func_call_name);
    if (symbol == nullptr) {
        // ERROR!!
        return nullptr;
    }

    int index = 0;
    for (auto arg : args_node->children) {
        // TODO:varargs breaks this kinda, fix later
        //if (index >= args_node->children.size()) {
        //    // ERROR!!
        //    return nullptr;
        //}

        UnaryParseNode* unaryarg = dynamic_cast<UnaryParseNode*>(arg);

        // TODO: varargs breaks this kinda, fix later
        // auto irtype = symbol_table.derive_sage_type(unaryarg);
        //if (irtype != symbol->type->getParamType(index)) {
        //    // ERROR!!
        //    return nullptr;
        //}

        llvm::Value* arg_value = visit_unary_expr(unaryarg);
        args.push_back(arg_value);

        index++;
    }

    string result_name = func_call_name + "_result";
    auto call_ir = builder->CreateCall(llvm::cast<llvm::Function>(symbol->value), args, result_name);

    return call_ir;
}

llvm::Value* SageCodeGenVisitor::visit_codeblock(BlockParseNode* node) {
    BinaryParseNode* binaryfill;
    UnaryParseNode* unaryfill;
    TrinaryParseNode* trinaryfill;
    llvm::Value* retval;
    for (AbstractParseNode* child : node->children) {
        switch (child->get_host_nodetype()) {
            case PN_UNARY:
                unaryfill = dynamic_cast<UnaryParseNode*>(child);
                retval = visit_unary_expr(unaryfill);
                break;
            case PN_BINARY:
                binaryfill = dynamic_cast<BinaryParseNode*>(child);
                retval = visit_binary_expr(binaryfill);
                break;
            case PN_TRINARY:
                trinaryfill = dynamic_cast<TrinaryParseNode*>(child);
                retval = visit_trinary_expr(trinaryfill);
                break;
            default:
                printf("VISIT CODEBLOCK GETTING HERE\n");
                printf("%s\n", nodetype_to_string(child->get_nodetype()).c_str());
                break;
        }
    }

    return retval;
}

llvm::Value* SageCodeGenVisitor::visit_unary_expr(UnaryParseNode* node) {
    LLVMSymbol* symbol_value;
    llvm::Value* deref_stack_value;
    llvm::Value* ptrvalue;
    string temp;
    
    switch(node->rep_nodetype) {
        case PN_VAR_REF: // PN_VAR_REF should do the same thing as PN_IDENTIFIER
        case PN_IDENTIFIER:
            symbol_value = symbol_table.lookup_symbol(node->get_token().lexeme);
            if (symbol_value == nullptr) {
                // ERROR!!
                return nullptr;
            }

            deref_stack_value = builder->CreateLoad(
                symbol_value->type,  // Type being loaded
                symbol_value->value,                    // Pointer to load from
                node->get_token().lexeme   // Name for the loaded value
            );

            return deref_stack_value;
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
            return visit_function_call(node);
        case PN_NUMBER:
            // NOTE: we don't actually know if this is supposed to be an int64, it could be something, will have to see if this causes errors in testing
            return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*llvm_context), stoi(node->get_token().lexeme));
        case PN_FLOAT:
            return llvm::ConstantFP::get(llvm::Type::getFloatTy(*llvm_context), stof(node->get_token().lexeme));
        case PN_STRING:
            // TODO: probably need a function that wil process the raw string lexeme's into string format rules and strip away \"\"
            temp = node->get_token().lexeme;
            temp.erase(std::remove(temp.begin(), temp.end(), '"'), temp.end());

            ptrvalue = builder->CreateGlobalStringPtr(
                temp,
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

llvm::Value* SageCodeGenVisitor::visit_trinary_expr(TrinaryParseNode* node) {
    switch(node->rep_nodetype) {
        case PN_FOR:
            break;
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

// TODO: split this into two functions trinary_decl and binary_decl
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

    llvm::Value* LHS_ir;
    llvm::Value* RHS_ir;

    if (LHS_node->get_host_nodetype() == PN_UNARY) {
        auto fill = dynamic_cast<UnaryParseNode*>(LHS_node);
        LHS_ir = visit_unary_expr(fill);
    }else {
        auto fill = dynamic_cast<BinaryParseNode*>(LHS_node);
        process_expression(fill);
    }

    if (RHS_node->get_host_nodetype() == PN_UNARY) {
        auto fill = dynamic_cast<UnaryParseNode*>(RHS_node);
        RHS_ir = visit_unary_expr(fill);
    }else {
        auto fill = dynamic_cast<BinaryParseNode*>(RHS_node);
        process_expression(fill);
    }
    
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
            return builder->CreateAdd(LHS_ir, RHS_ir, "addtmp");
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
