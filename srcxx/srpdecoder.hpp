#ifndef SRP_DECODER_HPP
#define SRP_DECODER_HPP

#include <libsigrokdecode/libsigrokdecode.h>
#include <glib.h>
#include "srpcxx.hpp"
#include <string>
#include <map>

namespace srp {
    struct SrpDecoderItem {
        std::string id;
        std::string name;
        std::string longname;
        std::string tags;
        //std::vector<std::string> tags;
    };


    class SrpDecoder {
    public:
        SrpDecoder(const srd_decoder *const dec, uint8_t stack_level);
        ~SrpDecoder();

        const std::string name() const;
        bool visible() const;
        void set_visible(bool visible);

        const std::map<std::string, GVariant*>& options() const;
        void set_option(const char *id, GVariant *value);

        srd_decoder_inst* create_decoder_inst(srd_session *session);
        void invalidate_decoder_inst();

    private:
        const srd_decoder* const srd_decoder_;
        uint8_t stack_level_;
        bool visible_;

        /*
        vector<DecodeChannel*> channels_;
        deque<Row> rows_;
        deque<AnnotationClass> ann_classes_;
        vector<DecodeBinaryClassInfo> bin_classes_;
        */

        std::map<std::string, GVariant*> options_;
        srd_decoder_inst *decoder_inst_;
    };
};
#endif
