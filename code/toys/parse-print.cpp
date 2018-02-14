#include"../terms.hpp"
#include"../parser.hpp"
#include<cstdio>


typedef LambdaCalculus::Term Term;
typedef Term::RefCountPtr TermPtr;

using namespace DeBruijnIndex::Parser;

struct
{
    template <typename T1, typename T2, typename T3>
    bool operator () (T1 &&, T2 &&, T3 &&) const
    {
        return false;
    }
} query;

struct TermPrinter : Term::Visitor<TermPrinter, void(Term *)>
{
    friend struct Term::Visitor<TermPrinter, void(Term *)>;
    TermPrinter() : dummy('a') { }
    void Print(TermPtr term)
    {
        VisitTerm(term.RawPtr());
    }
private:
    char dummy;
    void VisitInvalidTerm(Term *target)
    {
        printf("[invalid]");
    }
    void VisitBoundVariableTerm(Term *target)
    {
        putchar(dummy - (int)target->AsBoundVariable.Level);
    }
    void VisitAbstractionTerm(Term *target)
    {
        printf("(%c => ", dummy++);
        VisitTerm(target->AsAbstraction.Result.RawPtr());
        putchar(')');
        --dummy;
    }
    void VisitApplicationTerm(Term *target)
    {
        printf("Apply[");
        VisitTerm(target->AsApplication.Function.RawPtr());
        putchar(',');
        putchar(' ');
        VisitTerm(target->AsApplication.Replaced.RawPtr());
        putchar(']');
    }
    void VisitInternalErrorTerm(Term *target)
    {
        printf("[internal error]");
    }
};

int main()
{
    char buffer[1024];
    char const *err, *errpos;
    TermPtr result;
    while (scanf("%[^\n]", buffer) == 1)
    {
        if (!Parse(buffer, result, err, errpos, query))
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
            TermPrinter().Print(result);
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
