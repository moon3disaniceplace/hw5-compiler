#include "generate.hpp"


CodeGenVisitor::CodeGenVisitor() {
    emitPrintFunctions();
}


void CodeGenVisitor::emitPrintFunctions() {
    buffer.emit(R"(declare i32 @printf(i8*, ...)
declare void @exit(i32)
@.int_specifier = constant [4 x i8] c"%d\0A\00"
@.str_specifier = constant [4 x i8] c"%s\0A\00"
@.div_zero_msg = constant [23 x i8] c"Error division by zero\00"
define void @printi(i32) {
    %spec_ptr = getelementptr [4 x i8], [4 x i8]* @.int_specifier, i32 0, i32 0
    call i32 (i8*, ...) @printf(i8* %spec_ptr, i32 %0)
    ret void
}

define void @print(i8*) {
    %spec_ptr = getelementptr [4 x i8], [4 x i8]* @.str_specifier, i32 0, i32 0
    call i32 (i8*, ...) @printf(i8* %spec_ptr, i8* %0)
    ret void
})");
}



    std::string CodeGenVisitor::freshVar() { return buffer.freshVar(); }
    std::string CodeGenVisitor::freshLabel() { return buffer.freshLabel(); }


void CodeGenVisitor::visit(ast::String &node) {
    // Emit the string as a global constant in LLVM
    std::string stringVar = buffer.emitString(node.value);
    // Get a pointer to the string (GEP - GetElementPtr)
    std::string stringPtr = buffer.freshVar();
    buffer.emit(stringPtr + " = getelementptr [" + 
                std::to_string(node.value.size() + 1) + " x i8], [" + 
                std::to_string(node.value.size() + 1) + " x i8]* " + 
                stringVar + ", i32 0, i32 0");

    // Store the pointer for future use
    node.llvm_register = stringPtr;
}

void CodeGenVisitor::visit(ast::Bool &node) {
    std::string boolReg = buffer.freshVar();
    buffer.emit(boolReg + " = zext i1 " + (node.value ? "1" : "0") + " to i32");
    node.llvm_register = boolReg;
}

void CodeGenVisitor::visit(ast::Type &node) {
    // No code generation needed for type declarations
}

void CodeGenVisitor::visit(ast::Cast &node) {
    node.exp->accept(*this);  // Generate code for the expression to be casted
    std::string inputReg = node.exp->llvm_register;
    std::string castedReg = buffer.freshVar();

    // Check the type of conversion
    if (node.target_type->type == ast::BuiltInType::BYTE && node.exp->type == ast::BuiltInType::INT) {
        // INT to BYTE: Truncate the higher bits to fit into an 8-bit register
        std::string truncated = buffer.freshVar();
        buffer.emit(truncated + " = trunc i32 " + inputReg + " to i8");

        // Extend back to 32-bit (for consistent representation)
        buffer.emit(castedReg + " = zext i8 " + truncated + " to i32");
        node.llvm_register = castedReg;
    } 
    else{
        node.llvm_register = inputReg;
    }
}


void CodeGenVisitor::visit(ast::ExpList &node) {
    for (auto &exp : node.exps) {
        exp->accept(*this);
    }
}

void CodeGenVisitor::visit(ast::Formal &node) {
    // No code generation needed for function parameters
}

void CodeGenVisitor::visit(ast::Formals &node) {
    for (auto &formal : node.formals) {
        formal->accept(*this);
    }
}
    
    void CodeGenVisitor::visit(ast::NumB &node) {
        std::string byteReg = buffer.freshVar();
        buffer.emit(byteReg + " = zext i8 " + std::to_string(node.value) + " to i32");
        node.llvm_register = byteReg;
    }
    //not right
    void CodeGenVisitor::visit(ast::Num &node) {
        std::string intReg = buffer.freshVar();
        buffer.emit(intReg + " = add i32 0, " + std::to_string(node.value));
        node.llvm_register = intReg;
    }

    void CodeGenVisitor::visit(ast::ID &node) {
        if(node.offset < 0){
            std::string temp = freshVar();
            int nodeoffsetnow = -node.offset-1;
            buffer.emit(temp + " = add i32 0, %" + std::to_string(nodeoffsetnow));
            node.llvm_register = temp;
        }
        else{
            std::string temp = freshVar();
            buffer.emit(temp + " = load i32, i32* " + varMap[node.offset]);
            node.llvm_register = temp;
        }
    }

void CodeGenVisitor::visit(ast::BinOp &node) {
    node.left->accept(*this);
    node.right->accept(*this);

    std::string lhs = node.left->llvm_register;
    std::string rhs = node.right->llvm_register;
    std::string wideResult = freshVar();

    if (node.op == ast::BinOpType::ADD) {
        buffer.emit(wideResult + " = add i32 " + lhs + ", " + rhs);
    } else if (node.op == ast::BinOpType::SUB) {
        buffer.emit(wideResult + " = sub i32 " + lhs + ", " + rhs);
    } else if (node.op == ast::BinOpType::MUL) {
        buffer.emit(wideResult + " = mul i32 " + lhs + ", " + rhs);
    } else if (node.op == ast::BinOpType::DIV) {
        std::string zeroCheck = freshVar();
        std::string errorLabel = freshLabel();
        std::string continueLabel = freshLabel();

        buffer.emit(zeroCheck + " = icmp eq i32 " + rhs + ", 0");
        buffer.emit("br i1 " + zeroCheck + ", label " + errorLabel + ", label " + continueLabel);

        buffer.emitLabel(errorLabel);
        buffer.emit("call void @print(i8* @.div_zero_msg)");
        buffer.emit("call void @exit(i32 0)");
        buffer.emit("unreachable");

        buffer.emitLabel(continueLabel);
        buffer.emit(wideResult + " = sdiv i32 " + lhs + ", " + rhs);
    }
    node.llvm_register = wideResult;
    if (node.type == ast::BuiltInType::BYTE) {    
        // **Force wraparound with `& 255` (0xFF)**
        std::string wrapped = freshVar();
        buffer.emit(wrapped + " = and i32 " + wideResult + ", 255");

        // **Convert to byte properly**
        std::string truncated = freshVar();
        buffer.emit(truncated + " = trunc i32 " + wrapped + " to i8");

        // **Extend back to `i32`**
        std::string extended = freshVar();
        buffer.emit(extended + " = zext i8 " + truncated + " to i32");

        node.llvm_register = extended;
    }
}

    void CodeGenVisitor::visit(ast::RelOp &node) {
        node.left->accept(*this);
        node.right->accept(*this);
        std::string lhs = node.left->llvm_register;
        std::string rhs = node.right->llvm_register;
        std::string temp = freshVar();
        std::string cast = freshVar();
        if (node.op == ast::RelOpType::EQ) {
            buffer.emit(temp + " = icmp eq i32 " + lhs + ", " + rhs);
            buffer.emit(cast + " = zext i1 " + temp + " to i32");
        } else if (node.op == ast::RelOpType::NE) {
            buffer.emit(temp + " = icmp ne i32 " + lhs + ", " + rhs);
            buffer.emit(cast + " = zext i1 " + temp + " to i32");
        } else if (node.op == ast::RelOpType::LT) {
            buffer.emit(temp + " = icmp slt i32 " + lhs + ", " + rhs);
            buffer.emit(cast + " = zext i1 " + temp + " to i32");
        } else if (node.op == ast::RelOpType::GT) {
            buffer.emit(temp + " = icmp sgt i32 " + lhs + ", " + rhs);
            buffer.emit(cast + " = zext i1 " + temp + " to i32");
        } else if (node.op == ast::RelOpType::LE) {
            buffer.emit(temp + " = icmp sle i32 " + lhs + ", " + rhs);
            buffer.emit(cast + " = zext i1 " + temp + " to i32");
        } else if (node.op == ast::RelOpType::GE) {
            buffer.emit(temp + " = icmp sge i32 " + lhs + ", " + rhs);
            buffer.emit(cast + " = zext i1 " + temp + " to i32");
        }
        node.llvm_register = cast;
    }

    void CodeGenVisitor::visit(ast::Not &node) {
        node.exp->accept(*this);
        std::string temp = freshVar();
        buffer.emit(temp + " = xor i32 1, " + node.exp->llvm_register);
        node.llvm_register = temp;
    }
void CodeGenVisitor::visit(ast::And &node) {
    std::string evalRightLabel = buffer.freshLabel();
    std::string trueLabel = buffer.freshLabel();
    std::string falseLabel = buffer.freshLabel();
    std::string endLabel = buffer.freshLabel();

    node.left->accept(*this);

    std::string leftBool = buffer.freshVar();
    buffer.emit(leftBool + " = icmp ne i32 " + node.left->llvm_register + ", 0");
    buffer.emit("br i1 " + leftBool + ", label " + evalRightLabel + ", label " + falseLabel);

    buffer.emitLabel(evalRightLabel);
    node.right->accept(*this);

    std::string rightBool = buffer.freshVar();
    buffer.emit(rightBool + " = icmp ne i32 " + node.right->llvm_register + ", 0");
    buffer.emit("br i1 " + rightBool + ", label " + trueLabel + ", label " + falseLabel);

    buffer.emitLabel(trueLabel);
    buffer.emit("br label " + endLabel);

    buffer.emitLabel(falseLabel);
    buffer.emit("br label " + endLabel);

    buffer.emitLabel(endLabel);
    std::string resultReg = buffer.freshVar();
    buffer.emit(resultReg + " = phi i1 [0, " + falseLabel + "], [1, " + trueLabel + "]");

    std::string out = buffer.freshVar();
    buffer.emit(out + " = zext i1 " + resultReg + " to i32");
    node.llvm_register = out;
}


void CodeGenVisitor::visit(ast::Or &node) {
    std::string evalRightLabel = buffer.freshLabel();
    std::string trueLabel = buffer.freshLabel();
    std::string falseLabel = buffer.freshLabel();
    std::string endLabel = buffer.freshLabel();

    node.left->accept(*this);

    std::string leftBool = buffer.freshVar();
    buffer.emit(leftBool + " = icmp ne i32 " + node.left->llvm_register + ", 0");
    buffer.emit("br i1 " + leftBool + ", label " + trueLabel + ", label " + evalRightLabel);

    buffer.emitLabel(evalRightLabel);
    node.right->accept(*this);

    std::string rightBool = buffer.freshVar();
    buffer.emit(rightBool + " = icmp ne i32 " + node.right->llvm_register + ", 0");
    buffer.emit("br i1 " + rightBool + ", label " + trueLabel + ", label " + falseLabel);

    buffer.emitLabel(trueLabel);
    buffer.emit("br label " + endLabel);

    buffer.emitLabel(falseLabel);
    buffer.emit("br label " + endLabel);

    buffer.emitLabel(endLabel);
    std::string resultReg = buffer.freshVar();
    buffer.emit(resultReg + " = phi i1 [1, " + trueLabel + "], [0, " + falseLabel + "]");

    std::string out = buffer.freshVar();
    buffer.emit(out + " = zext i1 " + resultReg + " to i32");
    node.llvm_register = out;
}






    void CodeGenVisitor::visit(ast::VarDecl &node) {
        std::string varIndex = std::to_string(node.offset); // Get offset in the local variable array
        std::string ptr = buffer.freshVar();
        // Get pointer to the allocated slot inside the `[50 x i32]` array
        buffer.emit(ptr + " = getelementptr [50 x i32], [50 x i32]*" + functionvartable +", i32 0, i32 " + varIndex);
        // Store the pointer in the variable map for future access
        varMap[node.offset] = ptr;
        // If the variable has an initial value, store it in memory
        if (node.init_exp) {
            node.init_exp->accept(*this); // Generate code for the initial expression
            buffer.emit("store i32 " + node.init_exp->llvm_register + ", i32* " + ptr);
        }
        else{
            buffer.emit("store i32 0, i32* " + ptr);
        }
    }

    void CodeGenVisitor::visit(ast::Assign &node) {
        node.exp->accept(*this);
        buffer.emit("store i32 " + node.exp->llvm_register + ", i32* " + varMap[node.id->offset]);
    }

void CodeGenVisitor::visit(ast::If &node) {
    // Evaluate condition
    node.condition->accept(*this);

    // Generate unique labels
    std::string trueLabel = buffer.freshLabel();
    std::string falseLabel = node.otherwise ? buffer.freshLabel() : "";
    std::string endLabel = buffer.freshLabel();
    
    // Conditional branch based on condition
    if (node.otherwise) {
        std::string answer = buffer.freshVar();
        buffer.emit(answer + " = icmp ne i32 " + node.condition->llvm_register + ", 0");
        buffer.emit("br i1 " + answer + ", label " + trueLabel + ", label " + falseLabel);
    } else {
        std::string answer = buffer.freshVar();
        buffer.emit(answer + " = icmp ne i32 " + node.condition->llvm_register + ", 0");
        buffer.emit("br i1 " + answer + ", label " + trueLabel + ", label " + endLabel);
    }

    // If-true block
    buffer.emitLabel(trueLabel);
    node.then->accept(*this);
    buffer.emit("br label " + endLabel); // Jump to end after execution

    // If-false block (only if `else` exists)
    if (node.otherwise) {
        buffer.emitLabel(falseLabel);
        node.otherwise->accept(*this);
        buffer.emit("br label " + endLabel);
    }

    // End block
    buffer.emitLabel(endLabel);
}

void CodeGenVisitor::visit(ast::While &node) {
    std::string condLabel = buffer.freshLabel();
    std::string bodyLabel = buffer.freshLabel();
    std::string endLabel = buffer.freshLabel();
    loopCondLabels.push(condLabel);
    loopEndLabels.push(endLabel);
    // Jump to condition check first
    buffer.emit("br label " + condLabel);
    
    // Condition check
    buffer.emitLabel(condLabel);
    node.condition->accept(*this);
    std::string condReg = buffer.freshVar();
    buffer.emit(condReg + " = icmp ne i32 " + node.condition->llvm_register + ", 0");
    buffer.emit("br i1 " + condReg + ", label " + bodyLabel + ", label " + endLabel);

    // Loop body
    buffer.emitLabel(bodyLabel);
    node.body->accept(*this);
    buffer.emit("br label " + condLabel); // Jump back to check condition again

    // End of loop
    buffer.emitLabel(endLabel);
    loopCondLabels.pop();
    loopEndLabels.pop();
}
    void CodeGenVisitor::visit(ast::Break &node) {
        buffer.emit("br label " + loopEndLabels.top());
    }
    void CodeGenVisitor::visit(ast::Continue &node) {
        buffer.emit("br label " + loopCondLabels.top());
    }

    void CodeGenVisitor::visit(ast::FuncDecl &node){
       
    std::string funcName = node.id->value;
    std::string returnType = (node.return_type->type == ast::BuiltInType::VOID) ? "void" : "i32";

    // Generate parameter list
    std::string paramList;
    for (int i = 0; i < node.formals->formals.size(); ++i) {
        if (i > 0) paramList += ", ";
        paramList += "i32";
    }

    // Emit function definition
    buffer.emit("define " + returnType + " @" + funcName + "(" + paramList + ") {");

    // Create a new table variables for the function
    functionvartable = buffer.freshVar();
    buffer.emit(functionvartable + " = alloca [50 x i32]");
    //buffer.emit("store [50 x i32] zeroinitialiazer, [50 x i32]* " + functionvartable + ", align 4");
    // Emit function body
    node.body->accept(*this);

    // If the function is void, emit return void
    if (node.return_type->type == ast::BuiltInType::VOID) {
        buffer.emit("ret void");
    }
    else{
        buffer.emit("ret i32 0");
    }

    buffer.emit("}");
}

    void CodeGenVisitor::visit(ast::Funcs &node) {
        for (auto &func : node.funcs) {
            func->accept(*this);
        }
    }

    void CodeGenVisitor::visit(ast::Return &node) {
        if(node.exp){
        node.exp->accept(*this);
        buffer.emit("ret i32 " + node.exp->llvm_register);
        }
        else{
            buffer.emit("ret void");
        }
    }

void CodeGenVisitor::visit(ast::Call &node) {
    std::string resultReg = buffer.freshVar();


    std::vector<std::string> argList;

    for (const auto &arg : node.args->exps) {
        arg->accept(*this); // Generate LLVM IR for the argument
        std::string argType = (arg->type == ast::BuiltInType::STRING) ? "i8*" : "i32";
        argList.push_back(argType + " " + arg->llvm_register);
    }
    // Join arguments into a single string
    std::string argsStr = argList.empty() ? "" : argList[0];
    for (size_t i = 1; i < argList.size(); ++i) {
        argsStr += ", " + argList[i];
    }

    if(node.type == ast::BuiltInType::VOID){
        buffer.emit("call void @" + node.func_id->value + "(" + argsStr + ")");
    }
    else{
        buffer.emit(resultReg + " = call i32 @" + node.func_id->value + "(" + argsStr + ")");
        node.llvm_register = resultReg;
    }

}
    void CodeGenVisitor::visit(ast::Statements &node) {
        for (auto &stmt : node.statements) {
            stmt->accept(*this);
        }
    }



