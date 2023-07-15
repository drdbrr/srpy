#include <libsigrokcxx/libsigrokcxx.hpp>
#include <libsigrok/libsigrok.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <new>
#include <iostream>
#include "srpcxx.hpp"



namespace srp {
    enum BlockStatus
    {
        FREE = 0,
        WR,
        RD,
    };

    struct BlockNode {
        std::byte *block_;
        BlockStatus status_;
        BlockNode *next_;
        BlockNode(std::size_t blk_size);
        ~BlockNode(){
            std::free(block_);
        };
    };

    struct PacketHeader
    {
        short int index;    // channel index
        uint32_t length;    // samples number
        uint32_t pck_num;   // packet number
        uint64_t pck_smps;  // samples position number
    };

    constexpr std::size_t BUF_POWER = 2;
    //constexpr std::size_t MAX_BUF_POW = 5;
    constexpr std::size_t PH_SIZE = sizeof(PacketHeader);

    struct AChStat {
        uint32_t pck_num_cnt_ {0};
        uint64_t pck_smps_cnt_ {0};
    };

    class SrpSamples : public std::enable_shared_from_this<SrpSamples>
    {
    public:
        SrpSamples(const std::string stor_id, const uint64_t samplerate, const uint64_t limit_samples, std::unordered_map<uint8_t, AChStat*> analog);
        ~SrpSamples();

        void append_logic(const sr_datafeed_logic *logic);
        bool append_analog(const sr_datafeed_analog *analog);

        void feed_header();
        void feed_end();
        void parse(std::byte *tmp);

    private:
        const std::string stor_id_;
        const uint64_t samplerate_;
        const uint64_t limit_samples_;              // SR_CONF_LIMIT_MSEC, SR_CONF_LIMIT_SAMPLES, SR_CONF_LIMIT_FRAMES, SR_CONF_CONTINUOUS
        std::unordered_map<uint8_t, AChStat*> a_ch_map_;



        const std::size_t block_size_;
        uint8_t block_num_;
        uint32_t writeIdx_;

        std::byte *wrptr_;



        using AtomicIndex = std::atomic<uint32_t>;

        char pad0_[std::hardware_destructive_interference_size];


        std::atomic<BlockNode*> root_node_ {nullptr};
        std::atomic<BlockNode*> write_node_ {nullptr};
        std::atomic<BlockNode*> read_node_ {nullptr};

        alignas(std::hardware_destructive_interference_size) AtomicIndex n_ready_ {0};

        std::atomic_flag ready_;                    // signall to consumer thread: the queue has data

        char pad1_[std::hardware_destructive_interference_size - sizeof(AtomicIndex)];

        uint32_t n_done_{0};
        uint32_t n_made_{0};

        bool acqDone {true};

        std::thread data_th_;
        void data_proc_th();



        std::chrono::time_point<std::chrono::high_resolution_clock> t_start_;
        uint64_t t_elapsed_;

        friend class SrpSession;
        friend struct std::default_delete<SrpSamples>;
    };
};
