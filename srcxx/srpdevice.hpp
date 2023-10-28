#ifndef SRP_DEVICE_HPP
#define SRP_DEVICE_HPP

//#include <libsigrokcxx/libsigrokcxx.hpp>
#include "srpconfig.hpp"
#include <map>
#include <memory>
#include <string>

namespace srp {
    class SrpDriver;
    class SrpChannel;
    class SrpChGroup;

    class SrpDevice : public sigrok::UserOwned<SrpDevice> {
    public:
        explicit SrpDevice(std::shared_ptr<SrpDriver> driver, struct sr_dev_inst *sdi);
        ~SrpDevice();
        std::map<std::string, std::string> info();
        std::shared_ptr<SrpDriver> driver();
        std::map<std::string, std::shared_ptr<SrpConfig> > config();
        std::map<std::string, std::shared_ptr<SrpChannel> > channels();
        std::shared_ptr<SrpChannel> get_channel(std::string const &name);
        std::map<std::string, std::shared_ptr<SrpChGroup> > ch_groups();

        const uint64_t get_buf_size();
        
    private:
        void close();
        void open();
        
        bool device_open_;
        
        std::string dev_id_;
        
        struct sr_dev_inst *sdi_;
        std::shared_ptr<SrpDevice> get_shared_from_this();
        std::shared_ptr<SrpDriver> srp_driver_;
        
        std::map<std::string, std::shared_ptr<SrpConfig> > confs_;
        std::map<struct sr_channel *, std::unique_ptr<SrpChannel> > channels_;
        std::map<std::string, std::unique_ptr<SrpChGroup> > ch_groups_;
        
        friend class SrpChGroup;
        friend class SrpSession;
        friend struct std::default_delete<SrpDevice>;
    };
}
#endif
