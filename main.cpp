#include "output.hpp"
#include "nodes.hpp"
#include "semantic.hpp"
#include <iostream>

#include "generate.hpp"
// Extern from the bison-generated parser
extern int yyparse();

extern std::shared_ptr<ast::Node> program;

int main() {
    yyparse();
    SemanticVisitor semanticVisitor;
    program->accept(semanticVisitor);

    CodeGenVisitor codeGenVisitor;
    program->accept(codeGenVisitor);
    std::cout << codeGenVisitor.buffer;
    return 0;
}
