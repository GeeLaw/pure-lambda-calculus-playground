#include"terms.hpp"
#include"parser.hpp"
#include"reducer.hpp"
#include<cstdio>
#include"toys/toy.hpp"
#include<map>
#include<string>

using namespace DeBruijnIndex::Parser;
using namespace LambdaCalculus::Reduction;

struct SavedEntriesTag
{
    static std::map<std::string, TermPtr> entries;
    TermPtr operator () (char const *name, int length) const
    {
        return LookupEntry(std::string(name, (size_t)length));
    }
    void RemoveEntry(std::string const &name) const
    {
        auto found = entries.find(name);
        if (found != entries.end())
        {
            entries.erase(found);
        }
    }
    void AddEntry(std::string const &name, TermPtr const &ptr) const
    {
        entries[name] = ptr;
    }
    TermPtr LookupEntry(std::string const &name) const
    {
        auto found = entries.find(name);
        return found == entries.end() ? nullptr : found->second;
    }
    void ClearEntries() const
    {
        entries.clear();
    }
} const SavedEntries;
std::map<std::string, TermPtr> SavedEntriesTag::entries;

char buffer_short[1024];
char buffer[8192];
std::string const commands[] = { "set", "reduce", "print", "echo", "exit" };
#define CMD_SET 0
#define CMD_REDUCE 1
#define CMD_PRINT 2
#define CMD_ECHO 3
#define CMD_EXIT 4

int main()
{
    while (true)
    {
        if (scanf("%s", buffer_short) != 1)
        {
            break;
        }
        if (buffer_short == commands[CMD_SET])
        {
            scanf("%s%[^\n]", buffer_short, buffer);
            char const *err, *errpos;
            TermPtr result;
            if (!Parse(buffer, result, err, errpos, SavedEntries))
            {
                PutParserError(buffer, err, errpos);
                continue;
            }
            SavedEntries.AddEntry(buffer_short, result);
            continue;
        }
        if (buffer_short == commands[CMD_REDUCE])
        {
            scanf("%s", buffer_short);
            auto result = SavedEntries.LookupEntry(buffer_short);
            if (!(bool)result)
            {
                fprintf(stderr, "Error: identifier %s not found.\n", buffer_short);
                continue;
            }
            for (int i = 0;
                i != 65536
                && (EtaConversion::Perform(result)
                    || BetaReduction::Perform(result));
                ++i)
                ;
            SavedEntries.AddEntry(buffer_short, result);
            continue;
        }
        if (buffer_short == commands[CMD_PRINT])
        {
            scanf("%s", buffer_short);
            auto result = SavedEntries.LookupEntry(buffer_short);
            if (!(bool)result)
            {
                fprintf(stderr, "Error: identifier %s not found.\n", buffer_short);
                continue;
            }
            TermPrinter.Print(result);
            putchar('\n');
            continue;
        }
        if (buffer_short == commands[CMD_ECHO])
        {
            EatLine('.');
            int ch;
            while ((ch = getchar()) != -1)
            {
                putchar(ch);
                if (ch == '\n')
                {
                    break;
                }
            }
            continue;
        }
        if (buffer_short == commands[CMD_EXIT])
        {
            break;
        }
        fprintf(stderr, "Error: unrecognised command %s.\n", buffer_short);
        EatLine();
    }
    /* Release stored TermPtrs to prevent
     * too-late destruction. */
    SavedEntries.ClearEntries();
    return 0;
}
