#pragma once

#ifndef PARSER_HPP_
#define PARSER_HPP_ 1

#include"terms.hpp"

namespace DeBruijnIndex
{
    typedef LambdaCalculus::Term::Pointer TermPtr;

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
                for (; *str && *str != ch; ++str)
                    ;
                return *str;
            }
            static bool StringStartsWith(char const *str, char const *pattern)
            {
                for (; *pattern && *(pattern++) == *(str++); )
                    ;
                return !*pattern;
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
        struct AbstractionBoundStack
        {
        private:
            typedef Utilities::RefCountMemPool<AbstractionBoundStack> MemPool;
        public:
            AbstractionBoundStack() = delete;
            AbstractionBoundStack(AbstractionBoundStack const &) = delete;
            AbstractionBoundStack(AbstractionBoundStack &&) = delete;
            AbstractionBoundStack &operator = (AbstractionBoundStack const &) = delete;
            AbstractionBoundStack &operator = (AbstractionBoundStack &&) = delete;
            ~AbstractionBoundStack() = delete;
            typedef MemPool::Entry *Pointer;
            static Pointer Push(TermPtr entry, Pointer stack)
            {
                auto result = MemPool::Default.Allocate();
                result->Data.Entry.MoveConstructor(std::move(entry));
                result->Data.LastEntry = stack;
                return result;
            }
            static Pointer Pop(Pointer stack)
            {
                auto result = stack->Data.LastEntry;
                stack->Data.Entry.Finalise();
                MemPool::Default.Deallocate(stack);
                return result;
            }
            void DefaultConstructor() { }
            void Finalise() { }
            TermPtr Entry;
            Pointer LastEntry;
        };

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
            ParserImpl(char const *input, U &&constants)
                : stack(nullptr), src(input),
                err(nullptr), errpos(nullptr),
                constants(std::forward<U>(constants))
            { }

            AbstractionBoundStack::Pointer stack;
            Lexer::TokenSource src;
            char const *err;
            char const *errpos;
            T constants;

            TermPtr Parse()
            {
                auto result = ParseTerm();
                if (!(bool)result)
                {
                    return nullptr;
                }
                auto token = src.PeekCurrent();
                switch (token.Kind)
                {
                    case Lexer::Token::InvalidToken:
                        err = "Internal parser error: Invalid token should have been dealt with by ParseTerm.";
                        errpos = nullptr;
                        return nullptr;
                    case Lexer::Token::EndOfInputToken:
                        return result;
                    case Lexer::Token::LParenthesisToken:
                    case Lexer::Token::RParenthesisToken:
                    case Lexer::Token::LambdaToken:
                    case Lexer::Token::NamedObjectToken:
                    case Lexer::Token::BoundVariableToken:
                        err = "Unexpected token. Expecting end of input.";
                        errpos = token.Literal;
                        return nullptr;
                    default:
                        err = "Internal parser error: Internal parser error should have been dealt with by ParseTerm.";
                        errpos = nullptr;
                        return nullptr;
                }
            }

            TermPtr ParseTerm()
            {
                TermPtr application;
                while (true)
                {
                    auto token = src.PeekCurrent();
                    switch (token.Kind)
                    {
                        case Lexer::Token::InvalidToken:
                        {
                            err = token.ReasonIfInvalid;
                            errpos = token.Literal;
                            return nullptr;
                        }
                        /* Empty expression or Term -> ApplicationTerms+ */
                        case Lexer::Token::EndOfInputToken:
                        case Lexer::Token::RParenthesisToken:
                        {
                            err = "(Sub)expression is empty.";
                            errpos = token.Literal;
                            return application;
                        }
                        /* Term -> ApplicationTerms* lambda Term */
                        case Lexer::Token::LambdaToken:
                        {
                            if (!(bool)application)
                            {
                                return ParseAbstractionTerm();
                            }
                            TermPtr abstraction = ParseAbstractionTerm();
                            if (!(bool)abstraction)
                            {
                                return nullptr;
                            }
                            TermPtr result;
                            result.NewInstance()->ApplicationConstructor(
                                std::move(application), std::move(abstraction)
                            );
                            return result;
                        }
                        /* ApplicationTerm */
                        case Lexer::Token::LParenthesisToken:
                        case Lexer::Token::BoundVariableToken:
                        case Lexer::Token::NamedObjectToken:
                        {
                            if ((bool)application)
                            {
                                auto term = ParseApplicationTerm();
                                if (!(bool)term)
                                {
                                    return nullptr;
                                }
                                auto nested = std::move(application);
                                application.NewInstance()->ApplicationConstructor(
                                    std::move(nested), std::move(term)
                                );
                            }
                            else
                            {
                                application = ParseApplicationTerm();
                                if (!(bool)application)
                                {
                                    return nullptr;
                                }
                            }
                            break;
                        }
                        default:
                        {
                            err = "Internal parser error: lexer returns inconsistent data.";
                            errpos = nullptr;
                            return nullptr;
                        }
                    }
                }
            }

            TermPtr ParseApplicationTerm()
            {
                auto token = src.PeekCurrent();
                switch (token.Kind)
                {
                    /* ApplicationTerm -> (Term) */
                    case Lexer::Token::LParenthesisToken:
                    {
                        src.DiscardCurrent();
                        auto result = ParseTerm();
                        if (!(bool)result)
                        {
                            return nullptr;
                        }
                        token = src.PeekCurrent();
                        if (token.Kind != Lexer::Token::RParenthesisToken)
                        {
                            err = "Unexpected token. Expecting closing parenthesis.";
                            errpos = token.Literal;
                            return nullptr;
                        }
                        src.DiscardCurrent();
                        return result;
                    }
                    /* ApplicationTerm -> var */
                    case Lexer::Token::BoundVariableToken:
                    {
                        auto boundBy = stack;
                        for (auto k = token.Value;
                            (bool)--k && (bool)boundBy;
                            boundBy = boundBy->Data.LastEntry)
                            ;
                        if (!(bool)boundBy)
                        {
                            err = "Stack overflow. Free variable is not supported.";
                            errpos = token.Literal;
                            return nullptr;
                        }
                        TermPtr result;
                        result.NewInstance()->BoundVariableConstructor(boundBy->Data.Entry);
                        src.DiscardCurrent();
                        return result;
                    }
                    /* ApplicationTerm -> const */
                    case Lexer::Token::NamedObjectToken:
                    {
                        auto result = constants(token.Literal, token.Length);
                        if ((bool)result)
                        {
                            src.DiscardCurrent();
                            return result;
                        }
                        err = "Cannot find the specified named expression.";
                        errpos = token.Literal;
                        return nullptr;
                    }
                    case Lexer::Token::InvalidToken:
                    case Lexer::Token::EndOfInputToken:
                    case Lexer::Token::RParenthesisToken:
                    case Lexer::Token::LambdaToken:
                    {
                        err = "Internal parser error: unexpected call to ParseApplicationTerm at this token.";
                        errpos = token.Literal;
                        return nullptr;
                    }
                    default:
                    {
                        err = "Internal parser error: lexer returns inconsistent data.";
                        errpos = nullptr;
                        return nullptr;
                    }
                }
            }

            TermPtr ParseAbstractionTerm()
            {
                auto token = src.PeekCurrent();
                if (token.Kind == Lexer::Token::InvalidToken)
                {
                    err = "Internal parser error: lexer returns inconsistent data.";
                    errpos = nullptr;
                    return nullptr;
                }
                if (token.Kind != Lexer::Token::LambdaToken)
                {
                    err = "Internal parser error: unexpected call to ParseAbstractionTerm at this token.";
                    errpos = token.Literal;
                    return nullptr;
                }
                src.DiscardCurrent();
                TermPtr result;
                result.NewInstance();
                stack = AbstractionBoundStack::Push(result, stack);
                auto abstractee = ParseTerm();
                stack = AbstractionBoundStack::Pop(stack);
                if ((bool)abstractee)
                {
                    result->AbstractionConstructor(std::move(abstractee));
                    return result;
                }
                return nullptr;
            }

        };

        template <typename T>
        bool Parse(char const *input, TermPtr &result,
            char const *&err, char const *&errpos,
            T &&constants)
        {
            ParserImpl<T> helper(input, std::forward<T>(constants));
            result = helper.Parse();
            err = helper.err;
            errpos = helper.errpos;
            return (bool)result;
        }
    }
}

#endif // PARSER_HPP_
