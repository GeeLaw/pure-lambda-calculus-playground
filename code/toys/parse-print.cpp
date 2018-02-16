#include"../terms.hpp"
#include"../parser.hpp"
#include<cstdio>
#include"toy.hpp"

using namespace DeBruijnIndex::Parser;

char buffer[8192];

int main()
{
    while (scanf("%[^\n]", buffer) == 1)
    {
        EatLine();
        char const *err, *errpos;
        TermPtr result;
        if (!Parse(buffer, result, err, errpos, EmptyConstantTable))
        {
            PutParserError(buffer, err, errpos);
            continue;
        }
        HintAndPrintTerm("", result);
    }
    return 0;
}
