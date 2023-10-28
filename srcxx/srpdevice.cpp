#include "utils.hpp"
#include "srpdevice.hpp"
#include "srpchannels.hpp"
#include <unistd.h>
#include <iostream>

static const uint32_t GET_BUF_SIZE_ALLIGNED(const uint64_t samplerate) {
    const uint16_t T_LIMIT = 200;
    const uint64_t limit = samplerate / (1000 / T_LIMIT) * sizeof(float);// + sizeof(srp::BufSegment);
    const uint32_t pgsz = sysconf(_SC_PAGESIZE);
    const uint32_t size = (((limit + pgsz) & ~(pgsz - 1)));
    return size;
}

namespace srp {
    SrpDevice::SrpDevice(std::shared_ptr<SrpDriver> driver, struct sr_dev_inst *sdi):
        sdi_(sdi),
        srp_driver_(std::move(driver)),
        dev_id_(mcuuid()),
        device_open_(false)
    {
        //INIT DEV OPTIONS
        struct sr_dev_driver *drv = sr_dev_inst_driver_get(sdi_);
        GArray *opts = sr_dev_options(drv, sdi_, nullptr);
        if (opts) {
            for (guint i = 0; i < opts->len; i++){
                uint32_t key = g_array_index(opts, uint32_t, i);
                std::shared_ptr<SrpConfig> dev_conf {new SrpConfig{key, drv, sdi, nullptr}};
                confs_.emplace(dev_conf->id(), std::move(dev_conf));
            }
            g_array_free(opts, true);
        }
        
        //INIT CHANNELS
        for (GSList *entry = sr_dev_inst_channels_get(sdi_); entry; entry = entry->next) {
            auto *const ch = static_cast<struct sr_channel *>(entry->data);
            std::unique_ptr<SrpChannel> channel {new SrpChannel{ch}};
            channels_.emplace(ch, std::move(channel));
        }
        
        //INIT CH GROUPS
        for (GSList *entry = sr_dev_inst_channel_groups_get(sdi_); entry; entry = entry->next) {
            auto *const cg = static_cast<struct sr_channel_group *>(entry->data);
            std::unique_ptr<SrpChGroup> group {new SrpChGroup{this, cg}};
            ch_groups_.emplace(cg->name, std::move(group));
        }
        
        std::cout << "Dev constructed" << std::endl;
    };

    SrpDevice::~SrpDevice() {
        close();
        std::cout << "Dev destructed " << std::endl;
    };

    void SrpDevice::open() {
        if (device_open_)
            close();
        try {
            srcheck(sr_dev_open(sdi_));
        } catch (const sigrok::Error &e) {
            std::cout << e.what() << std::endl;
        }
        
        
        /*
        if ( ch_groups_.contains("Logic") )
            auto &lcg = ch_groups_["Logic"];
        
        if ( ch_groups_.contains("Analog") )
            auto &acg = ch_groups_["Analog"];
        */
        
        device_open_ = true;
    };

    void SrpDevice::close() {
        if (device_open_)
            srcheck(sr_dev_close(sdi_));
        
        device_open_ = false;
    };


    std::map<std::string, std::shared_ptr<SrpChGroup>> SrpDevice::ch_groups() {
        std::map<std::string, std::shared_ptr<SrpChGroup>> result;
        
        for (auto const& [name, group]: ch_groups_)
            result.emplace(name, group->share_owned_by(get_shared_from_this()));
        
        return result;
    };

    std::shared_ptr<SrpChannel> SrpDevice::get_channel(std::string const &name) {
        auto const& chnls = channels()[name];
        return std::move(chnls);
    };


    std::map<std::string, std::shared_ptr<SrpChannel>> SrpDevice::channels() {
        std::map<std::string, std::shared_ptr<SrpChannel>> result;
        //for (auto channel = sr_dev_inst_channels_get(sdi_); channel; channel = channel->next) {
            //auto *const ch = static_cast<struct sr_channel *>(channel->data);
            //result.insert(channels_[ch]->share_owned_by(get_shared_from_this()));
        //}
        
        for (auto const& [ch, channel] : channels_)
            result.emplace(ch->name, channel->share_owned_by(get_shared_from_this()));
        
        return result;
    };

    std::map<std::string, std::shared_ptr<SrpConfig>> SrpDevice::config() {
        return confs_;
    };

    std::map< std::string, std::string > SrpDevice::info() {
        std::map< std::string, std::string > info;
        info.emplace("vendor", chk_str(sr_dev_inst_vendor_get(sdi_)));
        info.emplace("model", chk_str(sr_dev_inst_model_get(sdi_)));
        info.emplace("version", chk_str(sr_dev_inst_version_get(sdi_)));
        info.emplace("sernm", chk_str(sr_dev_inst_sernum_get(sdi_)));
        info.emplace("connid", chk_str(sr_dev_inst_connid_get(sdi_)));
        return info;
    };

    std::shared_ptr<SrpDriver> SrpDevice::driver() {
        return srp_driver_;
    };


    std::shared_ptr<SrpDevice> SrpDevice::get_shared_from_this() {
        //return static_pointer_cast<SrpDevice>(shared_from_this());
        return shared_from_this();
    };


    const uint64_t SrpDevice::get_buf_size() {
        uint32_t bufsize;

        if ( confs_.contains("buffersize") ) { //( sr_dev_has_option(sdi_, SR_CONF_BUFFERSIZE) ){
            auto& bufsize_cfg = confs_["buffersize"];
            //shared_ptr<SrpConfig> bufsize_cfg = config()["buffersize"];
            auto sz = bufsize_cfg->value();

            bufsize = std::get<uint64_t>(bufsize_cfg->value());
            //std::cout << "-----> TYPE: " << (int)bufssize << std::endl;
        }
        else {
            auto& smplrate_cfg = confs_["samplerate"];
            const uint64_t smplrate = std::get<uint64_t>(smplrate_cfg->value());
            bufsize  = GET_BUF_SIZE_ALLIGNED(smplrate);
        }
        return bufsize;
    };
}
