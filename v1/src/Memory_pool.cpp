#include "../include/Memory_pool.h"
#include <cassert>

namespace memoryPool
{
    MemoryPool::MemoryPool(size_t BlockSize)
        /*
        这是构造函数初始化列表 类似于赋值BlockSize_ = BlockSize
        二者顶层逻辑不一样
        初始化是变量 / 对象创建时第一次赋予初始值，发生在对象诞生瞬间，
        是从无到有；赋值是对象已存在后，后续覆盖修改已有数值，是从旧到新，
        且 const、引用等成员仅支持初始化、不支持赋值，初始化效率也高于先默认构造再赋值的方式。
        */
        : BlockSize_(BlockSize), SlotSize_(0), firstBlock_(nullptr),
          curSlot_(nullptr), freeList_(nullptr), lastSlot_(nullptr)
    {
    }
    // 析构函数 释放内存池申请过的所有大块内存
    MemoryPool::~MemoryPool()
    {
        Slot *cur = firstBlock_;
        while (cur)
        {
            Slot *next = cur->next;
            // 把 Slot* 链表节点指针，还原成当初 operator new() 返回的原始裸内存地址 void*
            // reinterpret_cast<void *>(cur) 强制类型二进制原样转换
            // Slot* 指针原样地址不变，转成 void* 裸指针，不做任何内存拷贝、地址偏移，只改变编译器对这块地址的解读类型。
            operator delete(reinterpret_cast<void *>(cur));
            cur = next;
        }
    }

    // 实现无锁入队操作
    bool MemoryPool::pushFreeList(Slot *slot)
    {
        while (true)
        {
            // 获取当前头节点
            Slot *oldHead = freeList_.load(std::memory_order_relaxed);
            // 将新节点的next指向当前头结点
            slot->next.store(oldHead, std::memory_order_relaxed);

            // 尝试将新节点设置为头结点
            if (freeList_.compare_exchange_weak(oldHead, slot, std::memory_order_release, std::memory_order_relaxed))
            {
                return true;
            }
            // CAS失败则重试
        }
    }

    // 实现无锁出队操作
    Slot *MemoryPool::popFreeList()
    {
        while (true)
        {
            Slot *oldHead = freeList_.load(std::memory_order_relaxed);
            if (oldHead == nullptr)
            {
                return nullptr;
            }

            // 获取下一个节点
            Slot *newHead = oldHead->next.load(std::memory_order_relaxed);

            // 尝试更新头结点
            if (freeList_.compare_exchange_weak(oldHead, newHead, std::memory_order_acquire, std::memory_order_relaxed))
            {
                return oldHead;
            }
            // CAS失败则重试
        }
    }

    // 从当前MemoryPool中，拿出一个可用的Slot
    void *MemoryPool::allocate()
    {
        // 优先使用空闲链表中的内存槽
        Slot *slot = popFreeList();
        if (slot != nullptr)
        {
            return slot;
        }

        // 如果空闲链接为空 则分配新的内存
        std::lock_guard<std::mutex> lock(mutexForBlock_);
        if (curSlot_ >= lastSlot_)
        {
            allocateNewBlock();
        }

        Slot *result = curSlot_;
        curSlot_ = reinterpret_cast<Slot *>(reinterpret_cast<char *>(curSlot_) + SlotSize_);
        return result;
    }

    void MemoryPool::deallocate(void *ptr)
    {
        if (!ptr)
            return;
        Slot *slot = static_cast<Slot *>(ptr);
        pushFreeList(slot);
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

    /*
    单例模式
    让整个程序只存在一份64个内存池数组，所有申请/释放内存的地方都共用这一份内存池
    */

    MemoryPool &HashBucket::getMemoryPool(int index)
    {
        /*
        1. 只会初始化一次
        2. 生命周期持续到程序结束
        3. 所有调用 getMemoryPool() 的地方访问的是同一份 memoryPool 数组
        */
        static MemoryPool memoryPool[MEMORY_POOL_NUM];
        return memoryPool[index];
    }
}