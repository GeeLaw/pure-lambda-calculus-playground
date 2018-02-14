#pragma once

#ifndef UTILS_HPP_
#define UTILS_HPP_ 1

#include<cstdlib>
#include<utility>
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
    struct SmartValueTypeRefCountMemPool
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
        SmartValueTypeRefCountMemPool(size_t suggested = 16)
            : entries(nullptr), blocks(nullptr),
            nextAlloc(suggested < 16 ? 16 : suggested > 1024 ? 1024 : suggested),
            currentCount(0)
        {
        }
        SmartValueTypeRefCountMemPool(SmartValueTypeRefCountMemPool const &) = delete;
        SmartValueTypeRefCountMemPool(SmartValueTypeRefCountMemPool &&) = delete;
        SmartValueTypeRefCountMemPool &operator = (SmartValueTypeRefCountMemPool const &) = delete;
        SmartValueTypeRefCountMemPool &operator = (SmartValueTypeRefCountMemPool &&) = delete;
        ~SmartValueTypeRefCountMemPool()
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
    private:
        Entry *entries;
        struct Block
        {
            Block *NextBlock;
        } *blocks;
        size_t nextAlloc;
        size_t currentCount;
    };

    /* SmartValueTypeRefCountPtr itself is a smart value type. */
    template <typename TSmartValueType>
    struct SmartValueTypeRefCountPtr
    {
        SmartValueTypeRefCountPtr()
        {
            DefaultConstructor();
        }
        SmartValueTypeRefCountPtr(SmartValueTypeRefCountPtr const &other)
        {
            CopyConstructor(other);
        }
        SmartValueTypeRefCountPtr(SmartValueTypeRefCountPtr &&other)
        {
            MoveConstructor(std::move(other));
        }
        SmartValueTypeRefCountPtr &operator = (SmartValueTypeRefCountPtr &&other)
        {
            if (entry != other.entry)
            {
                Finalise();
                MoveConstructor(std::move(other));
            }
            return *this;
        }
        SmartValueTypeRefCountPtr &operator = (SmartValueTypeRefCountPtr const &other)
        {
            if (entry != other.entry)
            {
                Finalise();
                CopyConstructor(other);
            }
            return *this;
        }
        void DefaultConstructor()
        {
            entry = nullptr;
        }
        void CopyConstructor(SmartValueTypeRefCountPtr const &other)
        {
            entry = other.entry;
            if ((bool)entry)
            {
                ++entry->ReferenceCount;
            }
        }
        void MoveConstructor(SmartValueTypeRefCountPtr &&other)
        {
            entry = other.entry;
            other.entry = nullptr;
        }
        void Finalise()
        {
            if ((bool)entry && --entry->ReferenceCount == 0)
            {
                pool.Deallocate(entry);
            }
            entry = nullptr;
        }
        ~SmartValueTypeRefCountPtr()
        {
            Finalise();
        }
        bool NewInstance()
        {
            Finalise();
            entry = pool.Allocate();
            if ((bool)entry)
            {
                ++entry->ReferenceCount;
                return true;
            }
            return false;
        }
        TSmartValueType *operator -> () const
        {
            return RawPtr();
        }
        TSmartValueType *RawPtr() const
        {
            return entry ? &entry->Data : nullptr;
        }
        operator bool () const
        {
            return (bool)entry;
        }
    private:
        typename SmartValueTypeRefCountMemPool<TSmartValueType>::Entry *entry;
        static SmartValueTypeRefCountMemPool<TSmartValueType> pool;
    };

    template <typename TSmartValueType>
    SmartValueTypeRefCountMemPool<TSmartValueType> SmartValueTypeRefCountPtr<TSmartValueType>::pool;

}

#endif // UTILS_HPP_
