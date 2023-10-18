#include <cstdint>
#include <libsigrokcxx/libsigrokcxx.hpp>
#include <libsigrok/libsigrok.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <new>
#include <iostream>

#include <cstdlib>

#include <iterator>
#include <utility>
#include <memory>
#include <assert.h>
#include <deque>

//#include <folly/io/IOBuf.h>
//#include <folly/compression/Compression.h>

#include <cstring>

//#include "srpzstd.hpp"
#include "srpcxx.hpp"




#define BLK_NUM 15
constexpr int8_t BUF_FAC = 3;

// using folly::IOBuf;
// using folly::io::getStreamCodec;
// using folly::io::CodecType;
// using folly::io::StreamCodec;
//
// using folly::io::getCodec;
// using folly::io::Codec;

namespace srp {

    class SrpSamples : public std::enable_shared_from_this<SrpSamples>
    {
    public:
        //SrpSamples(const std::string stor_id, const uint64_t samplerate, const uint64_t limit_samples, std::vector<uint8_t> analog);
        SrpSamples(const std::string stor_id, const size_t blkLim, std::vector<uint8_t> analog);
        ~SrpSamples();

        void append_logic(const sr_datafeed_logic *logic);
        bool append_analog(const sr_datafeed_analog *analog);

        void feed_header();
        void feed_end();

    private:
        struct AChStat {
            ~AChStat(){
                //std::cout << "AChStat id: " << (int)chIdx_ << " is desturcted" << std::endl;
            };
            const uint8_t chIdx_;
            std::unique_ptr<IOBuf> iobuf_;

            float *obuf_;
            size_t outWrPos{0};

            std::vector<float> vobuf_;


            std::vector<std::unique_ptr<IOBuf>> comp_out_;

            uint32_t smps_cnt_ {0};                // input packets counter
            uint64_t pck_cnt_ {0};                 // input samples counter
        };

        struct BufBlk {
            BufBlk(uint8_t i, size_t size, std::byte* ptr): id_{i}, size_{size}, buf_{ptr} {};
            BufBlk(uint8_t i, size_t size): id_{i}, size_{size}, buf_{new std::byte[size]} {};
            ~BufBlk(){
                delete[] buf_;
                //std::cout << "BLK id: " << (int)id_ << " is desturcted" << std::endl;
            };
            const uint8_t id_;
            bool status_{false};        //READY to read, block is full
            size_t wrPos_{0};
            const size_t size_;
            uint32_t pckNum_ {0};       //NOTE: Number of written packets
            uint32_t wrSeq_{0};         //NOTE: Block write sequence counter for debug
            uint32_t rdSeq_{0};
            //std::byte ptr_[];         //flexible array
            std::byte *buf_;
        };


        struct PckHeader{
            PckHeader(uint8_t chIdx, uint64_t smpsNum, size_t size, void *data):
                chIdx_{chIdx},
                smpsSeq_{smpsNum},
                size_{size}
            {
                std::memcpy(&data_, data, size);
            };
            ~PckHeader(){};
            const uint8_t chIdx_;           //channel index
            const uint64_t smpsSeq_;        //number of packet
            const std::size_t size_;        //packet size in bytes
            char data_[];
        };

        const std::string stor_id_;
        //const uint64_t samplerate_;
        //const uint64_t limit_samples_;              // SR_CONF_LIMIT_MSEC, SR_CONF_LIMIT_SAMPLES, SR_CONF_LIMIT_FRAMES, SR_CONF_CONTINUOUS
        const size_t blkSz_;                                                                          //Size of block
        //const uint8_t blkNum_;                                                                        //Pool size

        std::unordered_map<uint8_t, std::shared_ptr<AChStat>> a_ch_map_;
        std::atomic_flag ready_{false};                    // signall to consumer thread: the queue has data
        bool acqDone {true};

        bool handle_block(std::shared_ptr<BufBlk> blk);

        static constexpr std::size_t PCK_H_SIZE = sizeof(PckHeader);
        std::shared_ptr<BufBlk> get_blk(size_t in_size);

        std::byte* buff_;
        std::vector<std::shared_ptr<BufBlk>> in_buff_;

        std::thread data_th_;
        void data_proc_th();


        std::unique_ptr<Codec> comp_codec_;

        uint32_t n_done_{0};
        uint32_t n_made_{0};
        uint32_t total_pck_cnt_{0};
        uint32_t pck_cnt_{0};
        uint32_t total_pck_done_{0};

        std::chrono::time_point<std::chrono::high_resolution_clock> t_start_;
        uint64_t t_elapsed_;

        //std::unique_ptr<SrpZstd> zstd_;

        using AtomicIndex = std::atomic<uint8_t>;
        char pad0_[std::hardware_destructive_interference_size];
        alignas(std::hardware_destructive_interference_size) AtomicIndex readIdx_ {0};
        //alignas(std::hardware_destructive_interference_size) AtomicIndex writeIdxCached_;
        alignas(std::hardware_destructive_interference_size) AtomicIndex writeIdx_ {0};
        //alignas(std::hardware_destructive_interference_size) AtomicIndex readIdxCached_;
        char pad1_[std::hardware_destructive_interference_size - sizeof(AtomicIndex)];



        friend class SrpSession;
        friend struct std::default_delete<SrpSamples>;
    };
};
