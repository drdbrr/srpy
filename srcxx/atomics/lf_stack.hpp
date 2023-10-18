//https://gist.github.com/inspirit/f044cabb9c5000a554fc

#pragma once

#include <atomic>
#include <type_traits>

namespace detail {

template<typename T, size_t PSize> struct pointer_pack {};

template<typename T> struct pointer_pack<T,8>
{
    // 64Bit Machine
    // On AMD64, virtual addresses are 48-bit numbers sign extended to 64.
    // We shift the address left 16 to eliminate the sign extended part and make
    // room in the bottom for the count.
    // In addition to the 16 bits taken from the top, we can take 3 from the
    // bottom, because node must be pointer-aligned, giving a total of 19 bits
    // of count.
    uint64_t operator() (T *node, uintptr_t cnt) const
    {
        return (uint64_t)((uintptr_t)(node))<<16 | (uint64_t)(cnt&((1<<19)-1));
    }
    T* operator() (uint64_t val) const
    {
        return (T*)(uintptr_t)(int64_t(val) >> 19 << 3);
    }
};

// 32Bit Machine
template<typename T> struct pointer_pack<T,4>
{
    uint64_t operator() (T *node, uintptr_t cnt) const {
        return (uint64_t)((uintptr_t)(node))<<32 | (uint64_t)(cnt);
    }

    T* operator() (uint64_t val) const {
        return (T*)(uintptr_t)(val >> 32);
    }
};

} // detail

template<typename T, size_t Capacity>
struct lfstack {
    struct node_t final {
        std::atomic<uint64_t> next;
        uintptr_t pushcnt;
        T data;
    };

    using pointer_pack = detail::pointer_pack<node_t,sizeof(uintptr_t)>;
    using node_type = typename std::aligned_storage<sizeof(node_t), 128>::type;
    node_type _pool[Capacity];

    alignas(128) std::atomic<uint64_t> _head = {0};
    alignas(128) std::atomic<uint64_t> _free = {0};

    lfstack() {
        // push all pool nodes to free stack
        for(size_t i = 0; i < Capacity; ++i)
        {
            node_t* node = reinterpret_cast<node_t*>(&_pool[i]);
            node->pushcnt = 0;
            node->next.store(0, std::memory_order_relaxed);
            _push(_free, node);
        }
        // Issue memory barrier so we can immediately start work
        std::atomic_thread_fence(std::memory_order_release);
    }

    template<class U>
    bool push(U && data) {
        node_t *node = _pop(_free);
        if (node == nullptr)
            return false;
        node->data = std::forward<U>(data);
        _push(_head, node);
        return true;
    }

    bool pop(T& data) {
        node_t *node = _pop(_head);
        if (node == nullptr)
            return false;
        data = std::move(node->data);
        _push(_free, node);
        return true;
    }

    void _push(std::atomic<uint64_t>& h, node_t* node) {
        node->pushcnt++;
        uint64_t packed = pointer_pack()(node, node->pushcnt);
        // check
        // if(pointer_pack()(packed) != node)
        //     throw std::runtime_error("lfstack push invalid packing!");
        uint64_t old = h.load(std::memory_order_relaxed);
        do
        {
            node->next.store(old, std::memory_order_relaxed);
        } while (!h.compare_exchange_weak(old, packed));
    }

    node_t* _pop(std::atomic<uint64_t>& h) {
        uint64_t next, old = h.load(std::memory_order_relaxed);
        node_t* node;
        do
        {
            if(old == 0) return nullptr;
            node = pointer_pack()(old);
            next = node->next.load(std::memory_order_relaxed);
        } while (!h.compare_exchange_weak(old, next));

        return node;
    }
};
