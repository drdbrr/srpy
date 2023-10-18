#include <cstdint>
#include <libsigrokcxx/libsigrokcxx.hpp>
#include <libsigrok/libsigrok.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <new>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <new>
#include <iterator>
#include <utility>
#include <memory>
#include <deque>
#include <cstring>
#include <cstddef>
#include <vector>
#include <stdio.h>
#include <unistd.h>

#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <string>

#include "srpcxx.hpp"

//https://github.com/facebook/folly/blob/main/folly/IndexedMemPool.h
//https://github.com/facebook/folly/blob/main/folly/test/IndexedMemPoolTest.cpp
//https://github.com/facebook/folly/blob/main/folly/experimental/LockFreeRingBuffer.h

//https://github.com/boostorg/log/blob/develop/include/boost/log/utility/ipc/reliable_message_queue.hpp
//https://github.com/boostorg/log/blob/develop/src/posix/ipc_reliable_message_queue.cpp

//https://github.com/rigtorp/SPSCQueue/blob/master/include/rigtorp/SPSCQueue.h
//https://github.com/rigtorp/MPMCQueue/blob/master/include/rigtorp/MPMCQueue.h
//https://github.com/ConorWilliams/libfork/tree/main

//static constexpr size_t GET_BLOCK_SIZE(const uint64_t &samplerate, const uint64_t &limit_samples);

//https://radiantsoftware.hashnode.dev/c-lock-free-object-pool


/*
#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
    using std::hardware_destructive_interference_size;
#else
    // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
    constexpr std::size_t hardware_constructive_interference_size = 64;
    constexpr std::size_t hardware_destructive_interference_size = 64;
#endif
*/
constexpr int8_t BUF_FAC = 10;

inline static std::size_t GET_BLK_ALIGN(const size_t blkLim){
    const size_t pgSz = getpagesize();//sysconf(_SC_PAGESIZE);
    return ( ((blkLim) / pgSz) + ( (blkLim) % pgSz != 0 ) ) * pgSz;
}


namespace srp {
    //https://github.com/SeaTalk/lock-free-object-pool/blob/main/object_pool.h
    //https://gist.github.com/jbseg/59613b131a10c57fe7a2b7250a3c96bb
    //https://github.com/MatthiasKillat/lockfree_demo/tree/main
    //https://luyuhuang.tech/2022/10/30/lock-free-queue.html
    //https://siddharths2710.wordpress.com/2017/12/29/lock-free-data-structures/
    //https://www.boost.org/doc/libs/1_83_0/boost/lockfree/queue.hpp
    //https://github.com/facebook/folly/blob/b20a885045570502e3752f3ee2b2bcc9f4c67c96/folly/experimental/flat_combining/FlatCombining.h#L213
    //https://github.com/rigtorp/SPSCQueue/blob/master/include/rigtorp/SPSCQueue.h
    //https://github.com/d36u9/async/blob/master/async/bounded_queue.h
    //https://github.com/max0x7ba/atomic_queue/blob/master/include/atomic_queue/atomic_queue.h
    //https://github.com/facebook/folly/blob/main/folly/IndexedMemPool.h

    struct CountedNodePtr {
        CountedNodePtr() noexcept : external_cnt(1), node_idx(-1) { }
        uint32_t external_cnt;
        int32_t node_idx;
    };

    template<typename T>
    class ObjectPool {
    public:
        struct alignas(64) Node {
            explicit Node(T *s) : obj(s) { }
            T *obj;
            int node_idx;
            CountedNodePtr snext;
            CountedNodePtr qnext;
        };

        template<typename ...Args>
        ObjectPool(uint8_t num, size_t dSize, Args&& ... args):
            blkSz_(GET_BLK_ALIGN(dSize)),
            objs_( static_cast<std::byte*>( std::malloc(num *  (blkSz_ + HEAD_SIZE) )))
        {
            nodes_.reserve(num);
            for (size_t i = 0; i < num; ++i) {
                std::byte *addr = &objs_[i * (blkSz_ + HEAD_SIZE)];
                T* ptr = new(addr)  T(blkSz_, std::forward<Args>(args)...);
                nodes_.emplace_back(ptr);
                nodes_.back().node_idx = i;
                nodes_[i].snext.node_idx = i - 1;
            }
            CountedNodePtr head;
            tail_entry_.store(head);
            head_entry_.store(head);

            head.node_idx = nodes_.size() - 1;
            top_entry_.store(head);
        }

        ~ObjectPool(){
            std::free(objs_);
        }

        void Reclaim(CountedNodePtr nd) {
            if (nd.node_idx < 0) return;
            ++nd.external_cnt;
            nodes_[nd.node_idx].snext = top_entry_.load(std::memory_order_relaxed);
            while(!top_entry_.compare_exchange_weak(nodes_[nd.node_idx].snext, nd, std::memory_order_release, std::memory_order_relaxed));
        }

        CountedNodePtr GetOne() {
            if (nodes_.empty())
                return CountedNodePtr();

            CountedNodePtr old = top_entry_.load(std::memory_order_relaxed);
            while (true) {
                old = IncreaseEntryCnt(old);
                auto node_idx = old.node_idx;
                if (node_idx < 0)
                    break;

                Node *node = &nodes_[node_idx];
                if (top_entry_.compare_exchange_strong(old, node->snext, std::memory_order_relaxed))
                    return old;
            }
            return CountedNodePtr();
        }

        T *GetObj(CountedNodePtr &nd) {
            return (nd.node_idx < 0) ? nullptr : nodes_[nd.node_idx].obj;
        }

        void push(CountedNodePtr nd) {
            ++nd.external_cnt;

            CountedNodePtr old_tail = tail_entry_.load(std::memory_order_relaxed);
            nodes_[old_tail.node_idx].qnext = nd;

            while(!tail_entry_.compare_exchange_weak(nodes_[nd.node_idx].qnext, nd, std::memory_order_release, std::memory_order_relaxed));
            q_size_.fetch_add(1);
        }

        CountedNodePtr pop() {
            CountedNodePtr old_head = head_entry_.load(std::memory_order_relaxed);

            while(true) {
                //old_head = IncrCnt(old_head);
                //auto node_idx = old_head.node_idx;
                //if (node_idx < 0)
                    //break;

                //Node *node = &nodes_[node_idx];
                //if (head_entry_.compare_exchange_strong(old_head, nodes_[old_head.node_idx].qnext/*node->qnext*/, std::memory_order_relaxed)){
                if (head_entry_.compare_exchange_weak(old_head, nodes_[old_head.node_idx].qnext/*node->qnext*/, std::memory_order_release, std::memory_order_relaxed)){
                    q_size_.fetch_sub(1);
                    return nodes_[old_head.node_idx].qnext;
                }
            }
        }

        const int32_t queue_size() {
            return q_size_.load(std::memory_order_acquire);
        }

        const int32_t size() {
            return (nodes_.size() -1) - top_entry_.load(std::memory_order_relaxed).node_idx;;
        };

        const size_t blk_size(){
            return blkSz_ - HEAD_SIZE; //20000736 20000160
        };

    private:
        static constexpr std::size_t HEAD_SIZE = sizeof(T);

        /*
        CountedNodePtr IncrCnt(CountedNodePtr &old_cnt) {
            CountedNodePtr new_cnt;
            do {
                new_cnt = old_cnt;
                ++new_cnt.external_cnt;
            } while (!head_entry_.compare_exchange_strong(old_cnt, new_cnt, std::memory_order_acquire, std::memory_order_relaxed));
            return new_cnt;
        }
        */

        CountedNodePtr IncreaseEntryCnt(CountedNodePtr &old_cnt) {
            CountedNodePtr new_cnt;
            do {
                new_cnt = old_cnt;
                ++new_cnt.external_cnt;
            } while (!top_entry_.compare_exchange_strong( old_cnt, new_cnt, std::memory_order_acquire, std::memory_order_relaxed));
            return new_cnt;
        }

        const size_t blkSz_;
        std::byte *objs_;
        std::vector<Node> nodes_;

        char pad0_[std::hardware_destructive_interference_size];
        alignas(64) std::atomic<CountedNodePtr> top_entry_;         //Source stack TOP
        alignas(64) std::atomic<int32_t> q_size_;
        alignas(64) std::atomic<CountedNodePtr> head_entry_;        //Dest queue head [0]
        alignas(64) std::atomic<CountedNodePtr> tail_entry_;        //Dest queue tail [n]
        char pad1_[std::hardware_destructive_interference_size - 64];
    };

    struct CAnalog {
        CAnalog(std::byte *data, const uint32_t size) : data_(new std::byte[size]), size_(size){
            std::memcpy(data_, data, size);
        };
        ~CAnalog(){
            delete[] data_;
        };
        const uint32_t size_;
        std::byte *data_;
    };

    struct BufSegment {
        explicit BufSegment(const uint32_t size): size_{size} {};
        const uint32_t capacity(){return (size_ - wrPos_);};

        const uint32_t write_data(std::byte* data, uint32_t inSize){
            if( (wrPos_ + inSize) > size_){
                std::cout << "wrPos: " << (int)wrPos_ << " inSz: " << (int)inSize << " sz: " << (int)size_ << std::endl;
                throw std::overflow_error("Segment buffer overflow");
            }

            std::memcpy(&ptr_[wrPos_], data, inSize);
            wrPos_+= inSize;
            return size_ - wrPos_;
        };

        void set(const uint8_t chIdx, const uint32_t smpsSeq){
        //void reset(){
            status_ = false;
            chIdx_ = chIdx;
            smpsSeq_ = smpsSeq;
            wrPos_ = 0;
        };

        bool status_{false};        //READY to read, block is full
        uint8_t chIdx_{0};
        uint32_t wrPos_{0};
        uint32_t smpsSeq_{0};
        const uint32_t size_;       //Size of bytes buffer
        std::byte ptr_[];           //flexible array
    };

    using SegmentPool = ObjectPool<BufSegment>;

    struct AChStat: public std::enable_shared_from_this<AChStat> {
        AChStat(const uint8_t idx, const uint32_t sz) : chIdx_(idx), coBuff_(new std::byte[sz]) {
            //std::cout << "CH: " << (int)bufSeg_->chIdx_ << " SEG idx: " << (int)bufSeg_->idx_ <<  std::endl;
        };
        ~AChStat() { delete[] coBuff_; };

        const uint8_t id(){return chIdx_;};
        const uint8_t chIdx_;
        uint32_t inSmps_{0};

        CountedNodePtr cPtr;

        std::byte *coBuff_;
        uint64_t coSamplesNum_;
        std::vector<CAnalog*> coSamples_;
        std::vector<CAnalog*> data(){
            return coSamples_;
        };

        //uint32_t pck_cnt_ {0};                 // Channel input packets counter
        //uint32_t smps_cnt_ {0};                // Channel input samples counter
    };

    class SrpSamplesSegmented: public std::enable_shared_from_this<SrpSamplesSegmented>
    {
    public:
        SrpSamplesSegmented(const std::string stor_id, const size_t blkLim, std::vector<uint8_t> analog);
        ~SrpSamplesSegmented();

        void append_logic(const sr_datafeed_logic *logic);
        bool append_analog(const sr_datafeed_analog *analog);

        void feed_header();
        void feed_end();

        std::unordered_map<uint8_t, std::shared_ptr<AChStat>> channels_cache() {
            return a_ch_map_;
        };

    private:

        const std::string stor_id_;
        std::unordered_map<uint8_t, std::shared_ptr<AChStat>> a_ch_map_;
        std::atomic_flag ready_{false};                    // signall to consumer thread: the queue has data
        bool acqDone {true};
        std::unique_ptr<SegmentPool> segPool_;

        int32_t blkSz_;
        const uint8_t blkNum_;

        std::thread data_th_;
        void data_proc_th();

        uint32_t n_done_{0};
        uint32_t n_made_{0};
        uint32_t total_pck_cnt_{0};
        uint32_t pck_cnt_{0};

        std::chrono::time_point<std::chrono::high_resolution_clock> t_start_; //https://github.com/skypjack/uvw/blob/b32fd63c833326d6dc0c486c0174e2cd60173ca4/src/uvw/fs_poll.h#L35
        uint64_t t_elapsed_;

        //friend class SrpSession;
        //friend struct std::default_delete<SrpSamplesSegmented>;
    };
};
