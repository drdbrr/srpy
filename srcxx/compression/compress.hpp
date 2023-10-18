#include <blosc2.h>

#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <cstring>
#include <iostream>

//https://github.com/oneapi-src/oneTBB/tree/master
//https://computing.llnl.gov/projects/floating-point-compression/related-projects
//https://github.com/cselab/aphros



//https://github.com/facebook/zstd/blob/dev/examples/streaming_memory_usage.c
//https://github.com/facebook/zstd/blob/dev/examples/streaming_compression_thread_pool.c
//https://github.com/facebook/zstd/blob/dev/examples/simple_compression.c
//https://github.com/facebook/zstd/blob/dev/examples/multiple_streaming_compression.c
//https://github.com/dascandy/decoco/blob/master/src/zstd.cpp
//https://github.com/dascandy/decoco/blob/master/src/zlib.cpp
//https://github.com/zlib-ng/minizip-ng
//https://github.com/google/snappy
//https://github.com/zlib-ng/zlib-ng
//https://protobuf.dev/reference/cpp/api-docs/google.protobuf.io.gzip_stream/#GzipOutputStream
//https://eax.me/zlib/
//https://github.com/ondesly/streams/blob/master/src/streams/zsstream.cpp
//https://github.com/cloudflare/zlib
//https://github.com/alibaba/async_simple/blob/ab8cb03d755035a6e348e6a3f9cea8e4781b4a6f/demo_example/io_context_pool.hpp#L38C9-L38C59


//https://vorbrodt.blog/2019/03/22/extremely-fast-compression-algorithm/
//https://github.com/apache/arrow/blob/44811ba18477560711d512939535c8389dd7787b/cpp/src/arrow/io/compressed.cc#L37
//https://github.com/apache/arrow/blob/44811ba18477560711d512939535c8389dd7787b/cpp/src/arrow/io/buffered.cc#L201
//https://github.com/xtensor-stack/xsimd
//https://github.com/mvorbrodt/blog/blob/master/src/compression.cpp
//https://github.com/lz4/lz4/tree/dev
//https://github.com/lz4/lz4/blob/dev/examples/blockStreaming_doubleBuffer.c
//https://github.com/mcmilk/7-Zip-zstd
//https://github.com/LLNL/zfp/tree/develop
//https://aras-p.info/blog/2023/02/03/Float-Compression-5-Science/

namespace compr{
    class Compress {
    public:
        Compress(uint8_t nth, const size_t osz);
        ~Compress();
        void compress_data(void* inData, const size_t inLen);
    private:
        blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
        const size_t blkOsz_;
        //blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
        //blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
        //blosc2_storage b2_storage = {.cparams=&cparams, .dparams=&dparams};

        blosc2_context *cctx, *dctx;
    };
}

