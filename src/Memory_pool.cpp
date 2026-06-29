#include "../include/Memory_pool.h"
#include <cassert>

namespace memoryPool
{
    MemoryPool::MemoryPool(size_t BlockSize)
        : BlockSize_(BlockSize)
    {
    }

    MemoryPool::~MemoryPool()
    {
        Slot *cur = firstBlock_;
        while (cur)
        {
            Slot *next = cur->next;
            // 把 Slot* 链表节点指针，还原成当初 operator new() 返回的原始裸内存地址 void*
            operator delete(reinterpret_cast<void *>(cur));
            cur = next;
        }
    }

    void MemoryPool::init(size_t size)
    {
        /*
        assert(表达式)：运行时校验括号内条件是否成立；
        如果 size <= 0，断言失败，程序直接崩溃并打印出错文件、行号，用来提前拦截非法入参。
        */
        assert(size > 0);
        SlotSize_ = size;
        firstBlock_ = nullptr;
        curSlot_ = nullptr;
        freeList_ = nullptr;
        lastSlot_ = nullptr;
    }

    void *MemoryPool::allocate()
    {
        // 优先使用空闲链表中的内存槽
        if (freeList_ != nullptr)
        {
            std::lock_guard<std::mutex> lock(mutexForFreelist_);
            if (freeList_ != nullptr)
            {
                Slot *tmp = freeList_;
                freeList_ = freeList_->next;
                return tmp;
            }
        }

        Slot *tmp;
        {
            std::lock_guard<std::mutex> lock(mutexForBlock_);
            if (curSlot_ >= lastSlot_)
            {
                // 当前内存已经满了 需要申请新的内存
                allocateNewBlock();
            }
            tmp = curSlot_;
            curSlot_ += SlotSize_ / sizeof(Slot);
        }
        return tmp;
    }

    void MemoryPool::deallocate(void *ptr)
    {
        if (ptr)
        {
            // 回收内存，把内存通过头插法插入到空闲链表中
            std::lock_guard<std::mutex> lock(mutexForFreelist_);
            reinterpret_cast<Slot *>(ptr)->next = freeList_;
            freeList_ = reinterpret_cast<Slot *>(ptr);
        }
    }

    void MemoryPool::allocateNewBlock()
    {
        // std::count << "申请一块新的内存, SlotSize_:" << SlotSize_ << endl;
        //  头插法插入新的内存块
        void *newBlock = operator new(BlockSize_);
        reinterpret_cast<Slot *>(newBlock)->next = firstBlock_;
        firstBlock_ = reinterpret_cast<Slot *>(newBlock);

        char *body = reinterpret_cast<char *>(newBlock) + sizeof(Slot *);
        size_t paddingSize = padPointer(body, SlotSize_);
        curSlot_ = reinterpret_cast<Slot *>(body + paddingSize);

        // 超过该标记位置，则说明该内存块已无内存槽可用，需向系统申请新的内存块；
        lastSlot_ = reinterpret_cast<Slot *>(reinterpret_cast<size_t>(newBlock) + BlockSize_ - SlotSize_ + 1);

        freeList_ = nullptr;
    }

    // 让指针对齐到槽大小的倍数位置
    size_t MemoryPool::padPointer(char *p, size_t align)
    {
        // align是槽大小
        return (align - reinterpret_cast<size_t>(p) % align);
    }

    void HashBucket::initMemoryPool()
    {
        for (int i = 0; i < MEMORY_POOL_NUM; i++)
        {
            getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
        }
    }

    // 单例模式
    MemoryPool &HashBucket::getMemoryPool(int index)
    {
        static MemoryPool memoryPool[MEMORY_POOL_NUM];
        return memoryPool[index];
    }
}