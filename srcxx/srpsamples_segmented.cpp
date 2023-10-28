#include "srpsamples_segmented.hpp"
#include <iostream>
#include <cstring>
#include <exception>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>


using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::time_point;

namespace srp {

    template<typename ...Args>
    ObjectPool::ObjectPool(uint8_t num, size_t dSize, Args&& ... args):
        blkSz_(GET_BLK_ALIGN(dSize)),
        objs_( static_cast<std::byte*>( std::malloc(num *  (blkSz_ + HEAD_SIZE) )))
    {
        nodes_.reserve(num);
        for (size_t i = 0; i < num; ++i) {
            std::byte *addr = &objs_[i * (blkSz_ + HEAD_SIZE)];
            BufSegment* ptr = new(addr)  BufSegment(blkSz_, std::forward<Args>(args)...);
            nodes_.emplace_back(ptr);
            nodes_.back().node_idx = i;
            nodes_[i].snext.node_idx = i - 1;
        }
        CountPtr head;
        tail_entry_.store(head);
        head_entry_.store(head);

        head.node_idx = nodes_.size() - 1;
        top_entry_.store(head);
    };

    ObjectPool::~ObjectPool() {
        std::free(objs_);
    };

    void ObjectPool::Reclaim(CountPtr nd) {
        if (nd.node_idx < 0) return;
        ++nd.external_cnt;
        nodes_[nd.node_idx].snext = top_entry_.load(std::memory_order_relaxed);
        while(!top_entry_.compare_exchange_weak(nodes_[nd.node_idx].snext, nd, std::memory_order_release, std::memory_order_relaxed));
    };

    CountPtr ObjectPool::GetOne() {
        if (nodes_.empty())
            return CountPtr();

        CountPtr old = top_entry_.load(std::memory_order_relaxed);
        while (true) {
            old = IncreaseEntryCnt(old);
            auto node_idx = old.node_idx;
            if (node_idx < 0)
                break;

            Node *node = &nodes_[node_idx];
            if (top_entry_.compare_exchange_strong(old, node->snext, std::memory_order_relaxed))
                return old;
        }
        return CountPtr();
    };

    BufSegment* ObjectPool::GetObj(CountPtr &nd) {
        return (nd.node_idx < 0) ? nullptr : nodes_[nd.node_idx].obj;
    };

    void ObjectPool::push(CountPtr nd) {
        ++nd.external_cnt;

        CountPtr old_tail = tail_entry_.load(std::memory_order_relaxed);
        nodes_[old_tail.node_idx].qnext = nd;

        while(!tail_entry_.compare_exchange_weak(nodes_[nd.node_idx].qnext, nd, std::memory_order_release, std::memory_order_relaxed));
        q_size_.fetch_add(1);
    };

    CountPtr ObjectPool::pop() {
        CountPtr old_head = head_entry_.load(std::memory_order_relaxed);
        while(true) {
            if (head_entry_.compare_exchange_weak(old_head, nodes_[old_head.node_idx].qnext/*node->qnext*/, std::memory_order_release, std::memory_order_relaxed)){
                q_size_.fetch_sub(1);
                return nodes_[old_head.node_idx].qnext;
            }
        }
    };

    const int32_t ObjectPool::queue_size() const {
        return q_size_.load(std::memory_order_acquire);
    };

    const int32_t ObjectPool::size() const {
        return (nodes_.size() -1) - top_entry_.load(std::memory_order_relaxed).node_idx;;
    };


    const size_t ObjectPool::blk_size() const {
        return blkSz_ - HEAD_SIZE; //20000736 20000160
    };


    CountPtr ObjectPool::IncreaseEntryCnt (CountPtr &old_cnt) {
        CountPtr new_cnt;
        do {
            new_cnt = old_cnt;
            ++new_cnt.external_cnt;
        } while (!top_entry_.compare_exchange_strong( old_cnt, new_cnt, std::memory_order_acquire, std::memory_order_relaxed));
        return new_cnt;
    };



    CAnalog::CAnalog(std::byte *data, const uint32_t size) : data_(new std::byte[size]), size_(size) {
        std::memcpy(data_, data, size);
    };

    CAnalog::~CAnalog() {
        delete[] data_;
    };


    BufSegment::BufSegment(const uint32_t size) noexcept:
        size_{size}
    {};

    const uint32_t BufSegment::capacity() const {
        return (size_ - wrPos_);
    };


    const uint32_t BufSegment::write_data(std::byte* data, uint32_t inSize) {
        if( (wrPos_ + inSize) > size_){
            std::cout << "wrPos: " << (int)wrPos_ << " inSz: " << (int)inSize << " sz: " << (int)size_ << std::endl;
            throw std::overflow_error("Segment buffer overflow");
        }

        std::memcpy(&ptr_[wrPos_], data, inSize);
        wrPos_+= inSize;
        return size_ - wrPos_;
    };

    void BufSegment::set(const uint8_t chIdx, const uint32_t smpsSeq) {
        status_ = false;
        chIdx_ = chIdx;
        smpsSeq_ = smpsSeq;
        wrPos_ = 0;
    };

    AChStat::AChStat(const uint8_t idx, const uint32_t sz) :
        chIdx_(idx),
        coBuff_(new std::byte[sz])
    {};

    AChStat::~AChStat() {
        delete[] coBuff_;
    };

    const uint8_t AChStat::id() const {
        return chIdx_;
    };

    std::vector<CAnalog*> AChStat::data() {
        return coSamples_;
    };


    SrpSamplesSegmented::SrpSamplesSegmented(const std::string stor_id, const size_t blkLim, std::vector<uint8_t> analog) :
        stor_id_(stor_id),
        blkNum_(analog.size() * BUF_FAC),
        blkSz_(blkLim),
        segPool_(std::make_unique<ObjectPool>(analog.size() * BUF_FAC, blkLim))
    {
        for (const auto chIdx : analog) {
            std::shared_ptr<AChStat> stat {new AChStat{chIdx, blkSz_}};
            stat->cPtr = segPool_->GetOne();

            BufSegment *blk = segPool_->GetObj(stat->cPtr);
            blk->set(chIdx, stat->inSmps_);

            a_ch_map_.emplace(chIdx, stat);
        }
    };

    SrpSamplesSegmented::~SrpSamplesSegmented() {
        if (data_th_.joinable())
            data_th_.join();

        std::cout << "Destroy Storage: " << stor_id_ << std::endl;
    };

    void SrpSamplesSegmented::feed_header() {
        std::cout << "HEADER packet" << std::endl;
        acqDone = false;
        data_th_ = std::thread(&SrpSamplesSegmented::data_proc_th, this);
        t_start_ = high_resolution_clock::now();
    };


    void SrpSamplesSegmented::feed_end() {
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
    };

    void SrpSamplesSegmented::append_logic(const sr_datafeed_logic *logic) {
        //std::cout << "L" << std::endl;
    };

    bool SrpSamplesSegmented::append_analog(const sr_datafeed_analog *analog) {
        auto const *ch = static_cast<const struct sr_channel *>(analog->meaning->channels->data);
        auto const ch_stat = a_ch_map_[ch->index];
        const uint32_t in_size = analog->num_samples * sizeof(float);
        BufSegment *blk = segPool_->GetObj(ch_stat->cPtr);

        if(blk->capacity() < in_size){
            blk->status_ = true;
            segPool_->push(ch_stat->cPtr);

            ch_stat->cPtr = segPool_->GetOne();
            blk = segPool_->GetObj(ch_stat->cPtr);
            blk->set(ch_stat->chIdx_, ch_stat->inSmps_);

            if(segPool_->size() > blkNum_ ) {
                std::cout << "BUFFER OVERFLOW: Mem pool exausted" << std::endl;
                std::terminate();
            }

            ready_.test_and_set(std::memory_order_release);
            ready_.notify_one();
        }
        blk->write_data((std::byte*)analog->data, in_size);
        ch_stat->inSmps_ += analog->num_samples;

        return true;
    };

    void SrpSamplesSegmented::data_proc_th() {
        std::cout << "START DATA THREAD" << std::endl;
        CountPtr cPtr;
        BufSegment *blk;

        ZSTD_CCtx *cctx = ZSTD_createCCtx();

        while(!acqDone){
            ready_.wait(acqDone, std::memory_order_acquire);
            ready_.clear(std::memory_order_release);

            while( segPool_->queue_size() >= 1){
                cPtr = segPool_->pop();
                blk = segPool_->GetObj(cPtr);
                auto ch_stat = a_ch_map_[blk->chIdx_];

                int64_t const cSize = ZSTD_compressCCtx(cctx, ch_stat->coBuff_, blkSz_, blk->ptr_, blk->wrPos_, 4);
                ch_stat->coSamples_.push_back(new CAnalog{ch_stat->coBuff_, cSize});
                ch_stat->coSamplesNum_++;


                std::cout << "CH: " << (int)blk->chIdx_ << " InSize Mb: " << (double)blk->wrPos_/1024/1024  << " Csize Mb: " << (double)cSize/1024/1024 << " seq: " << (int)blk->smpsSeq_ << std::endl;

                segPool_->Reclaim(cPtr);
            }
            //std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "**END DATA THREAD:" << std::endl
        << "    Blocks processed: " << n_done_ << std::endl;

        ZSTD_freeCCtx(cctx);
    };
}
