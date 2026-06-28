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

    void* MemoryPool::allocate()
    {
        
    }
}