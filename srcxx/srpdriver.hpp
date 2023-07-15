#include <libsigrokcxx/libsigrokcxx.hpp>
#include <libsigrok/libsigrok.h>
#include "srpcxx.hpp"

namespace srp {    
    class SrpDriver: public sigrok::ParentOwned<SrpDriver, SrpManager>
    {
    public:
        explicit SrpDriver(struct sr_dev_driver *drv);
        ~SrpDriver();
        std::string name() const;
        std::string longname() const;
        std::list<std::string> scan_options();
        std::vector<std::shared_ptr<SrpDevice> > scan(std::map<std::string, std::string> opts);
        
    private:
        struct sr_dev_driver *driver_;
        bool initialized_;
        
        friend class SrpManager;
        friend class SrpDevice;
        friend struct std::default_delete<SrpDriver>;
    };
}
