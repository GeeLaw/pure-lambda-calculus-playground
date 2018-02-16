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
        typedef Utilities::RefCountPtr<Term> Pointer;

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
            Tag.DefaultConstructor();
        }

        void BoundVariableConstructor(Pointer boundBy)
        {
            Kind = BoundVariableTerm;
            AsBoundVariable.BoundBy.MoveConstructor(std::move(boundBy));
            AsBoundVariable.BoundBy.DecreaseReference();
            Tag.DefaultConstructor();
        }

        void AbstractionConstructor(Pointer result)
        {
            Kind = AbstractionTerm;
            AsAbstraction.Result.MoveConstructor(std::move(result));
            Tag.DefaultConstructor();
        }

        void ApplicationConstructor(Pointer func, Pointer rplc)
        {
            Kind = ApplicationTerm;
            AsApplication.Function.MoveConstructor(std::move(func));
            AsApplication.Replaced.MoveConstructor(std::move(rplc));
            Tag.DefaultConstructor();
        }

        void Finalise()
        {
            switch (Kind)
            {
                case BoundVariableTerm:
                    /* We must NOT destroy AsBoundVariable.BoundBy
                     * because the finalisation of this object
                     * is caused by this BoundBy.
                     * Note that we cannot first IncreaseReference
                     * then Finalise the BoundBy, which will cause
                     * the finalisation of BoundBy to run a second
                     * time.
                     */
                    break;
                case AbstractionTerm:
                    AsAbstraction.Result.Finalise();
                    break;
                case ApplicationTerm:
                    AsApplication.Function.Finalise();
                    AsApplication.Replaced.Finalise();
                    break;
            }
            Tag.Finalise();
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
                /* Be careful to avoid circular references! */
                Pointer BoundBy;
            } AsBoundVariable;
            struct
            {
                Pointer Result;
            } AsAbstraction;
            struct
            {
                Pointer Function;
                Pointer Replaced;
            } AsApplication;
        };
        Utilities::VariantPtr Tag;

    private:
        /* The argument U is used to avoid full
         * template specialisation inside a struct,
         * which is a defect in C++11.
         */
        template <typename T, typename U = void>
        struct VisitorPointerCheck
        {
            typedef VisitorPointerCheck<T, U> THelper;
            static_assert(sizeof(THelper) != sizeof(THelper),
                "The first argument must be of type "
                "Term (const) * (const), "
                "Term::Pointer, "
                "Term::Pointer const & "
                "or Term::Pointer &.");
        };
        template <typename U>
        struct VisitorPointerCheck<Term *, U>
        {
            typedef Term *AdjustedPointer;
        };
        template <typename U>
        struct VisitorPointerCheck<Term * const, U>
        {
            typedef Term *AdjustedPointer;
        };
        template <typename U>
        struct VisitorPointerCheck<Term const *, U>
        {
            typedef Term const *AdjustedPointer;
        };
        template <typename U>
        struct VisitorPointerCheck<Term const * const, U>
        {
            typedef Term const *AdjustedPointer;
        };
        template <typename U>
        struct VisitorPointerCheck<Pointer, U>
        {
            typedef Pointer const &AdjustedPointer;
        };
        template <typename U>
        struct VisitorPointerCheck<Pointer const &, U>
        {
            typedef Pointer const &AdjustedPointer;
        };
        template <typename U>
        struct VisitorPointerCheck<Pointer &, U>
        {
            typedef Pointer &AdjustedPointer;
        };

    public:
        template <typename TVisitor, typename TFunc>
        struct Visitor;

        template <typename TVisitor, typename TResult, typename TPointer, typename...TArgs>
        struct Visitor<TVisitor, TResult (TPointer, TArgs...)>
        {
            friend TVisitor;
        private:
            TResult VisitTerm(typename VisitorPointerCheck<TPointer>::AdjustedPointer target, TArgs...args)
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
