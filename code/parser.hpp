#pragma once

#ifndef PARSER_HPP_
#define PARSER_HPP_ 1

#include"terms.hpp"

namespace DeBruijnIndex
{
    typedef LambdaCalculus::Term::RefCountPtr TermPtr;

    namespace Lexer
    {
        typedef unsigned TokenKind;

        struct Token
        {
            static constexpr TokenKind InvalidToken = 0;
            static constexpr TokenKind EndOfInputToken = 1;
            static constexpr TokenKind LParenthesisToken = 2;
            static constexpr TokenKind RParenthesisToken = 3;
            static constexpr TokenKind LambdaToken = 4;
            static constexpr TokenKind NamedObjectToken = 5;
            static constexpr TokenKind BoundVariableToken = 6;

            TokenKind Kind;
            char const *Literal;
            size_t Length;
            size_t Value;
            char const *ReasonIfInvalid;
        };

        struct TokenSource
        {
            TokenSource(TokenSource &&) = default;
            TokenSource(TokenSource const &) = default;
            TokenSource &operator = (TokenSource &&) = default;
            TokenSource &operator = (TokenSource const &) = default;
            ~TokenSource() = default;
            explicit TokenSource(char const *in)
                : input(in)
            {
                DiscardCurrent();
            }
            void DiscardCurrent()
            {
                for (; IsWhitespace(*input); ++input)
                    ;
                if (*input == '\0')
                {
                    current = { Token::EndOfInputToken, input, 0 };
                    return;
                }
                if (*input == '(')
                {
                    input += 1;
                    current = { Token::LParenthesisToken, input - 1, 1 };
                    return;
                }
                if (*input == ')')
                {
                    input += 1;
                    current = { Token::RParenthesisToken, input - 1, 1 };
                    return;
                }
                if (*input == '.')
                {
                    input += 1;
                    current = { Token::LambdaToken, input - 1, 1 };
                    return;
                }
                if (StringStartsWith(input, "lambda")
                    && !IsIdentifierFollowingChar(input[6]))
                {
                    input += 6;
                    current = { Token::LambdaToken, input - 6, 6 };
                    return;
                }
                if (IsIdentifierBeginChar(*input))
                {
                    auto begin = input;
                    for (; IsIdentifierFollowingChar(*input); ++input)
                        ;
                    current = { Token::NamedObjectToken, begin, (size_t)(input - begin) };
                    return;
                }
                if (IsDigit(*input))
                {
                    auto begin = input;
                    size_t value = 0;
                    for (; IsDigit(*input); ++input)
                    {
                        value = value * 10 + (*input - '0');
                        if (value > 65536)
                        {
                            for (; IsDigit(*input); ++input)
                                ;
                            current = { Token::InvalidToken, begin, (size_t)(input - begin),
                                0, "Stack too deep (variable index > 65536)." };
                            return;
                        }
                    }
                    if (value == 0)
                    {
                        current = { Token::InvalidToken, begin, (size_t)(input - begin), 0,
                            "Bound variable cannot be 0." };
                        return;
                    }
                    current = { Token::BoundVariableToken, begin, (size_t)(input - begin), value };
                    return;
                }
                current = { Token::InvalidToken, input, 0, 0, "Unrecognised token." };
                return;
            }
            Token const PeekCurrent() const
            {
                return current;
            }
        private:
            char const *input;
            Token current;
            static bool StringContainsChar(char const *str, char ch)
            {
                for (; *str; ++str)
                {
                    if (*str == ch)
                    {
                        return true;
                    }
                }
                return false;
            }
            static bool StringStartsWith(char const *str, char const *pattern)
            {
                for (; *pattern; ++pattern, ++str)
                {
                    if (*str != *pattern)
                    {
                        return false;
                    }
                }
                return true;
            }
            static bool IsWhitespace(char ch)
            {
                return StringContainsChar(" \t\v\b\r\n", ch);
            }
            static bool IsDigit(char ch)
            {
                return ch >= '0' && ch <= '9';
            }
            static bool IsIdentifierBeginChar(char ch)
            {
                return (ch >= 'A' && ch <= 'Z')
                    || (ch >= 'a' && ch <= 'z')
                    || StringContainsChar("~!$%^&*-+=|\\/<>?_", ch);
            }
            static bool IsIdentifierFollowingChar(char ch)
            {
                return IsDigit(ch) || IsIdentifierBeginChar(ch);
            }
        };
    }

    namespace Parser
    {
        /*            Term -> ApplicationTerm* lambda Term
         *            Term -> ApplicationTerm+
         * ApplicationTerm -> const | var | (Term)
         */
        template <typename T>
        struct ParserImpl
        {
            ParserImpl() = delete;
            ParserImpl(ParserImpl &&) = delete;
            ParserImpl(ParserImpl const &) = delete;
            ParserImpl &operator = (ParserImpl &&) = delete;
            ParserImpl &operator = (ParserImpl const &) = delete;
            template <typename U>
            ParserImpl(char const *input, U &&query)
                : src(input), err(nullptr), errpos(nullptr),
                query(std::forward<U>(query))
            { }
            
            Lexer::TokenSource src;
            char const *err;
            char const *errpos;
            T query;

            bool Parse(TermPtr &result)
            {
                if (!ParseTerm(result, 0))
                {
                    return false;
                }
                auto token = src.PeekCurrent();
                switch (token.Kind)
                {
                    case Lexer::Token::InvalidToken:
                        err = "Internal parser error: Invalid token should have been dealt with by ParseTerm.";
                        errpos = nullptr;
                        return false;
                    case Lexer::Token::EndOfInputToken:
                        return true;
                    case Lexer::Token::LParenthesisToken:
                    case Lexer::Token::RParenthesisToken:
                    case Lexer::Token::LambdaToken:
                    case Lexer::Token::NamedObjectToken:
                    case Lexer::Token::BoundVariableToken:
                        err = "Unexpected token. Expecting end of input.";
                        return false;
                    default:
                        err = "Internal parser error: Internal parser error should have been dealt with by ParseTerm.";
                        errpos = nullptr;
                        return false;
                }
            }

            bool ParseTerm(TermPtr &result, size_t sh)
            {
                TermPtr apps, tmp;
                while (true)
                {
                    auto token = src.PeekCurrent();
                    switch (token.Kind)
                    {
                        case Lexer::Token::InvalidToken:
                            err = token.ReasonIfInvalid;
                            errpos = token.Literal;
                            return false;
                        /* Empty expression or Term -> ApplicationTerms+ */
                        case Lexer::Token::EndOfInputToken:
                        case Lexer::Token::RParenthesisToken:
                            result = std::move(apps);
                            err = "(Sub)expression is empty.";
                            errpos = token.Literal;
                            return (bool)result;
                        /* Term -> ApplicationTerms* lambda Term */
                        case Lexer::Token::LambdaToken:
                            src.DiscardCurrent();
                            if (ParseTerm(tmp, sh + 1))
                            {
                                TermPtr abst;
                                abst.NewInstance();
                                abst->AbstractionConstructor(std::move(tmp));
                                if ((bool)apps)
                                {
                                    result.NewInstance();
                                    result->ApplicationConstructor(
                                        std::move(apps), std::move(abst)
                                    );
                                }
                                else
                                {
                                    result = std::move(abst);
                                }
                                return true;
                            }
                            return false;
                        /* ApplicationTerm */
                        case Lexer::Token::LParenthesisToken:
                        case Lexer::Token::BoundVariableToken:
                        case Lexer::Token::NamedObjectToken:
                            if (!ParseApplicationTerm(tmp, sh))
                            {
                                return false;
                            }
                            if ((bool)apps)
                            {
                                auto tmpapps = std::move(apps);
                                apps.NewInstance();
                                apps->ApplicationConstructor(
                                    std::move(tmpapps), std::move(tmp)
                                );
                            }
                            else
                            {
                                apps = std::move(tmp);
                            }
                            break;
                        default:
                            err = "Internal parser error: lexer returns inconsistent data.";
                            errpos = nullptr;
                            return false;
                    }
                }
            }

            bool ParseApplicationTerm(TermPtr &result, size_t sh)
            {
                TermPtr tmp;
                auto token = src.PeekCurrent();
                switch (token.Kind)
                {
                    /* ApplicationTerm -> (Term) */
                    case Lexer::Token::LParenthesisToken:
                        src.DiscardCurrent();
                        if (!ParseTerm(result, sh))
                        {
                            return false;
                        }
                        token = src.PeekCurrent();
                        if (token.Kind != Lexer::Token::RParenthesisToken)
                        {
                            err = "Unexpected token. Expecting closing parenthesis.";
                            errpos = token.Literal;
                            return false;
                        }
                        src.DiscardCurrent();
                        return true;
                    /* ApplicationTerm -> var */
                    case Lexer::Token::BoundVariableToken:
                        if (token.Value > sh)
                        {
                            err = "Stack overflow. Free variable is not supported.";
                            errpos = token.Literal;
                            return false;
                        }
                        src.DiscardCurrent();
                        result.NewInstance();
                        result->BoundVariableConstructor(token.Value);
                        return true;
                    /* ApplicationTerm -> const */
                    case Lexer::Token::NamedObjectToken:
                        if ((bool)query(token.Literal, token.Length, tmp))
                        {
                            result = std::move(tmp);
                            src.DiscardCurrent();
                            return true;
                        }
                        err = "Cannot find the specified named expression.";
                        errpos = token.Literal;
                        return false;
                    case Lexer::Token::InvalidToken:
                    case Lexer::Token::EndOfInputToken:
                    case Lexer::Token::RParenthesisToken:
                    case Lexer::Token::LambdaToken:
                        err = "Internal parser error: unexpected call to ParseApplicationTerm at this token.";
                        errpos = token.Literal;
                        return false;
                    default:
                        err = "Internal parser error: lexer returns inconsistent data.";
                        errpos = nullptr;
                        return false;
                }
            }

        };

        template <typename T>
        bool Parse(char const *input, TermPtr &result,
            char const *&err, char const *&errpos,
            T &&query)
        {
            ParserImpl<T> helper(input, std::forward<T>(query));
            auto ret = helper.Parse(result);
            err = helper.err;
            errpos = helper.errpos;
            return ret;
        }
    }
}

#endif // PARSER_HPP_
