#include <iostream>
#include <mutex>
#include <new>
#include <utility>
#include <cstddef>

namespace memoryPool
{
#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512

    struct Slot
    {
        Slot *next;
    };
    /*
    把外部能用的操作放在public，把内部实现细节 数据 辅助函数全部藏在private
    */
    class MemoryPool
    {
    public:
        MemoryPool(size_t BlockSize = 4096);
        ~MemoryPool();

        void init(size_t); // 初始化该内存池管理的slot大小

        void *allocate();        // 负责从当前内存池中分配一块slot给用户
                                 // 一块原始内存地址，还没有构造对象。真正构造对象是在后面的 newElement<T>() 里完成的。
        void deallocate(void *); // 把用户释放的 slot 放回内存池。

    private:
        void allocateNewBlock();                  // 内部函数 负责向系统申请新的大块内存
        size_t padPointer(char *p, size_t align); // 用于计算地址对齐需要填充多少字节

    private:
        size_t BlockSize_;               // 内存块大小
        size_t SlotSize_;                // 槽大小
        Slot *firstBlock_;            // 指向内存池管理的首个实际内存块
        Slot *curSlot_;               // 指向当前未被使用的槽
        Slot *freeList_;              // 指向空闲的槽（被使用之后又被释放）
        Slot *lastSlot_;              // 指向当前内存块中最后能够存放元素的位置标识（超过该位置需要重新申请新的内存块）
        std::mutex mutexForFreelist_; // 保证freeList_在多线程中操作的原子性
        std::mutex mutexForBlock_;    // 保证多线程情况下避免不必要的重复开辟内存导致的浪费行为
    };
    /*
        是分发器 负责根据用户申请的大小，找到对应的MemoryPool
        申请 1~8B    → 第 0 个 MemoryPool
        申请 9~16B   → 第 1 个 MemoryPool
        申请 17~24B  → 第 2 个 MemoryPool
    */
    class HashBucket
    {
    public:
        static void initMemoryPool();                // 初始化64个内存池
        static MemoryPool &getMemoryPool(int index); // 根据下标返回相应的内存池
        /*
            根据用户申请的大小，决定走哪个内存池。
            ((20 + 7) / 8) - 1 = 2 选择第二个池子
        */
        static void *useMemory(size_t size)
        {
            if (size <= 0)
                return nullptr;
            if (size > MAX_SLOT_SIZE)
                return operator new(size); // 全局库函数，分配未初始化的原始堆内存，不做任何初始化。

            return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();
        }

        // 只回收内存，不负责析构对象
        static void freeMemory(void *ptr, size_t size)
        {
            if (!ptr)
                return;
            if (size > MAX_SLOT_SIZE) // 大对象直接还给系统
            {
                operator delete(ptr);
                return; // return 后面必须跟一个有值的表达式，void 函数调用不能当作返回值。
            }

            getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
        }

        template <typename T, typename... Args>
        friend T *newElement(Args &&...args);

        template <typename T>
        friend void deleteElement(T *p);
    };

    /*
        Args&&... args 是构造函数参数。
        class Student {
        public:
            Student(int age, std::string name);
        };
        可以这样创建  Student* s = newElement<Student>(18, "gdj");
        这里：
            T = Student
            Args = int, const char*
    */
    template <typename T, typename... Args>
    T *newElement(Args &&...args)
    {
        T *p = nullptr;
        if ((p = reinterpret_cast<T *>(HashBucket::useMemory(sizeof(T)))) != nullptr)
            new (p) T(std::forward<Args>(args)...);

        return p;
    }

    // reinterpret_cast<T>(val)：纯粹二进制比特层面强制重新解释内存，不做任何数值转换、不做安全检查、不调整值。

    template <typename T>
    void deleteElement(T *p)
    {
        // 对象析构
        if (p)
        {
            p->~T();
            HashBucket::freeMemory(reinterpret_cast<void *>(p), sizeof(T));
        }
    }
}