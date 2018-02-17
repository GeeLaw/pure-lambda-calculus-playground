#pragma once

#ifndef TOY_HPP_
#define TOY_HPP_ 1

#include"../terms.hpp"
#include"../parser.hpp"
#include<cstdio>

typedef LambdaCalculus::Term Term;
typedef Term::Pointer TermPtr;

struct
{
    template <typename T1, typename T2>
    TermPtr operator () (T1 &&, T2 &&) const
    {
        return nullptr;
    }
} EmptyConstantTable;

struct TermPrinterTag : Term::Visitor<TermPrinterTag, void (TermPtr const &, FILE *, size_t, bool)>
{
    friend struct Term::Visitor<TermPrinterTag, void (TermPtr const &, FILE *, size_t, bool)>;
    TermPrinterTag() { }
    void Print(TermPtr const &term, FILE *fp = stdout)
    {
        VisitTerm(term, fp, 0, true);
    }
private:
    typedef Utilities::PodSurrogate<size_t> VariableNameTag;
    void VisitInvalidTerm(TermPtr const &, FILE *fp, size_t, bool)
    {
        fputs("[invalid]", fp);
    }
    void VisitInternalErrorTerm(TermPtr const &, FILE *fp, size_t, bool)
    {
        fputs("[internal error]", fp);
    }
    void VisitBoundVariableTerm(TermPtr const &target, FILE *fp, size_t level, bool)
    {
        fprintf(fp, "%zu", level - target->AsBoundVariable.BoundBy->Tag.RawPtrUnsafe<VariableNameTag>()->Value);
    }
    void VisitAbstractionTerm(TermPtr const &target, FILE *fp, size_t level, bool lastAbs)
    {
        target->Tag.NewInstance<VariableNameTag>()->Value = level++;
        if (!lastAbs)
        {
            fputc('(', fp);
        }
        fputs("lambda ", fp);
        VisitTerm(target->AsAbstraction.Result, fp, level, true);
        if (!lastAbs)
        {
            fputc(')', fp);
        }
        target->Tag = nullptr;
        --level;
    }
    void VisitApplicationTerm(TermPtr const &target, FILE *fp, size_t level, bool lastAbs)
    {
        auto const &func = target->AsApplication.Function;
        auto const &rplc = target->AsApplication.Replaced;
        bool const paren = (rplc->Kind == Term::ApplicationTerm);
        VisitTerm(func, fp, level, false);
        putchar(' ');
        if (paren)
        {
            putchar('(');
        }
        VisitTerm(rplc, fp, level, paren || lastAbs);
        if (paren)
        {
            putchar(')');
        }
    }
} TermPrinter;

void PutParserError(char const *input,
    char const *err, char const *errpos)
{
    fprintf(stderr, "Error: %s\n", err);
    if (errpos)
    {
        fputs(input, stderr);
        fputc('\n', stderr);
        for (int i = 0; i != errpos - input; ++i)
        {
            fputc(' ', stderr);
        }
        fputc('^', stderr);
        fputc('\n', stderr);
    }
}

void HintAndPrintTerm(char const *hint, TermPtr const &term)
{
    fputs(hint, stdout);
    TermPrinter.Print(term);
    fputc('\n', stdout);
}

void EatLine(int separator = '\n')
{
    int ch;
    while ((ch = getchar()) != -1 && ch != separator)
        ;
}

#endif // TOY_HPP_
