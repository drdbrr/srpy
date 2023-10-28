#ifndef SRP_SAMPLES_SEGMENTED_HPP
#define SRP_SAMPLES_SEGMENTED_HPP

#include <libsigrokcxx/libsigrokcxx.hpp>

#include <thread>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <new>

#include <unistd.h>

namespace srp {
    constexpr uint8_t BUF_FAC = 10;

    static inline const uint32_t GET_BLK_ALIGN(const uint32_t blkLim) {
        const uint32_t pgSz = getpagesize();//sysconf(_SC_PAGESIZE);
        return ( ((blkLim) / pgSz) + ( (blkLim) % pgSz != 0 ) ) * pgSz;
    }

    struct BufSegment {
        explicit BufSegment(const uint32_t size) noexcept;
        const uint32_t capacity() const;
        const uint32_t write_data(std::byte* data, uint32_t inSize);
        void set(const uint8_t chIdx, const uint32_t smpsSeq);
        bool status_{false};        //READY to read, block is full
        uint8_t chIdx_{0};
        uint32_t wrPos_{0};
        uint32_t smpsSeq_{0};
        const uint32_t size_;       //Size of bytes buffer
        std::byte ptr_[];           //flexible array
    };

    struct CountPtr {
        CountPtr() noexcept : external_cnt(1), node_idx(-1) { }
        uint32_t external_cnt;
        int32_t node_idx;
    };

    struct alignas(64) Node {
        explicit Node(BufSegment *s) : obj(s) { };
        BufSegment *obj;
        int node_idx;
        CountPtr snext;
        CountPtr qnext;
    };

    class ObjectPool {
    public:
        template<typename ...Args>
        ObjectPool(uint8_t num, size_t dSize, Args&& ... args);

        ~ObjectPool();
        void Reclaim(CountPtr nd);
        CountPtr GetOne();
        BufSegment* GetObj(CountPtr &nd);

        void push(CountPtr nd);
        CountPtr pop();
        const int32_t queue_size() const;
        const int32_t size() const;
        const size_t blk_size() const;

    private:
        static constexpr std::size_t HEAD_SIZE = sizeof(BufSegment);
        CountPtr IncreaseEntryCnt(CountPtr &old_cnt);
        const size_t blkSz_;
        std::byte *objs_;
        std::vector<Node> nodes_;

        char pad0_[std::hardware_destructive_interference_size];
        alignas(64) std::atomic<CountPtr> top_entry_;         //Source stack TOP
        alignas(64) std::atomic<int32_t> q_size_;
        alignas(64) std::atomic<CountPtr> head_entry_;        //Dest queue head [0]
        alignas(64) std::atomic<CountPtr> tail_entry_;        //Dest queue tail [n]
        char pad1_[std::hardware_destructive_interference_size - 64];
    };

    struct CAnalog {
        CAnalog(std::byte *data, const uint32_t size);
        ~CAnalog();

        const uint32_t size_;
        std::byte *data_;
    };

    struct AChStat: public std::enable_shared_from_this<AChStat> {
        AChStat(const uint8_t idx, const uint32_t sz);
        ~AChStat();

        const uint8_t id()const;
        const uint8_t chIdx_;
        uint32_t inSmps_{0};

        CountPtr cPtr;

        std::byte *coBuff_;
        uint64_t coSamplesNum_;
        std::vector<CAnalog*> coSamples_;
        std::vector<CAnalog*> data();

        //uint32_t pck_cnt_ {0};                 // Channel input packets counter
        //uint32_t smps_cnt_ {0};                // Channel input samples counter
    };

    class SrpSamplesSegmented: public std::enable_shared_from_this<SrpSamplesSegmented> {
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
        std::unique_ptr<ObjectPool> segPool_;

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

        //friend struct std::default_delete<SrpSamplesSegmented>;
    };
};

#endif
