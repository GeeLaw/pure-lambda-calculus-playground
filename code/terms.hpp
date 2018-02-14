#pragma once

#ifndef TERMS_HPP_
#define TERMS_HPP_ 1

#include"utils.hpp"
#include<utility>

namespace LambdaCalculus
{

    typedef unsigned TermKind;

    struct Term
    {
        typedef Utilities::SmartValueTypeRefCountPtr<Term> RefCountPtr;

        static constexpr TermKind InvalidTerm = 0;
        static constexpr TermKind BoundVariableTerm = 1;
        static constexpr TermKind AbstractionTerm = 2;
        static constexpr TermKind ApplicationTerm = 3;

        Term() = delete;
        Term(Term &&) = delete;
        Term(Term const &) = delete;
        Term &operator = (Term &&) = delete;
        Term &operator = (Term const &) = delete;
        ~Term() = delete;

        void DefaultConstructor()
        {
            Kind = InvalidTerm;
        }

        void BoundVariableConstructor(size_t level)
        {
            Kind = BoundVariableTerm;
            AsBoundVariable.Level = level;
        }

        void AbstractionConstructor(RefCountPtr result)
        {
            Kind = AbstractionTerm;
            AsAbstraction.Result.MoveConstructor(std::move(result));
        }

        void ApplicationConstructor(RefCountPtr func, RefCountPtr rplc)
        {
            Kind = ApplicationTerm;
            AsApplication.Function.MoveConstructor(std::move(func));
            AsApplication.Replaced.MoveConstructor(std::move(rplc));
        }

        void Finalise()
        {
            switch (Kind)
            {
                case AbstractionTerm:
                    AsAbstraction.Result.Finalise();
                    break;
                case ApplicationTerm:
                    AsApplication.Function.Finalise();
                    AsApplication.Replaced.Finalise();
                    break;
            }
            Kind = InvalidTerm;
        }

        TermKind Kind;
        /* Convention:
         * - If Kind == InvalidTerm, none of the union members are valid.
         * - If Kind == XTerm, where X is not "Invalid", AsX is valid.
         */
        union
        {
            struct
            {
                size_t Level;
            } AsBoundVariable;
            struct
            {
                RefCountPtr Result;
            } AsAbstraction;
            struct
            {
                RefCountPtr Function;
                RefCountPtr Replaced;
            } AsApplication;
        };

        template <typename TVisitor, typename TFunc>
        struct Visitor;

        template <typename TVisitor, typename TResult, typename...TArgs>
        struct Visitor<TVisitor, TResult(Term *, TArgs...)>
        {
            friend TVisitor;
        private:
            TResult VisitTerm(Term *target, TArgs...args)
            {
                auto that = static_cast<TVisitor *>(this);
                switch (target->Kind)
                {
                    case InvalidTerm:
                        return that->VisitInvalidTerm(target, std::forward<TArgs>(args)...);
                    case BoundVariableTerm:
                        return that->VisitBoundVariableTerm(target, std::forward<TArgs>(args)...);
                    case AbstractionTerm:
                        return that->VisitAbstractionTerm(target, std::forward<TArgs>(args)...);
                    case ApplicationTerm:
                        return that->VisitApplicationTerm(target, std::forward<TArgs>(args)...);
                    default:
                        return that->VisitInternalErrorTerm(target, std::forward<TArgs>(args)...);
                }
            }
        };
    };

}

#endif // TERMS_HPP_
