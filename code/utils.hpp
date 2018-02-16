#pragma once

#ifndef UTILS_HPP_
#define UTILS_HPP_ 1

#include<cstdlib>
#include<utility>
#include<cstddef>
#include<typeinfo>

namespace Utilities
{

    /* A smart value type is a structure, where:
     * - It has DefaultConstructor() member function that
     *   initialises the structure from garbage data.
     * - It has Finalise() member function that clears up the
     *   resources for reuse (does not need to reset the memory).
     */

    template <typename TSmartValueType>
    struct RefCountMemPool
    {
        struct Entry
        {
            union
            {
                Entry *NextEntry;
                size_t ReferenceCount;
            };
            TSmartValueType Data;
            Entry() = delete;
            Entry(Entry &&) = delete;
            Entry(Entry const &) = delete;
            Entry &operator = (Entry &&) = delete;
            Entry &operator = (Entry const &) = delete;
            ~Entry() = delete;
        };
        RefCountMemPool(size_t suggested = 16)
            : entries(nullptr), blocks(nullptr),
            nextAlloc(suggested < 16 ? 16 : suggested > 1024 ? 1024 : suggested),
            currentCount(0)
        {
        }
        RefCountMemPool(RefCountMemPool const &) = delete;
        RefCountMemPool(RefCountMemPool &&) = delete;
        RefCountMemPool &operator = (RefCountMemPool const &) = delete;
        RefCountMemPool &operator = (RefCountMemPool &&) = delete;
        ~RefCountMemPool()
        {
            for (auto i = blocks; i; )
            {
                auto ni = i->NextBlock;
                std::free(i);
                i = ni;
            }
        }
        size_t Capacity() const { return currentCount; }
        bool EnsureCapacity(size_t expect)
        {
            if (expect <= currentCount)
            {
                return true;
            }
            /* Adjust number of entries to allocate. */
            size_t toAlloc = expect - currentCount;
            toAlloc = (toAlloc < nextAlloc ? nextAlloc : toAlloc);
            nextAlloc = (toAlloc < 2048 ? toAlloc * 2 : 4096);
            /* Allocate a new block and prepend it to the block list. */
            auto newBlock = (Block *)std::malloc(sizeof(Block) + sizeof(Entry) * toAlloc);
            if (!(bool)newBlock)
            {
                return false;
            }
            newBlock->NextBlock = blocks;
            blocks = newBlock;
            /* Link the new entries. */
            auto newEntries = (Entry *)(void *)(newBlock + 1);
            for (size_t i = 0; i + 1 != toAlloc; ++i)
            {
                newEntries[i].NextEntry = newEntries + (i + 1);
            }
            newEntries[toAlloc - 1].NextEntry = entries;
            entries = newEntries;
            currentCount += toAlloc;
            return true;
        }
        Entry *Allocate()
        {
            if (!EnsureCapacity(1))
            {
                return nullptr;
            }
            auto entry = entries;
            entries = entry->NextEntry;
            entry->ReferenceCount = 0;
            entry->Data.DefaultConstructor();
            --currentCount;
            return entry;
        }
        void Deallocate(Entry *entry)
        {
            entry->Data.Finalise();
            entry->NextEntry = entries;
            entries = entry;
            ++currentCount;
        }
        static RefCountMemPool<TSmartValueType> Default;
    private:
        Entry *entries;
        struct Block
        {
            Block *NextBlock;
        } *blocks;
        size_t nextAlloc;
        size_t currentCount;
    };

    template <typename TSmartValueType>
    RefCountMemPool<TSmartValueType>
        RefCountMemPool<TSmartValueType>::Default;

    struct VariantPtr;

    /* RefCountPtr itself is a smart value type. */
    template <typename TSmartValueType>
    struct RefCountPtr
    {
        friend struct VariantPtr;
    private:
        typedef RefCountMemPool<TSmartValueType> MemPool;
        typename MemPool::Entry *entry;
        RefCountPtr(typename MemPool::Entry *ptr)
        {
            FromEntryConstructor(ptr);
        }
        void FromEntryConstructor(typename MemPool::Entry *ptr)
        {
            entry = ptr;
            IncreaseReference();
        }
    public:
        RefCountPtr(std::nullptr_t = nullptr)
        {
            DefaultConstructor();
        }
        RefCountPtr(RefCountPtr const &other)
        {
            CopyConstructor(other);
        }
        RefCountPtr(RefCountPtr &&other)
        {
            MoveConstructor(std::move(other));
        }
        RefCountPtr &operator = (std::nullptr_t)
        {
            Finalise();
            DefaultConstructor();
            return *this;
        }
        RefCountPtr &operator = (RefCountPtr other)
        {
            if (entry != other.entry)
            {
                auto tmp = entry;
                entry = other.entry;
                other.entry = tmp;
            }
            return *this;
        }
        void DefaultConstructor()
        {
            entry = nullptr;
        }
        void CopyConstructor(RefCountPtr const &other)
        {
            entry = other.entry;
            IncreaseReference();
        }
        void MoveConstructor(RefCountPtr &&other)
        {
            entry = other.entry;
            other.entry = nullptr;
        }
        void Finalise()
        {
            if ((bool)entry && --entry->ReferenceCount == 0)
            {
                MemPool::Default.Deallocate(entry);
            }
        }
        ~RefCountPtr()
        {
            Finalise();
        }
        TSmartValueType *NewInstance()
        {
            Finalise();
            entry = MemPool::Default.Allocate();
            if ((bool)entry)
            {
                ++entry->ReferenceCount;
                return &entry->Data;
            }
            return nullptr;
        }
        TSmartValueType *operator -> () const
        {
            return RawPtr();
        }
        TSmartValueType *RawPtr() const
        {
            return (bool)entry ? &entry->Data : nullptr;
        }
        explicit operator bool () const
        {
            return (bool)entry;
        }
        friend bool operator == (RefCountPtr const &a, RefCountPtr const &b)
        {
            return a.entry == b.entry;
        }
        friend bool operator == (RefCountPtr const &a, TSmartValueType const *b)
        {
            return a.RawPtr() == b;
        }
        friend bool operator == (TSmartValueType const *a, RefCountPtr const &b)
        {
            return a == b.RawPtr();
        }
        friend bool operator == (RefCountPtr const &ptr, std::nullptr_t)
        {
            return !(bool)ptr.entry;
        }
        friend bool operator == (std::nullptr_t, RefCountPtr const &ptr)
        {
            return !(bool)ptr.entry;
        }
        friend bool operator != (RefCountPtr const &a, RefCountPtr const &b)
        {
            return a.entry != b.entry;
        }
        friend bool operator != (RefCountPtr const &a, TSmartValueType const *b)
        {
            return a.RawPtr() != b;
        }
        friend bool operator != (TSmartValueType const *a, RefCountPtr const &b)
        {
            return a != b.RawPtr();
        }
        friend bool operator != (RefCountPtr const &ptr, std::nullptr_t)
        {
            return (bool)ptr.entry;
        }
        friend bool operator != (std::nullptr_t, RefCountPtr const &ptr)
        {
            return (bool)ptr.entry;
        }
        void IncreaseReference() const
        {
            if ((bool)entry)
            {
                ++entry->ReferenceCount;
            }
        }
        void DecreaseReference() const
        {
            if ((bool)entry)
            {
                --entry->ReferenceCount;
            }
        }
        void Forget()
        {
            entry = nullptr;
        }
    };

    struct VariantPtr
    {
    private:
        std::type_info const *type;
        void *entry;
        typedef void ManipulatorFunc(void *entry);
        ManipulatorFunc *incRef, *decRef, *delRef;
        template <typename T>
        static void IncreaseReferenceStatic(void *entry)
        {
            auto typed = (typename RefCountMemPool<T>::Entry *)entry;
            ++typed->ReferenceCount;
        }
        template <typename T>
        static void DecreaseReferenceStatic(void *entry)
        {
            auto typed = (typename RefCountMemPool<T>::Entry *)entry;
            --typed->ReferenceCount;
        }
        template <typename T>
        static void ReleaseReferenceStatic(void *entry)
        {
            auto typed = (typename RefCountMemPool<T>::Entry *)entry;
            if (!--typed->ReferenceCount)
            {
                RefCountMemPool<T>::Default.Deallocate(typed);
            }
        }
    public:
        VariantPtr(std::nullptr_t = nullptr)
        {
            DefaultConstructor();
        }
        VariantPtr(VariantPtr const &other)
        {
            CopyConstructor(other);
        }
        VariantPtr(VariantPtr &&other)
        {
            MoveConstructor(std::move(other));
        }
        template <typename T>
        VariantPtr(RefCountPtr<T> const &ptr)
        {
            FromTypedPtrConstructor(ptr);
        }
        ~VariantPtr() { Finalise(); }
        VariantPtr &operator = (std::nullptr_t)
        {
            Finalise();
            DefaultConstructor();
            return *this;
        }
        VariantPtr &operator = (VariantPtr other)
        {
            if (entry != other.entry)
            {
#define SWAP_MEMBER_HELPER_(member) do { auto tmp = member; member = other.member; other.member = tmp; } while (false)
                SWAP_MEMBER_HELPER_(type);
                SWAP_MEMBER_HELPER_(entry);
                SWAP_MEMBER_HELPER_(incRef);
                SWAP_MEMBER_HELPER_(decRef);
                SWAP_MEMBER_HELPER_(delRef);
#undef SWAP_MEMBER_HELPER_
            }
            return *this;
        }
        template <typename T>
        VariantPtr &operator = (RefCountPtr<T> const &ptr)
        {
            if (entry != ptr.entry)
            {
                VariantPtr toss = std::move(*this);
                FromTypedPtrConstructor(ptr);
            }
            return *this;
        }
        void DefaultConstructor()
        {
            type = nullptr;
            entry = nullptr;
            incRef = nullptr;
            decRef = nullptr;
            delRef = nullptr;
        }
        void CopyConstructor(VariantPtr const &other)
        {
            type = other.type;
            entry = other.entry;
            incRef = other.incRef;
            decRef = other.decRef;
            delRef = other.delRef;
            if ((bool)entry)
            {
                incRef(entry);
            }
        }
        void MoveConstructor(VariantPtr &&other)
        {
            type = other.type;
            entry = other.entry;
            incRef = other.incRef;
            decRef = other.decRef;
            delRef = other.delRef;
            other.type = nullptr;
            other.entry = nullptr;
            other.incRef = nullptr;
            other.decRef = nullptr;
            other.delRef = nullptr;
        }
        template <typename T>
        void FromTypedPtrConstructor(RefCountPtr<T> const &ptr)
        {
            if (!(bool)ptr.entry)
            {
                DefaultConstructor();
            }
            else
            {
                type = &typeid(T);
                entry = ptr.entry;
                incRef = &IncreaseReferenceStatic<T>;
                decRef = &DecreaseReferenceStatic<T>;
                delRef = &ReleaseReferenceStatic<T>;
                incRef(entry);
            }
        }
        void Finalise()
        {
            if ((bool)entry)
            {
                delRef(entry);
            }
        }
        template <typename T>
        T *NewInstance()
        {
            Finalise();
            auto typed = RefCountMemPool<T>::Default.Allocate();
            if ((bool)typed)
            {
                type = &typeid(T);
                entry = typed;
                incRef = &IncreaseReferenceStatic<T>;
                decRef = &DecreaseReferenceStatic<T>;
                delRef = &ReleaseReferenceStatic<T>;
                incRef(entry);
            }
            else
            {
                DefaultConstructor();
            }
            return &typed->Data;
        }
        template <typename T>
        T *RawPtr() const
        {
            return Is<T>()
                ? RawPtrUnsafe<T>()
                : nullptr;
        }
        template <typename T>
        T *RawPtrUnsafe() const
        {
            return &((typename RefCountMemPool<T>::Entry *)entry)->Data;
        }
        template <typename T>
        RefCountPtr<T> As() const
        {
            return Is<T>()
                ? AsUnsafe<T>()
                : nullptr;
        }
        template <typename T>
        RefCountPtr<T> AsUnsafe() const
        {
            return (typename RefCountMemPool<T>::Entry *)entry;
        }
        template <typename T>
        bool Is() const
        {
            return type != nullptr && *type == typeid(T);
        }
        explicit operator bool () const
        {
            return (bool)entry;
        }
        friend bool operator == (VariantPtr const &a, VariantPtr const &b)
        {
            return a.entry == b.entry;
        }
        friend bool operator == (VariantPtr const &ptr, std::nullptr_t)
        {
            return !(bool)ptr.entry;
        }
        friend bool operator == (std::nullptr_t, VariantPtr const &ptr)
        {
            return !(bool)ptr.entry;
        }
        template <typename T>
        friend bool operator == (VariantPtr const &a, RefCountPtr<T> const &b)
        {
            return a.entry == b.entry;
        }
        template <typename T>
        friend bool operator == (RefCountPtr<T> const &a, VariantPtr const &b)
        {
            return a.entry == b.entry;
        }
        template <typename T>
        friend bool operator == (VariantPtr const &variant, T const *raw)
        {
            return (!(bool)variant.entry && !(bool)raw)
                || (variant.Is<T>() && variant.RawPtrUnsafe<T>() == raw);
        }
        template <typename T>
        friend bool operator == (T const *raw, VariantPtr const &variant)
        {
            return (!(bool)variant.entry && !(bool)raw)
                || (variant.Is<T>() && variant.RawPtrUnsafe<T>() == raw);
        }
        friend bool operator != (VariantPtr const &a, VariantPtr const &b)
        {
            return a.entry != b.entry;
        }
        friend bool operator != (VariantPtr const &ptr, std::nullptr_t)
        {
            return (bool)ptr.entry;
        }
        friend bool operator != (std::nullptr_t, VariantPtr const &ptr)
        {
            return (bool)ptr.entry;
        }
        template <typename T>
        friend bool operator != (VariantPtr const &a, RefCountPtr<T> const &b)
        {
            return a.entry != b.entry;
        }
        template <typename T>
        friend bool operator != (RefCountPtr<T> const &a, VariantPtr const &b)
        {
            return a.entry != b.entry;
        }
        template <typename T>
        friend bool operator != (VariantPtr const &variant, T const *raw)
        {
            return !(variant == raw);
        }
        template <typename T>
        friend bool operator != (T const *raw, VariantPtr const &variant)
        {
            return !(raw == variant);
        }
        void IncreaseReference()
        {
            if ((bool)entry)
            {
                incRef(entry);
            }
        }
        void DecreaseReference()
        {
            if ((bool)entry)
            {
                decRef(entry);
            }
        }
        void Forget()
        {
            DefaultConstructor();
        }
    };

    template <typename T>
    struct PodSurrogate
    {
        PodSurrogate() = delete;
        PodSurrogate(PodSurrogate const &) = delete;
        PodSurrogate(PodSurrogate &&) = delete;
        PodSurrogate &operator = (PodSurrogate const &) = delete;
        PodSurrogate &operator = (PodSurrogate &&) = delete;
        ~PodSurrogate() = delete;
        void DefaultConstructor() { }
        void Finalise() { }
        T Value;
    };

}

#endif // UTILS_HPP_
