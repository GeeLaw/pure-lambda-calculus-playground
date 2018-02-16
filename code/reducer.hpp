#pragma once

#ifndef REDUCER_HPP_
#define REDUCER_HPP_ 1

#include"terms.hpp"
#include<cstdio>

namespace LambdaCalculus
{
    namespace Reduction
    {
        typedef Term::Pointer TermPtr;

        struct EtaConversion : Term::Visitor<EtaConversion, bool (TermPtr &, TermPtr const &)>
        {
            friend struct Term::Visitor<EtaConversion, bool (TermPtr &, TermPtr const &)>;
            static bool Perform(TermPtr &target)
            {
                TermPtr surrogate = target;
                EtaConversion instance;
                instance.dirty = false;
                instance.VisitTerm(target, nullptr);
                surrogate->RecursivelyClearTag();
                return instance.dirty;
            }
        private:
            typedef Utilities::PodSurrogate<bool> Memoisation;
            EtaConversion() = default;
            EtaConversion(EtaConversion const &) = default;
            EtaConversion(EtaConversion &&) = default;
            EtaConversion &operator = (EtaConversion const &) = default;
            EtaConversion &operator = (EtaConversion &&) = default;
            ~EtaConversion() = default;
            bool dirty;
            bool VisitInvalidTerm(TermPtr &, TermPtr const &)
            {
                return false;
            }
            bool VisitInternalErrorTerm(TermPtr &, TermPtr const &)
            {
                return false;
            }
            bool VisitBoundVariableTerm(TermPtr &target, TermPtr const &bound)
            {
                return (target->AsBoundVariable.BoundBy == bound);
            }
            bool VisitAbstractionTerm(TermPtr &target, TermPtr const &bound)
            {
                auto &body = target->AsAbstraction.Result;
                bool const result = VisitTerm(body, bound);
                if (!(bool)target->Tag)
                {
                    auto &memoised = target->Tag.NewInstance<Memoisation>()->Value;
                    memoised = false;
                    if (body->Kind != Term::ApplicationTerm)
                    {
                        return result;
                    }
                    auto &func = body->AsApplication.Function;
                    auto &rplc = body->AsApplication.Replaced;
                    if (rplc->Kind != Term::BoundVariableTerm)
                    {
                        return result;
                    }
                    if (rplc->AsBoundVariable.BoundBy != target)
                    {
                        return result;
                    }
                    if (VisitTerm(func, target))
                    {
                        return result;
                    }
                    dirty = true;
                    memoised = true;
                    target = func;
                    return result;
                }
                if (target->Tag.RawPtrUnsafe<Memoisation>()->Value)
                {
                    target = body->AsApplication.Function;
                }
                return result;
            }
            bool VisitApplicationTerm(TermPtr &target, TermPtr const &bound)
            {
                auto &func = target->AsApplication.Function;
                auto &rplc = target->AsApplication.Replaced;
                return VisitTerm(func, bound) | VisitTerm(rplc, bound);
            }
        };

        struct DeepCloneAndReplace : Term::Visitor<DeepCloneAndReplace, TermPtr (TermPtr const &)>
        {
            friend struct Term::Visitor<DeepCloneAndReplace, TermPtr (TermPtr const &)>;
            static TermPtr Perform(TermPtr const &target, TermPtr const &bound, TermPtr const &replaced)
            {
                DeepCloneAndReplace instance(bound, replaced);
                auto result = instance.VisitTerm(target);
                target->RecursivelyClearTag();
                return result;
            }
        private:
            struct Memoisation
            {
                TermPtr Cloned;
                Memoisation() = delete;
                Memoisation(Memoisation const &) = delete;
                Memoisation(Memoisation &&) = delete;
                Memoisation &operator = (Memoisation const &) = delete;
                Memoisation &operator = (Memoisation &&) = delete;
                void DefaultConstructor()
                {
                    Cloned.DefaultConstructor();
                }
                void Finalise()
                {
                    Cloned.Finalise();
                }
            };
            DeepCloneAndReplace(TermPtr const &bound, TermPtr const &replaced)
                : bound(bound), replaced(replaced)
            { }
            DeepCloneAndReplace(DeepCloneAndReplace &&) = default;
            DeepCloneAndReplace(DeepCloneAndReplace const &) = default;
            DeepCloneAndReplace &operator = (DeepCloneAndReplace &&) = default;
            DeepCloneAndReplace &operator = (DeepCloneAndReplace const &) = default;
            ~DeepCloneAndReplace() = default;
            TermPtr const &bound;
            TermPtr const &replaced;
            TermPtr VisitInvalidTerm(TermPtr const &)
            {
                return nullptr;
            }
            TermPtr VisitInternalErrorTerm(TermPtr const &)
            {
                return nullptr;
            }
            TermPtr VisitBoundVariableTerm(TermPtr const &target)
            {
                auto const &boundBy = target->AsBoundVariable.BoundBy;
                if (boundBy == bound)
                {
                    return replaced;
                }
                if (!(bool)target->Tag)
                {
                    /* Case 1: variable is not bound in the cloned tree. */
                    if (!boundBy->Tag.Is<Memoisation>())
                    {
                        target->Tag.NewInstance<Memoisation>()->Cloned = target;
                    }
                    /* Case 2: variable is bound in the cloned tree. */
                    else
                    {
                        target->Tag.NewInstance<Memoisation>()
                            ->Cloned.NewInstance()
                            ->BoundVariableConstructor(
                                boundBy->Tag.RawPtrUnsafe<Memoisation>()->Cloned
                            );
                    }
                }
                return target->Tag.RawPtrUnsafe<Memoisation>()->Cloned;
            }
            TermPtr VisitAbstractionTerm(TermPtr const &target)
            {
                if (!(bool)target->Tag)
                {
                    auto &clonedAbstraction = target->Tag.NewInstance<Memoisation>()->Cloned;
                    clonedAbstraction.NewInstance();
                    auto clonedResult = VisitTerm(target->AsAbstraction.Result);
                    if ((bool)clonedResult)
                    {
                        clonedAbstraction->AbstractionConstructor(std::move(clonedResult));
                    }
                    else
                    {
                        clonedAbstraction = nullptr;
                    }
                }
                return target->Tag.RawPtrUnsafe<Memoisation>()->Cloned;
            }
            TermPtr VisitApplicationTerm(TermPtr const &target)
            {
                if (!(bool)target->Tag)
                {
                    auto clonedFunc = VisitTerm(target->AsApplication.Function);
                    auto clonedRplc = (bool)clonedFunc
                        ? VisitTerm(target->AsApplication.Replaced)
                        : nullptr;
                    auto memoised = target->Tag.NewInstance<Memoisation>();
                    if ((bool)clonedRplc)
                    {
                        memoised->Cloned.NewInstance()
                            ->ApplicationConstructor(
                                std::move(clonedFunc), std::move(clonedRplc)
                            );
                    }
                }
                return target->Tag.RawPtrUnsafe<Memoisation>()->Cloned;
            }
        };

        /* Perform one step of beta reduction
         * in normal order with call-by-need. */
        struct BetaReduction : Term::Visitor<BetaReduction, void (TermPtr &)>
        {
            friend struct Term::Visitor<BetaReduction, void (TermPtr &)>;
            static bool Perform(TermPtr &target)
            {
                BetaReduction worker;
                worker.VisitTerm(target);
                return worker.replacing;
            }
        private:
            BetaReduction() : replacing(false) { }
            BetaReduction(BetaReduction const &) = default;
            BetaReduction(BetaReduction &&) = default;
            BetaReduction &operator = (BetaReduction const &) = default;
            BetaReduction &operator = (BetaReduction &&) = default;
            ~BetaReduction() = default;
            TermPtr replacer, replacee;
            bool replacing;
            void VisitInvalidTerm(TermPtr &)
            {
            }
            void VisitInternalErrorTerm(TermPtr &)
            {
            }
            void VisitBoundVariableTerm(TermPtr &)
            {
            }
            void VisitAbstractionTerm(TermPtr &target)
            {
                VisitTerm(target->AsAbstraction.Result);
            }
            void VisitApplicationTerm(TermPtr &target)
            {
                auto &func = target->AsApplication.Function;
                auto &rplc = target->AsApplication.Replaced;
                if (replacing)
                {
                    if (target == replacee)
                    {
                        target = replacer;
                        return;
                    }
                    VisitTerm(func);
                    VisitTerm(rplc);
                    return;
                }
                if (func->Kind == Term::AbstractionTerm)
                {
                    replacer = DeepCloneAndReplace::Perform(
                        func->AsAbstraction.Result,
                        func, rplc);
                    replacee = target;
                    replacing = true;
                    target = replacer;
                    return;
                }
                VisitTerm(func);
                VisitTerm(rplc);
            }
        };
    }
}

#endif // REDUCER_HPP_
