#include"../terms.hpp"
#include"../parser.hpp"
#include"../reducer.hpp"
#include<cstdio>
#include"toy.hpp"

using namespace DeBruijnIndex::Parser;
using namespace LambdaCalculus::Reduction;

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
        HintAndPrintTerm("     Formatted: ", result);
        bool did = true;
        while (did)
        {
            did = false;
            if (EtaConversion::Perform(result))
            {
                HintAndPrintTerm("Eta-conversion: ", result);
                did = true;
            }
            if (BetaReduction::Perform(result))
            {
                HintAndPrintTerm("Beta-reduction: ", result);
                did = true;
            }
        }
        HintAndPrintTerm("   Normal form: ", result);
    }
    return 0;
}
