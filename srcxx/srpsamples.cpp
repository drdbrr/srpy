#include <atomic>
#include <libsigrok/libsigrok.h>
#include <libsigrok/proto.h>
#include "srpsamples.hpp"
#include "srpdevice.hpp"
#include "srpsession.hpp"
#include "srpchannels.hpp"
#include "srpconfig.hpp"
#include <cstdlib>

#include <iomanip>

#include <execution>
#include <algorithm>
#include <iterator>
#include <functional>


#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>

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

inline static std::size_t GET_BLK_ALIGN(const size_t blkLim){
    const size_t pgSz = getpagesize();
    return ( (blkLim / pgSz) + ( blkLim % pgSz != 0 ) ) * pgSz;
}

namespace srp {
    SrpSamples::SrpSamples(const std::string stor_id, const size_t blkLim, std::vector<uint8_t> analog) :
        stor_id_(stor_id),
        blkSz_(GET_BLK_ALIGN(blkLim * analog.size())),
        buff_{new std::byte[blkSz_ * BLK_NUM]},
        comp_codec_(getCodec(CodecType::ZLIB, 9))
    {
        //std::cout << "blkSz_: " << (int)blkSz_ << std::endl;
        //const size_t limit_size =  (std::min( limit_samples_, LIMIT_SIZE(samplerate_) )) * sizeof(float);
        //const size_t pgsz = getpagesize();
        //const size_t blksz = ( ((limit_size * analog.size()) / pgsz) + ( (limit_size * analog.size()) % pgsz != 0 ) ) * pgsz;

        for (const auto chIdx : analog){
            std::shared_ptr<AChStat> stat {new AChStat{chIdx}};
            //stat->iobuf_->create(blkSz_/analog.size());
            stat->iobuf_ = IOBuf::create(blkSz_ / analog.size());

            //obuf_ = new float[blkSz_ / analog.size() / 4];

            stat->vobuf_.reserve(blkSz_ / analog.size() / 4);

            a_ch_map_.emplace(chIdx, stat);
        }

        for(uint8_t i = 0; i < BLK_NUM; i++){
            std::shared_ptr<BufBlk> blk {new BufBlk{i, blkSz_, &buff_[i*blkSz_]}};
            in_buff_.emplace_back(blk);
        }

        in_buff_.shrink_to_fit();
    }

    SrpSamples::~SrpSamples()
    {
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
        auto wrIdx = writeIdx_.load(std::memory_order_relaxed);
        auto blk = in_buff_[wrIdx];
        blk->status_ = true;
        blk->wrSeq_++;
        wrIdx++;
        if(wrIdx >= in_buff_.size())
            wrIdx = 0;
        writeIdx_.store(wrIdx, std::memory_order_release);


        acqDone = true;
        ready_.test_and_set();
        ready_.notify_one();

        time_point<high_resolution_clock> t_end = high_resolution_clock::now();
        t_elapsed_ = duration_cast<std::chrono::milliseconds>(t_end - t_start_).count();

        std::cout << "**END packet: " << std::endl
        << "    Total income packets: " << total_pck_cnt_ << std::endl
        << "    Time delta: " << (double)t_elapsed_ / 1000 << "s" << std::endl
        << "    Thread ID: " << std::this_thread::get_id() << std::endl
        << "    Blocks produced: " << n_made_ << std::endl;
    }

    void SrpSamples::append_logic(const sr_datafeed_logic *logic)
    {
        //std::cout << "L" << std::endl;
    }



    //https://habr.com/ru/articles/708918/
    //https://en.cppreference.com/w/cpp/atomic/atomic/compare_exchange
    //https://preshing.com/20150402/you-can-do-any-kind-of-atomic-read-modify-write-operation/
    //https://en.algorithmica.org/hpc/cpu-cache/bandwidth/#bypassing-the-cache

    //https://github.com/facebook/folly/blob/main/folly/ProducerConsumerQueue.h
    //https://rigtorp.se/ringbuffer/
    //https://github.com/facebook/folly/blob/main/folly/AtomicLinkedList.h
    //https://github.com/facebook/folly/blob/main/folly/AtomicIntrusiveLinkedList.h
    //https://stackoverflow.com/questions/43243726/magic-ring-buffer-implementation-in-linux-kernel-space
    //https://www.kernel.org/doc/Documentation/trace/ring-buffer-design.txt

    std::shared_ptr<SrpSamples::BufBlk> SrpSamples::get_blk(size_t in_size){
        auto wrIdx = writeIdx_.load(std::memory_order_relaxed);
        auto blk = in_buff_[wrIdx];

        const uint32_t nextWrPos = blk->wrPos_ + (in_size + PCK_H_SIZE);

        if( blk->size_ < nextWrPos ){
            blk->status_ = true;
            blk->wrSeq_++;

            wrIdx++;
            if(wrIdx >= in_buff_.size())
                wrIdx = 0;

            if (wrIdx == readIdx_.load(std::memory_order_acquire)) {
                std::cout << "BUFFER OVERFLOW" << std::endl;
                std::terminate();
            }

            writeIdx_.store(wrIdx, std::memory_order_release);
            ready_.test_and_set();
            ready_.notify_one();
            blk = in_buff_[wrIdx];
        }
        return blk;
    }

    bool SrpSamples::append_analog(const sr_datafeed_analog *analog){
        auto const *ch = static_cast<const struct sr_channel *>(analog->meaning->channels->data);
        auto const ch_stat = a_ch_map_[ch->index];
        const uint32_t in_size = analog->num_samples * sizeof(float);

        //std::cout << "Cap: " << (int)ch_stat->iobuf_->capacity() << std::endl;

        auto const blk = get_blk(in_size);
        std::byte *addr = &blk->buf_[blk->wrPos_];

        new(addr) PckHeader{ch->index, ch_stat->smps_cnt_, in_size, analog->data};

        blk->wrPos_+= PCK_H_SIZE + in_size;
        blk->pckNum_++;

        ch_stat->smps_cnt_ += analog->num_samples;
        ch_stat->pck_cnt_++;

        return true;
    }

    //  https://numpy.org/doc/stable/reference/c-api/array.html#c.PyArray_SimpleNewFromData


    //https://github.com/facebook/folly/blob/b8f65b19372207c60889836787b7734f0124bfa9/folly/compression/test/CompressionTest.cpp#L789
    //https://github.com/facebook/folly/blob/f513832be29decb1bb7c5b97f17f6162c57b048c/folly/compression/test/CompressionTest.cpp#L826
    bool SrpSamples::handle_block(std::shared_ptr<BufBlk> blk){
        PckHeader *packet;
        uint64_t pckIdx = 0;
        uint32_t i;

        for(i = 0; i < blk->pckNum_; i++){
            //packet = std::bit_cast<PckHeader *>(&blk->buf_[pckIdx]);
            packet = (PckHeader*)&blk->buf_[pckIdx];
            pckIdx += sizeof(PckHeader) + packet->size_;


            auto const ch_stat = a_ch_map_[packet->chIdx_];

            /*
            if(ch_stat->vobuf_.capacity() - ch_stat->vobuf_.size() < packet->size_/4)
                ch_stat->vobuf_.clear();

            std::vector<float> src(packet->data_, packet->data_+packet->size_);
            std::copy(std::execution::par_unseq, src.begin(), src.end(), std::back_inserter(ch_stat->vobuf_));
            */


            /*
            if( (blkSz_/a_ch_map_.size()/4 - ch_stat->outWrPos) < packet->size_/4)
                ch_stat->outWrPos = 0;

            float *dest = &ch_stat->obuf_[ch_stat->outWrPos];
            std::memcpy(dest, (float*)packet->data_, packet->size_/4);
            ch_stat->outWrPos += packet->size_/4;
            */

            //std::cout << "CH: " << (int)packet->chIdx_ << " data[" << (int)packet->size_/4 << "]: " << (float)dest[packet->size_/4] << std::endl;


            if(ch_stat->iobuf_->tailroom() < packet->size_)
                ch_stat->iobuf_->clear();

            std::memcpy(ch_stat->iobuf_->writableTail(), packet->data_, packet->size_);
            ch_stat->iobuf_->append(packet->size_);
        }

        for (auto const& [key, ch_stat] : a_ch_map_){
            ch_stat->comp_out_.push_back(comp_codec_->compress(ch_stat->iobuf_.get()));
            std::cout << "Out sz: " << ch_stat->comp_out_.size() << std::endl;


            ch_stat->iobuf_->clear();
        }

        //std::cout << "Packets parsed: " << (int)i << std::endl;
        return true;
    }

    //https://github.com/facebook/folly/blob/main/folly/experimental/channels/detail/AtomicQueue.h
    //https://github.com/Moneyl/BinaryTools/blob/master/BinaryTools/MemoryBuffer.h
    //https://blog.devgenius.io/a-simple-guide-to-atomics-in-c-670fc4842c8b

    void SrpSamples::data_proc_th()
    {
        std::cout << "START DATA THREAD" << std::endl;

        while(!acqDone){
            ready_.wait(acqDone);
            ready_.clear();

            auto const rdIdx = readIdx_.load(std::memory_order_relaxed);
            auto const wrIdx = writeIdx_.load(std::memory_order_acquire);

            int8_t q_size = wrIdx - rdIdx;
            if (q_size < 0)
                q_size += in_buff_.size();

            uint8_t nextRecord = rdIdx;
            for(uint8_t i = 0; i < q_size; i++){
                nextRecord++;
                //uint8_t nextRecord = rdIdx + 1;
                if (nextRecord == in_buff_.size())
                    nextRecord = 0;
                auto const blk = in_buff_[rdIdx];
                if(blk->status_){
                    //std::cout << "BLK id: " << (int)blk->id_ << " pckNum: " << blk->pckNum_ << " wrSeq: " << (int)blk->wrSeq_<< std::endl;
                    handle_block(blk);

                    blk->wrPos_ = 0;
                    blk->pckNum_ = 0;
                    blk->status_ = false;
                    readIdx_.store(nextRecord, std::memory_order_release);
                }
            }
            //readIdx_.store(nextRecord, std::memory_order_release);
            //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::cout << "**END DATA THREAD:" << std::endl
        << "    Total packets processed:" << total_pck_done_ << std::endl
        << "    Blocks processed: " << n_done_ << std::endl;
    }
}
