/***************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2008 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact:  Qt Software Information (qt-info@nokia.com)
**
**
** Non-Open Source Usage
**
** Licensees may use this file in accordance with the Qt Beta Version
** License Agreement, Agreement version 2.2 provided with the Software or,
** alternatively, in accordance with the terms contained in a written
** agreement between you and Nokia.
**
** GNU General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU General
** Public License versions 2.0 or 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the packaging
** of this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
**
** http://www.fsf.org/licensing/licenses/info/GPLv2.html and
** http://www.gnu.org/copyleft/gpl.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt GPL Exception
** version 1.3, included in the file GPL_EXCEPTION.txt in this package.
**
***************************************************************************/

#ifndef CPLUSPLUS_RESOLVEEXPRESSION_H
#define CPLUSPLUS_RESOLVEEXPRESSION_H

#include "LookupContext.h"

#include <ASTVisitor.h>
#include <Semantic.h>
#include <FullySpecifiedType.h>

namespace CPlusPlus {

class CPLUSPLUS_EXPORT ResolveExpression: protected ASTVisitor
{
public:
    typedef QPair<FullySpecifiedType, Symbol *> Result;

public:
    ResolveExpression(const LookupContext &context);
    virtual ~ResolveExpression();

    QList<Result> operator()(ExpressionAST *ast);

    QList<Result> resolveMemberExpression(const QList<Result> &baseResults,
                                          unsigned accessOp,
                                          Name *memberName) const;

    QList<Result> resolveMember(const Result &result,
                                Name *memberName,
                                NamedType *namedTy) const;

    QList<Result> resolveMember(const Result &result,
                                Name *memberName,
                                NamedType *namedTy,
                                Class *klass) const;

    QList<Result> resolveArrowOperator(const Result &result,
                                       NamedType *namedTy,
                                       Class *klass) const;

    QList<Result> resolveArrayOperator(const Result &result,
                                       NamedType *namedTy,
                                       Class *klass) const;

protected:
    QList<Result> switchResults(const QList<Result> &symbols);

    void addResult(const FullySpecifiedType &ty, Symbol *symbol = 0);
    void addResult(const Result &result);
    void addResults(const QList<Result> &results);

    using ASTVisitor::visit;

    virtual bool visit(ExpressionListAST *ast);
    virtual bool visit(BinaryExpressionAST *ast);
    virtual bool visit(CastExpressionAST *ast);
    virtual bool visit(ConditionAST *ast);
    virtual bool visit(ConditionalExpressionAST *ast);
    virtual bool visit(CppCastExpressionAST *ast);
    virtual bool visit(DeleteExpressionAST *ast);
    virtual bool visit(ArrayInitializerAST *ast);
    virtual bool visit(NewExpressionAST *ast);
    virtual bool visit(TypeidExpressionAST *ast);
    virtual bool visit(TypenameCallExpressionAST *ast);
    virtual bool visit(TypeConstructorCallAST *ast);
    virtual bool visit(PostfixExpressionAST *ast);
    virtual bool visit(SizeofExpressionAST *ast);
    virtual bool visit(NumericLiteralAST *ast);
    virtual bool visit(BoolLiteralAST *ast);
    virtual bool visit(ThisExpressionAST *ast);
    virtual bool visit(NestedExpressionAST *ast);
    virtual bool visit(StringLiteralAST *ast);
    virtual bool visit(ThrowExpressionAST *ast);
    virtual bool visit(TypeIdAST *ast);
    virtual bool visit(UnaryExpressionAST *ast);

    //names
    virtual bool visit(QualifiedNameAST *ast);
    virtual bool visit(OperatorFunctionIdAST *ast);
    virtual bool visit(ConversionFunctionIdAST *ast);
    virtual bool visit(SimpleNameAST *ast);
    virtual bool visit(DestructorNameAST *ast);
    virtual bool visit(TemplateIdAST *ast);

    // postfix expressions
    virtual bool visit(CallAST *ast);
    virtual bool visit(ArrayAccessAST *ast);
    virtual bool visit(PostIncrDecrAST *ast);
    virtual bool visit(MemberAccessAST *ast);

    QList<Scope *> visibleScopes(const Result &result) const;

private:
    LookupContext _context;
    Semantic sem;
    QList<Result> _results;
};

class CPLUSPLUS_EXPORT ResolveClass
{
public:
    ResolveClass();

    QList<Symbol *> operator()(NamedType *namedTy,
                               ResolveExpression::Result p,
                               const LookupContext &context);

    QList<Symbol *> operator()(ResolveExpression::Result p,
                               const LookupContext &context);

private:
    QList<Symbol *> resolveClass(NamedType *namedTy,
                                        ResolveExpression::Result p,
                                        const LookupContext &context);

    QList<Symbol *> resolveClass(ResolveExpression::Result p,
                                        const LookupContext &context);

private:
    QList<ResolveExpression::Result> _blackList;
};


} // end of namespace CPlusPlus

#endif // CPLUSPLUS_RESOLVEEXPRESSION_H
