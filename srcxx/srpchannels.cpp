#include "srpchannels.hpp"
#include "srpdevice.hpp"
#include "srpconfig.hpp"
#include "utils.hpp"

namespace srp {
    SrpChGroup::SrpChGroup(SrpDevice *srp_device, struct sr_channel_group *cg):
        ch_group_(cg)
    {
        struct sr_dev_driver *drv = sr_dev_inst_driver_get(srp_device->sdi_);
        
        GArray *opts = sr_dev_options(drv, srp_device->sdi_, cg);
        if (opts) {
            for (guint i = 0; i < opts->len; i++){
                uint32_t key = g_array_index(opts, uint32_t, i);
                std::shared_ptr<SrpConfig> cg_conf {new SrpConfig{key, drv, srp_device->sdi_, cg}};
                cg_confs_.emplace(cg_conf->id(), std::move(cg_conf));
            }
        }
    }

    /*
    SrpChGroup::~SrpChGroup()
    {}
    */

    std::string SrpChGroup::name() {
        return ch_group_->name;
    };

    std::map<std::string, std::shared_ptr<SrpConfig> > SrpChGroup::config() {
        /*
        map<string, shared_ptr<SrpConfig>> result;
        
        for (auto const& [name, cgconf]: cg_confs_){
            result.emplace(name, cgconf->share_owned_by(parent()));
        }
        */
        return cg_confs_;//result;
    };

    uint16_t SrpChGroup::size() {
        return g_slist_length(ch_group_->channels);
    }


    //--------- SrpChannel ---------//
    SrpChannel::SrpChannel(struct sr_channel *ch):
        ch_(ch)
    {};

    SrpChannel::~SrpChannel()
    {};

    std::string SrpChannel::name() const {
        return ch_->name;
    };

    int SrpChannel::type() const {
        return ch_->type;
    };

    const bool SrpChannel::enabled() const {
        return ch_->enabled;
    };

    void SrpChannel::set_enabled(const bool value) {
        srcheck(sr_dev_channel_enable(ch_, value));
    };

    unsigned int SrpChannel::index() const {
        return ch_->index;
    };
}
