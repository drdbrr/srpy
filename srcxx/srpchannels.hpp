#ifndef SRP_CHANNELS_HPP
#define SRP_CHANNELS_HPP

#include <libsigrokcxx/libsigrokcxx.hpp>

#include <map>
#include <string>
#include <memory>

namespace srp {
    class SrpDevice;
    class SrpConfig;

    class SrpChGroup: public sigrok::ParentOwned<SrpChGroup, SrpDevice> {
    public:
        SrpChGroup(SrpDevice *srp_device, struct sr_channel_group *cg);
        ~SrpChGroup() {};
        std::string name();
        std::uint16_t size();
        std::map<std::string, std::shared_ptr<SrpConfig>> config();
        

    private:
        struct sr_channel_group *ch_group_;
        std::map<std::string, std::shared_ptr<SrpConfig> > cg_confs_;
        
        friend class SrpDevice;
        friend struct std::default_delete<SrpChGroup>;
    };

    class SrpChannel: public sigrok::ParentOwned<SrpChannel, SrpDevice> {
    public:
        std::string name() const;
        int type() const;
        const bool enabled() const;
        void set_enabled(const bool value);
        unsigned int index() const;
        
    private:
        explicit SrpChannel(struct sr_channel *ch);
        ~SrpChannel();
        struct sr_channel *ch_;
        friend class SrpDevice;
        friend struct std::default_delete<SrpChannel>;
    };
};
#endif
