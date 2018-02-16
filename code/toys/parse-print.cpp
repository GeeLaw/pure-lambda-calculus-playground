#include"../terms.hpp"
#include"../parser.hpp"
#include<cstdio>


typedef LambdaCalculus::Term Term;
typedef Term::Pointer TermPtr;

using namespace DeBruijnIndex::Parser;

struct EmptyConstantTable
{
    template <typename T1, typename T2>
    TermPtr operator () (T1 &&, T2 &&) const
    {
        return nullptr;
    }
};

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

int main()
{
    char buffer[1024];
    while (scanf("%[^\n]", buffer) == 1)
    {
        char const *err, *errpos;
        TermPtr result;
        if (!Parse(buffer, result, err, errpos, EmptyConstantTable()))
        {
            printf("Error: %s\n", err);
            if (errpos)
            {
                puts(buffer);
                for (int i = 0; i != errpos - buffer; ++i)
                {
                    putchar(' ');
                }
                putchar('^');
                putchar('\n');
            }
        }
        else
        {
            TermPrinter.Print(result);
            putchar('\n');
        }
        int ch;
        while ((ch = getchar()) != -1)
        {
            if (ch == '\n')
            {
                break;
            }
        }
    }
    return 0;
}
