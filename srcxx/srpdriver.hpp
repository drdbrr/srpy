#ifndef SRP_DRIVER_HPP
#define SRP_DRIVER_HPP

#include <vector>
#include <memory>
#include <map>
#include <string>
#include <list>

#include "srpmanager.hpp"
#include "srpdevice.hpp"

namespace srp {
    class SrpManager;

    class SrpDriver: public sigrok::ParentOwned<SrpDriver, SrpManager> {
    public:
        explicit SrpDriver(struct sr_dev_driver *drv);
        ~SrpDriver();
        std::string name() const;
        std::string longname() const;
        std::list<std::string> scan_options();

        std::map<std::string, std::shared_ptr<SrpConfig> > config();
        std::vector<std::shared_ptr<SrpDevice> > scan(std::map<std::string, std::string> opts);
        
    private:
        struct sr_dev_driver *driver_;
        bool initialized_;

        std::map<std::string, std::shared_ptr<SrpConfig> > confs_;
        
        friend class SrpManager;
        friend struct std::default_delete<SrpDriver>;
    };
}
#endif
