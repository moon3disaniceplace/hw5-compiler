#ifndef CODEGENVISITOR_HPP
#define CODEGENVISITOR_HPP
#include "output.hpp"
#include "nodes.hpp"
#include "tables.hpp"
#include <iostream>
#include <map>
#include <string>
#include <stack>
class CodeGenVisitor : public Visitor {
public:
    int maxlocal = 50;
    output::CodeBuffer buffer;
    std::string functionvartable;
    std::map<int, std::string> varMap;  // Maps variables to stack locations
    std::stack<std::string> loopCondLabels;
    std::stack<std::string> loopEndLabels;
    CodeGenVisitor();

    void emitPrintFunctions();

    std::string freshVar();
    std::string freshLabel();

    //not right
    void visit(ast::Num &node);

    void visit(ast::NumB &node);

    void visit(ast::String &node);

    void visit(ast::Bool &node); 

    void visit(ast::ID &node); //good

    void visit(ast::BinOp &node); //good
 
    void visit(ast::RelOp &node); //good

    void visit(ast::Not &node); //good

    void visit(ast::And &node); //good
 
    void visit(ast::Or &node); //good
    void visit(ast::Type &node);

    void visit(ast::Cast &node);

    void visit(ast::ExpList &node);

    void visit(ast::Call &node);

    void visit(ast::Statements &node); //good

    void visit(ast::Break &node); //good

    void visit(ast::Continue &node); //good

    void visit(ast::Return &node);

    void visit(ast::If &node); //good

    void visit(ast::While &node); //good

    void visit(ast::VarDecl &node); //good

    void visit(ast::Assign &node); //good

    void visit(ast::Formal &node);

    void visit(ast::Formals &node);

    void visit(ast::FuncDecl &node);

    void visit(ast::Funcs &node);
};

#endif