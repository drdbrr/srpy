#include <libsigrok/libsigrok.h>
#include <libsigrok/proto.h>
#include "srpsamples.hpp"
#include "srpdevice.hpp"
#include "srpsession.hpp"
#include "srpchannels.hpp"
#include "srpconfig.hpp"
#include <cstdlib>

using std::string;
using std::move;

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::time_point;



constexpr uint64_t LIMIT_SIZE(const uint64_t srate){
    const uint32_t t_interval = 100; //milliseconds, 0.1s
    return srate / (1000 / t_interval);
}

inline static uint32_t GET_BLOCK_SIZE(const uint64_t samplerate, const uint64_t limit_samples, uint8_t ch_number){
    const std::size_t limit_size = (std::min( limit_samples, LIMIT_SIZE(samplerate) )) * sizeof(float) * ch_number;
    const std::size_t pgsz = getpagesize();
    return ( (limit_size / pgsz) + ( limit_size % pgsz != 0 ) ) * pgsz;
}

namespace srp {
    BlockNode::BlockNode(std::size_t blk_size) :
        block_(static_cast<std::byte *>(std::malloc(blk_size))),
        status_(FREE),
        next_(nullptr)
    {
    }

    SrpSamples::SrpSamples(const string stor_id, const uint64_t samplerate, const uint64_t limit_samples, std::unordered_map<uint8_t, AChStat*> analog) :
        stor_id_(stor_id),
        samplerate_(samplerate),
        limit_samples_(limit_samples),
        a_ch_map_(std::move(analog)),

        block_size_(GET_BLOCK_SIZE(samplerate_, limit_samples_, a_ch_map_.size())),
        block_num_(BUF_POWER),
        writeIdx_(0),
        n_done_(0),
        wrptr_(nullptr),
        ready_(false)
    {
        BlockNode* tmp_n;
        BlockNode* root_n;
        for(uint8_t i = 0; i < block_num_; i++) {
            tmp_n = new BlockNode{block_size_};
            tmp_n->next_ = nullptr;

            root_n = root_node_.load(std::memory_order_relaxed);
            if (!root_n)
                root_node_.store(tmp_n, std::memory_order_release);

            else {
                BlockNode* visitor_n = root_n;

                while (visitor_n->next_)
                    visitor_n = visitor_n->next_;

                visitor_n->next_ = tmp_n;
            }
        }

        tmp_n->next_ = root_n;
        wrptr_ = root_n->block_;
        write_node_.store(root_n, std::memory_order_release);
        read_node_.store(root_n, std::memory_order_release);
    }

    SrpSamples::~SrpSamples()
    {
        BlockNode *tmp_n;
        for(uint8_t i = 0; i < block_num_; i++) {
            BlockNode *tmp_n = root_node_.load(std::memory_order_relaxed);
            delete root_node_;
            root_node_.store(tmp_n->next_);
        }
        root_node_ = nullptr;
        read_node_ = nullptr;
        write_node_ = nullptr;

        if (data_th_.joinable())
            data_th_.join();

        std::cout << "Destroy Storage: " << stor_id_ << std::endl;
    }

    void SrpSamples::feed_header()
    {
        std::cout << "HEADER packet" << std::endl;
        acqDone = false;
        data_th_ = std::thread(&SrpSamples::data_proc_th, this);
        t_start_ = high_resolution_clock::now();
    }


    void SrpSamples::feed_end()
    {
        acqDone = true;
        ready_.test_and_set();
        ready_.notify_one();

        time_point<high_resolution_clock> t_end = high_resolution_clock::now();
        t_elapsed_ = duration_cast<std::chrono::milliseconds>(t_end - t_start_).count();

        std::cout << "**END packet: " << std::endl
        << "    Time delta: " << (double)t_elapsed_ / 1000 << "s" << std::endl
        << "    Thread ID: " << std::this_thread::get_id() << std::endl
        << "    Blocks produced: " << n_made_ << std::endl;
    }

    void SrpSamples::append_logic(const sr_datafeed_logic *logic)
    {
        //std::cout << "L" << std::endl;
    }

    bool SrpSamples::append_analog(const sr_datafeed_analog *analog)
    {
        auto const *ch = static_cast<const struct sr_channel *>(analog->meaning->channels->data);
        const uint32_t in_size = analog->num_samples * sizeof(float);
        AChStat *ch_stat = a_ch_map_[ch->index];

        const std::size_t end_size = writeIdx_ + PH_SIZE + in_size;

        if (end_size > block_size_ ) {

            //ATTENTION: КРИТИЧЕСКАЯ СЕКЦИЯ
            BlockNode *write_node = write_node_.load(std::memory_order_acquire);
            BlockNode *next_n = write_node->next_;

            /*
            if (next_n->status_ != FREE){
                if (block_num_ < MAX_BUF_POW){
                    BlockNode* tmp_n = new BlockNode{block_size_};
                    tmp_n->next_ = next_n;
                    write_node->next_ = tmp_n;
                    next_n = tmp_n;
                    block_num_++;
                    std::cout << "  **Buffer Is full. Extending to " << (int)block_num_  << std::endl;
                }
                else{
                    std::cout << "  **Buffer Overflow. Programm Terminaing" << std::endl;
                    std::exit(EXIT_FAILURE);
                    return false;
                }
            }
            */

            if (next_n->status_ == FREE){
                write_node->status_ = RD;
                write_node_.store(write_node, std::memory_order_release);
                next_n->status_ = WR;
                wrptr_ = next_n->block_;
                writeIdx_ = 0;
                write_node_.store(next_n, std::memory_order_release);
                n_ready_.fetch_add(1, std::memory_order_release);
                n_made_++;

                ready_.test_and_set();
                ready_.notify_one();
            }
            else{
                std::cout << "  **Buffer Overflow. Programm Terminaing" << std::endl;
                std::exit(EXIT_FAILURE);
                return false;
            }
            //ready_.test_and_set();
            //ready_.notify_one();
        }

        //WARNING: НЕ КРИТИЧЕСКАЯ СЕКЦИЯ
        std::construct_at((PacketHeader *)&wrptr_[writeIdx_], ch->index, analog->num_samples, ch_stat->pck_num_cnt_, ch_stat->pck_smps_cnt_);
        writeIdx_ += PH_SIZE;
        std::memcpy(&wrptr_[writeIdx_], analog->data, in_size);
        writeIdx_ += in_size;

        //uint32_t *num = reinterpret_cast<uint32_t *>(wrptr_);

        //*num++;

        ch_stat->pck_num_cnt_++;
        ch_stat->pck_smps_cnt_ += analog->num_samples;
        return true;
    }

    void SrpSamples::parse(std::byte *tmp)
    {
        PacketHeader *head = std::bit_cast<PacketHeader *>(&tmp[0]);
        std::cout << "Block done: " << n_done_ << " ch i: " << head->index << " len: " << head->length << " pck_num: " << head->pck_num << std::endl;
    }

    void SrpSamples::data_proc_th()
    {
        std::cout << "START DATA THREAD" << std::endl;
        std::byte *tmp = static_cast<std::byte *>(std::malloc(block_size_));
        std::memset(tmp, '\0', block_size_);

        while(!acqDone){
            ready_.wait(acqDone);
            ready_.clear();

            while(uint8_t i = n_ready_.load(std::memory_order_relaxed)){
                BlockNode *rdblk = read_node_.load(std::memory_order_acquire);

                if (rdblk->status_ == RD){
                    std::memcpy(tmp, rdblk->block_, block_size_);
                    parse(tmp);
                    rdblk->status_ = FREE;
                    BlockNode *next_n = rdblk->next_;
                    n_done_++;
                    read_node_.store(next_n, std::memory_order_release);
                    n_ready_.fetch_sub(1, std::memory_order_release);
                }
                else {
                    std::cout << "  **Buffer: Is empty. No blocks to read" << std::endl;
                }
            }
            //std::this_thread::sleep_for(std::chrono::milliseconds(100));

        }

        std::free(tmp);
        std::cout << "**END DATA THREAD:" << std::endl
        << "    Blocks processed: " << n_done_ << std::endl;
    }
}
