/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the V4VM module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QV4CODEGEN_P_H
#define QV4CODEGEN_P_H

#include "qv4global.h"
#include "qv4jsir_p.h"
#include <private/qqmljsastvisitor_p.h>
#include <QtCore/QStringList>
#include <assert.h>

namespace QQmlJS {

namespace AST {
class UiParameterList;
}

namespace VM {
struct DiagnosticMessage;
struct ExecutionContext;
}

namespace Debugging {
class Debugger;
} // namespace Debugging

class ErrorHandler
{
public:
    virtual void syntaxError(VM::DiagnosticMessage *message) = 0;
};

class Q_V4_EXPORT Codegen: protected AST::Visitor
{
public:
    Codegen(VM::ExecutionContext *ctx, bool strict);
    Codegen(ErrorHandler *errorHandler, bool strictMode);

    enum Mode {
        GlobalCode,
        EvalCode,
        FunctionCode
    };

    IR::Function *operator()(const QString &fileName, AST::Program *ast, IR::Module *module, Mode mode = GlobalCode, const QStringList &inheritedLocals = QStringList());
    IR::Function *operator()(const QString &fileName, AST::FunctionExpression *ast, IR::Module *module);

protected:
    enum Format { ex, cx, nx };
    struct Result {
        IR::Expr *code;
        IR::BasicBlock *iftrue;
        IR::BasicBlock *iffalse;
        Format format;
        Format requested;

        explicit Result(Format requested = ex)
            : code(0)
            , iftrue(0)
            , iffalse(0)
            , format(ex)
            , requested(requested) {}

        explicit Result(IR::BasicBlock *iftrue, IR::BasicBlock *iffalse)
            : code(0)
            , iftrue(iftrue)
            , iffalse(iffalse)
            , format(ex)
            , requested(cx) {}

        inline IR::Expr *operator*() const { Q_ASSERT(format == ex); return code; }
        inline IR::Expr *operator->() const { Q_ASSERT(format == ex); return code; }

        bool accept(Format f)
        {
            if (requested == f) {
                format = f;
                return true;
            }
            return false;
        }
    };

    struct Environment {
        Environment *parent;

        enum MemberType {
            UndefinedMember,
            VariableDefinition,
            VariableDeclaration,
            FunctionDefinition
        };
        struct Member {
            MemberType type;
            int index;
            AST::FunctionExpression *function;
        };
        typedef QMap<QString, Member> MemberMap;

        MemberMap members;
        int maxNumberOfArguments;
        bool hasDirectEval;
        bool hasNestedFunctions;
        bool isStrict;
        enum UsesArgumentsObject {
            ArgumentsObjectUnknown,
            ArgumentsObjectNotUsed,
            ArgumentsObjectUsed
        };

        UsesArgumentsObject usesArgumentsObject;

        Environment(Environment *parent)
            : parent(parent)
            , maxNumberOfArguments(0)
            , hasDirectEval(false)
            , hasNestedFunctions(false)
            , isStrict(false)
            , usesArgumentsObject(ArgumentsObjectUnknown)
        {
            if (parent && parent->isStrict)
                isStrict = true;
        }

        int findMember(const QString &name) const
        {
            MemberMap::const_iterator it = members.find(name);
            if (it == members.end())
                return -1;
            assert((*it).index != -1 || !parent);
            return (*it).index;
        }

        bool lookupMember(const QString &name, Environment **scope, int *index, int *distance)
        {
            Environment *it = this;
            *distance = 0;
            for (; it; it = it->parent, ++(*distance)) {
                int idx = it->findMember(name);
                if (idx != -1) {
                    *scope = it;
                    *index = idx;
                    return true;
                }
            }
            return false;
        }

        void enter(const QString &name, MemberType type, AST::FunctionExpression *function = 0)
        {
            if (! name.isEmpty()) {
                MemberMap::iterator it = members.find(name);
                if (it == members.end()) {
                    Member m;
                    m.index = -1;
                    m.type = type;
                    m.function = function;
                    members.insert(name, m);
                } else {
                    if ((*it).type <= type) {
                        (*it).type = type;
                        (*it).function = function;
                    }
                }
            }
        }
    };

    Environment *newEnvironment(AST::Node *node, Environment *parent)
    {
        Environment *env = new Environment(parent);
        _envMap.insert(node, env);
        return env;
    }

    struct UiMember {
    };

    struct ScopeAndFinally {
        ScopeAndFinally *parent;
        AST::Finally *finally;
        IR::ExprList *finishTryArgs;
        bool popScope;

        ScopeAndFinally(ScopeAndFinally *parent) : parent(parent), finally(0), finishTryArgs(0), popScope(true) {}
        ScopeAndFinally(ScopeAndFinally *parent, AST::Finally *finally, IR::ExprList *finishTryArgs)
        : parent(parent), finally(finally), finishTryArgs(finishTryArgs), popScope(false)
        {}
    };

    struct Loop {
        AST::LabelledStatement *labelledStatement;
        AST::Statement *node;
        IR::BasicBlock *breakBlock;
        IR::BasicBlock *continueBlock;
        Loop *parent;
        ScopeAndFinally *scopeAndFinally;

        Loop(AST::Statement *node, IR::BasicBlock *breakBlock, IR::BasicBlock *continueBlock, Loop *parent)
            : labelledStatement(0), node(node), breakBlock(breakBlock), continueBlock(continueBlock), parent(parent) {}
    };

    void enterEnvironment(AST::Node *node);
    void leaveEnvironment();

    void enterLoop(AST::Statement *node, IR::BasicBlock *breakBlock, IR::BasicBlock *continueBlock);
    void leaveLoop();


    IR::Expr *member(IR::Expr *base, const QString *name);
    IR::Expr *subscript(IR::Expr *base, IR::Expr *index);
    IR::Expr *argument(IR::Expr *expr);
    IR::Expr *reference(IR::Expr *expr);
    IR::Expr *unop(IR::AluOp op, IR::Expr *expr);
    IR::Expr *binop(IR::AluOp op, IR::Expr *left, IR::Expr *right);
    IR::Expr *call(IR::Expr *base, IR::ExprList *args);
    void move(IR::Expr *target, IR::Expr *source, IR::AluOp op = IR::OpInvalid);
    void cjump(IR::Expr *cond, IR::BasicBlock *iftrue, IR::BasicBlock *iffalse);

    void linearize(IR::Function *function);
    IR::Function *defineFunction(const QString &name, AST::Node *ast,
                                 AST::FormalParameterList *formals,
                                 AST::SourceElements *body,
                                 Mode mode = FunctionCode,
                                 const QStringList &inheritedLocals = QStringList());
    int indexOfArgument(const QStringRef &string) const;

    void unwindException(ScopeAndFinally *outest);

    void statement(AST::Statement *ast);
    void statement(AST::ExpressionNode *ast);
    void condition(AST::ExpressionNode *ast, IR::BasicBlock *iftrue, IR::BasicBlock *iffalse);
    Result expression(AST::ExpressionNode *ast);
    QString propertyName(AST::PropertyName *ast);
    Result sourceElement(AST::SourceElement *ast);
    UiMember uiObjectMember(AST::UiObjectMember *ast);

    void accept(AST::Node *node);

    void functionBody(AST::FunctionBody *ast);
    void program(AST::Program *ast);
    void sourceElements(AST::SourceElements *ast);
    void variableDeclaration(AST::VariableDeclaration *ast);
    void variableDeclarationList(AST::VariableDeclarationList *ast);

    IR::Expr *identifier(const QString &name, int line = 0, int col = 0);

    // nodes
    virtual bool visit(AST::ArgumentList *ast);
    virtual bool visit(AST::CaseBlock *ast);
    virtual bool visit(AST::CaseClause *ast);
    virtual bool visit(AST::CaseClauses *ast);
    virtual bool visit(AST::Catch *ast);
    virtual bool visit(AST::DefaultClause *ast);
    virtual bool visit(AST::ElementList *ast);
    virtual bool visit(AST::Elision *ast);
    virtual bool visit(AST::Finally *ast);
    virtual bool visit(AST::FormalParameterList *ast);
    virtual bool visit(AST::FunctionBody *ast);
    virtual bool visit(AST::Program *ast);
    virtual bool visit(AST::PropertyNameAndValue *ast);
    virtual bool visit(AST::PropertyAssignmentList *ast);
    virtual bool visit(AST::PropertyGetterSetter *ast);
    virtual bool visit(AST::SourceElements *ast);
    virtual bool visit(AST::StatementList *ast);
    virtual bool visit(AST::UiArrayMemberList *ast);
    virtual bool visit(AST::UiImport *ast);
    virtual bool visit(AST::UiImportList *ast);
    virtual bool visit(AST::UiObjectInitializer *ast);
    virtual bool visit(AST::UiObjectMemberList *ast);
    virtual bool visit(AST::UiParameterList *ast);
    virtual bool visit(AST::UiProgram *ast);
    virtual bool visit(AST::UiQualifiedId *ast);
    virtual bool visit(AST::VariableDeclaration *ast);
    virtual bool visit(AST::VariableDeclarationList *ast);

    // expressions
    virtual bool visit(AST::Expression *ast);
    virtual bool visit(AST::ArrayLiteral *ast);
    virtual bool visit(AST::ArrayMemberExpression *ast);
    virtual bool visit(AST::BinaryExpression *ast);
    virtual bool visit(AST::CallExpression *ast);
    virtual bool visit(AST::ConditionalExpression *ast);
    virtual bool visit(AST::DeleteExpression *ast);
    virtual bool visit(AST::FalseLiteral *ast);
    virtual bool visit(AST::FieldMemberExpression *ast);
    virtual bool visit(AST::FunctionExpression *ast);
    virtual bool visit(AST::IdentifierExpression *ast);
    virtual bool visit(AST::NestedExpression *ast);
    virtual bool visit(AST::NewExpression *ast);
    virtual bool visit(AST::NewMemberExpression *ast);
    virtual bool visit(AST::NotExpression *ast);
    virtual bool visit(AST::NullExpression *ast);
    virtual bool visit(AST::NumericLiteral *ast);
    virtual bool visit(AST::ObjectLiteral *ast);
    virtual bool visit(AST::PostDecrementExpression *ast);
    virtual bool visit(AST::PostIncrementExpression *ast);
    virtual bool visit(AST::PreDecrementExpression *ast);
    virtual bool visit(AST::PreIncrementExpression *ast);
    virtual bool visit(AST::RegExpLiteral *ast);
    virtual bool visit(AST::StringLiteral *ast);
    virtual bool visit(AST::ThisExpression *ast);
    virtual bool visit(AST::TildeExpression *ast);
    virtual bool visit(AST::TrueLiteral *ast);
    virtual bool visit(AST::TypeOfExpression *ast);
    virtual bool visit(AST::UnaryMinusExpression *ast);
    virtual bool visit(AST::UnaryPlusExpression *ast);
    virtual bool visit(AST::VoidExpression *ast);
    virtual bool visit(AST::FunctionDeclaration *ast);

    // property names
    virtual bool visit(AST::IdentifierPropertyName *ast);
    virtual bool visit(AST::NumericLiteralPropertyName *ast);
    virtual bool visit(AST::StringLiteralPropertyName *ast);

    // source elements
    virtual bool visit(AST::FunctionSourceElement *ast);
    virtual bool visit(AST::StatementSourceElement *ast);

    // statements
    virtual bool visit(AST::Block *ast);
    virtual bool visit(AST::BreakStatement *ast);
    virtual bool visit(AST::ContinueStatement *ast);
    virtual bool visit(AST::DebuggerStatement *ast);
    virtual bool visit(AST::DoWhileStatement *ast);
    virtual bool visit(AST::EmptyStatement *ast);
    virtual bool visit(AST::ExpressionStatement *ast);
    virtual bool visit(AST::ForEachStatement *ast);
    virtual bool visit(AST::ForStatement *ast);
    virtual bool visit(AST::IfStatement *ast);
    virtual bool visit(AST::LabelledStatement *ast);
    virtual bool visit(AST::LocalForEachStatement *ast);
    virtual bool visit(AST::LocalForStatement *ast);
    virtual bool visit(AST::ReturnStatement *ast);
    virtual bool visit(AST::SwitchStatement *ast);
    virtual bool visit(AST::ThrowStatement *ast);
    virtual bool visit(AST::TryStatement *ast);
    virtual bool visit(AST::VariableStatement *ast);
    virtual bool visit(AST::WhileStatement *ast);
    virtual bool visit(AST::WithStatement *ast);

    // ui object members
    virtual bool visit(AST::UiArrayBinding *ast);
    virtual bool visit(AST::UiObjectBinding *ast);
    virtual bool visit(AST::UiObjectDefinition *ast);
    virtual bool visit(AST::UiPublicMember *ast);
    virtual bool visit(AST::UiScriptBinding *ast);
    virtual bool visit(AST::UiSourceElement *ast);

    void throwSyntaxErrorOnEvalOrArgumentsInStrictMode(IR::Expr* expr, const AST::SourceLocation &loc);

    void throwSyntaxError(const AST::SourceLocation &loc, const QString &detail);
    void throwReferenceError(const AST::SourceLocation &loc, const QString &detail);

private:
    QString _fileName;
    Result _expr;
    QString _property;
    UiMember _uiMember;
    IR::Module *_module;
    IR::Function *_function;
    IR::BasicBlock *_block;
    IR::BasicBlock *_exitBlock;
    IR::BasicBlock *_throwBlock;
    unsigned _returnAddress;
    Mode _mode;
    Environment *_env;
    Loop *_loop;
    AST::LabelledStatement *_labelledStatement;
    ScopeAndFinally *_scopeAndFinally;
    QHash<AST::Node *, Environment *> _envMap;
    QHash<AST::FunctionExpression *, int> _functionMap;
    VM::ExecutionContext *_context;
    bool _strictMode;
    Debugging::Debugger *_debugger;
    ErrorHandler *_errorHandler;

    class ScanFunctions;
};

} // end of namespace QQmlJS

#endif // QV4CODEGEN_P_H
