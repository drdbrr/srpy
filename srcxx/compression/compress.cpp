#include "compress.hpp"


//https://github.com/facebook/zstd/blob/01dbbdf560bcd674a72c7786539a8821b24aaafa/lib/compress/zstdmt_compress.c#L1417C9-L1417C20
//https://github.com/facebook/zstd/blob/01dbbdf560bcd674a72c7786539a8821b24aaafa/contrib/pzstd/Pzstd.cpp#L406
//https://github.com/mcmilk/zstdmt/blob/56799acb7be56d93832d58af1d281e95714b004b/lib/zstd-mt_compress.c#L322
namespace compr {
    Compress::Compress(uint8_t nth, const size_t osz):
        blkOsz_(osz),
        obuf_(new uint8_t[osz])
    {
        //blosc2_init();
        //blosc2_set_nthreads(nth);
        //blosc1_set_compressor("zlib");
        std::cout << "Blosc2 compressor constructed: " << blosc2_get_version_string() << std::endl;
    };
    Compress::~Compress() {
        delete[] obuf_;
        blosc2_destroy();
    };


    //blosc2_compress(int clevel, int doshuffle, int32_t typesize, const void* src, int32_t srcsize, void* dest, int32_t destsize);

    void Compress::compress_data(void* inData, const size_t inLen) {
        //int csize = blosc2_compress(5, BLOSC_BITSHUFFLE, 1, inData, (int32_t)inLen, obuf_, (int32_t)osz_);
        size_t csize = blosc1_compress(1, 1, 1, inLen, inData, obuf_, blkOsz_);
        std::cout << "isz: " << (int)inLen << " osz: " << (int)csize << std::endl;
    };
}

