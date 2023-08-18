#include "srpdecoder.hpp"

namespace srp {
    SrpDecoder::SrpDecoder(const srd_decoder *const dec, uint8_t stack_level) :
        srd_decoder_(dec),
        stack_level_(stack_level),
        visible_(true),
        decoder_inst_(nullptr)
    {

    }
    SrpDecoder::~SrpDecoder(){

    }

    const std::string SrpDecoder::name() const{
        return srd_decoder_->name;
    }

    bool SrpDecoder::visible() const{
        return visible_;
    }

    void SrpDecoder::set_visible(bool visible){
        visible_ = visible;
    }

    const std::map<std::string, GVariant*>& SrpDecoder::options() const{
        return options_;
    }

    void SrpDecoder::set_option(const char *id, GVariant *value){
        options_[id] = value;
    }

    srd_decoder_inst* SrpDecoder::create_decoder_inst(srd_session *session){

        //GHashTable *opt_hash;

        GHashTable *const opt_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

        decoder_inst_ = srd_inst_new(session, srd_decoder_->id, opt_hash);
        return decoder_inst_;
    }

    void SrpDecoder::invalidate_decoder_inst(){
        decoder_inst_ = nullptr;
    }
}
