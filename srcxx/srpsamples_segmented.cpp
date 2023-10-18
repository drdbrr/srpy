#include <atomic>
#include <libsigrok/libsigrok.h>
#include <libsigrok/proto.h>
#include "srpsamples_segmented.hpp"
#include "srpdevice.hpp"
#include "srpsession.hpp"
#include "srpchannels.hpp"
#include "srpconfig.hpp"
#include <cstdlib>

#include <execution>
#include <algorithm>
#include <memory>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>


using std::string;
using std::move;

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::time_point;

//#define BUF_FAC 2       //ATTENTION: Buffering factor
//#define T_LIMIT 100     //ATTENTION: Buffering time limit (100 = milliseconds, 0.1s)
//#define H_SZ sizeof(srp::BufSegment)


namespace srp {
    SrpSamplesSegmented::SrpSamplesSegmented(const string stor_id, const size_t blkLim, std::vector<uint8_t> analog) :
        stor_id_(stor_id),
        blkNum_(analog.size() * BUF_FAC),
        blkSz_(blkLim),
        segPool_(std::make_unique<SegmentPool>(analog.size() * BUF_FAC, blkLim))
    {
        std::cout << "BUF CHUNK SIZE: " << (int)blkLim << std::endl;
        for (const auto chIdx : analog) {
            std::shared_ptr<AChStat> stat {new AChStat{chIdx, blkSz_}};
            stat->cPtr = segPool_->GetOne();

            BufSegment *blk = segPool_->GetObj(stat->cPtr);
            blk->set(chIdx, stat->inSmps_);

            //blk->chIdx_ = chIdx;

            //stat->coutBuff = (uint8_t*)std::malloc(segPool_->blk_size());
            //stat->coutSz = segPool_->blk_size();//segPool_->blk_size();

            //stat->oBuff_ = new char[blkSz_];


            a_ch_map_.emplace(chIdx, stat);
        }
        //blkSz_ = segPool_->blk_size();
    }

    SrpSamplesSegmented::~SrpSamplesSegmented() {
        if (data_th_.joinable())
            data_th_.join();

        std::cout << "Destroy Storage: " << stor_id_ << std::endl;
    }

    void SrpSamplesSegmented::feed_header() {
        std::cout << "HEADER packet" << std::endl;
        acqDone = false;
        data_th_ = std::thread(&SrpSamplesSegmented::data_proc_th, this);
        t_start_ = high_resolution_clock::now();
    }


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
    }

    void SrpSamplesSegmented::append_logic(const sr_datafeed_logic *logic)
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
    //https://github.com/facebook/folly/blob/623b7343c42a56a05fc194155775ffcd9689eb84/folly/synchronization/detail/ThreadCachedLists.h#L126
    //https://github.com/facebook/folly/blob/main/folly/experimental/channels/detail/AtomicQueue.h
    //https://vk.com/@habr-kak-rabotat-s-atomarnymi-tipami-dannyh-v-c
    //https://man7.org/linux/man-pages/man1/perf-c2c.1.html
    //https://github.com/max0x7ba/atomic_queue/tree/master
    //https://github.com/cameron314/concurrentqueue/blob/master/concurrentqueue.h
    //https://github.com/cameron314/readerwriterqueue/blob/master/readerwritercircularbuffer.h
    //https://github.com/xtensor-stack/xsimd
    //https://towardsdatascience.com/introduction-to-weight-quantization-2494701b9c0c << ATTENTION


    //poll TICK = 0.2s
    //48MHz = 0.29s
    //30MHz = 0.286s
    //24MHz = 0.288s

    //16MHz = 0.408s

    //8MHz = 0.801s = 6291456 samples = 24mb
    //4MHz = 1.612s



    //libusb_fill_bulk_transfer(transfer, usb->devhdl, HANTEK_EP_IN, buf, data_amount, cb, (void *)sdi, 4000);
    //Timeout запроса пакета данных от стройства 4000ms = 4s
    //Работает для частоты <= 1MHz
    //1MHz = 4.03s  4001280 samples
    // ^^^
    //100KHz = 4.013s =  400384 samples

    bool SrpSamplesSegmented::append_analog(const sr_datafeed_analog *analog){
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
    }


    /*
    //https://sigrok.org/gitweb/?p=libsigrok.git;a=blob;f=src/hardware/hantek-6xxx/api.c;hb=c12ca361e724ed1e04c659420d74dd68efe345a9#l534
    bool SrpSamplesSegmented::append_analog(const sr_datafeed_analog *analog){
        auto const *ch = static_cast<const struct sr_channel *>(analog->meaning->channels->data);
        auto const ch_stat = a_ch_map_[ch->index];
        const uint32_t in_size = analog->num_samples * sizeof(float);
        BufSegment *blk = segPool_->GetObj(ch_stat->cPtr);

        uint32_t remain, copy_size;
        uint32_t in_smps = analog->num_samples;


        std::byte* rdptr = static_cast<std::byte*>(analog->data);

        while(in_smps){
            remain = blk->capacity()/4;
            if (remain) {
                copy_size = std::min(in_smps, remain);
                in_smps -= copy_size;
                blk->write_data(rdptr, copy_size * 4);
                rdptr += copy_size * 4;
                ch_stat->inSmps_ += copy_size;
                remain -= copy_size;
            }
            if (!remain) {
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

        }
        return true;
    }
    */

    //https://github.com/GPUOpen-Tools/compressonator
    //https://github.com/grenaud/libgab/blob/master/gzstream/gzstream.h
    //https://github.com/fanagislab/kmerfreq/blob/master/gzstream.cpp

    //https://github.com/zlib-ng/zlib-ng
    //https://github.com/zlib-ng/minizip-ng/tree/master/doc#using-streams
    //https://github.com/zlib-ng/minizip-ng/blob/master/test/test_stream_compress.cc


    //https://github.com/syoyo/tinyexr/blob/79e5df59a97c8aa29172b155897734cdb4ee7b81/tinyexr.h#L3449


    //https://www.blosc.org/ !!!!!!!!!!!!
    //https://github.com/Blosc/c-blosc2
    //https://github.com/LLNL/zfp/tree/develop

    // void SrpSamplesSegmented::compress_data(std::byte* data, std::size_t bsz) {
    //     auto zChunk = compr_->compress(data, bsz);
    // }


    //https://github.com/facebook/folly/blob/b8f65b19372207c60889836787b7734f0124bfa9/folly/compression/test/CompressionTest.cpp#L789



    /*
     Codecs(compcode):
        BLOSC_BLOSCLZ = 0,
        BLOSC_LZ4 = 1,
        BLOSC_LZ4HC = 2,
        BLOSC_ZLIB = 4,
        BLOSC_ZSTD = 5,

        BLOSC_CODEC_NDLZ = 32,

        https://github.com/Blosc/c-blosc2/blob/main/plugins/codecs/zfp/README.md?plain=1
        BLOSC_CODEC_ZFP_FIXED_ACCURACY = 33,
        BLOSC_CODEC_ZFP_FIXED_PRECISION = 34,
        BLOSC_CODEC_ZFP_FIXED_RATE = 35,

        BLOSC_CODEC_OPENHTJ2K = 36,


     Filters:
        BLOSC_NOSHUFFLE = 0,   //!< No shuffle (for compatibility with Blosc1).
        BLOSC_NOFILTER = 0,    //!< No filter.
        BLOSC_SHUFFLE = 1,     //!< Byte-wise shuffle.
        BLOSC_BITSHUFFLE = 2,  //!< Bit-wise shuffle.
        BLOSC_DELTA = 3,       //!< Delta filter.
        BLOSC_TRUNC_PREC = 4,  //!< Truncate mantissa precision; positive values in cparams.filters_meta will keep bits; negative values will reduce bits.

     Splitmode:
        BLOSC_ALWAYS_SPLIT = 1,
        BLOSC_NEVER_SPLIT = 2,
        BLOSC_AUTO_SPLIT = 3,
        BLOSC_FORWARD_COMPAT_SPLIT = 4,

        int csize = blosc2_compress_ctx(cctx_, blk->ptr_, (int32_t)blk->wrPos_, ptrr, (int32_t)ch_stat->coutSz);
        if(!csize){
            char* errmsg = print_error(csize);
            std::cout << "CSIZE: " << csize << std::endl;
        }


        https://github.com/Blosc/c-blosc2/blob/main/examples/zstd_dict.c
        https://github.com/Blosc/c-blosc2/tree/main/plugins/codecs/zfp
    */

    void SrpSamplesSegmented::data_proc_th() {
        std::cout << "START DATA THREAD" << std::endl;
        CountedNodePtr cPtr;
        BufSegment *blk;

        //std::unique_ptr<BloscCtx<BLOSC_CODEC_ZFP_FIXED_ACCURACY>> blosc_ctx(new BloscCtx<BLOSC_CODEC_ZFP_FIXED_ACCURACY>{7, a_ch_map_.size()});
        //std::unique_ptr<BloscCtx<BLOSC_ZSTD>> blosc_ctx(new BloscCtx<BLOSC_ZSTD>{5, a_ch_map_.size()});



        ZSTD_CCtx *cctx = ZSTD_createCCtx();

        while(!acqDone){
            ready_.wait(acqDone, std::memory_order_acquire);
            ready_.clear(std::memory_order_release);

            while( segPool_->queue_size() >= 1){
                cPtr = segPool_->pop();
                blk = segPool_->GetObj(cPtr);
                auto ch_stat = a_ch_map_[blk->chIdx_];

                //int32_t csize = 0;// = blosc2_compress_ctx(blosc_ctx->cctx_, blk->ptr_, blk->wrPos_, ch_stat->coBuff_, blkSz_);
                //ch_stat->coSamples_.push_back(new CAnalog{ch_stat->coBuff_, csize});


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
    }
}
