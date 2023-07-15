#include <libsigrokcxx/libsigrokcxx.hpp>
#include <libsigrok/libsigrok.h>
#include "srpcxx.hpp"
#include <variant>
#include <map>
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace srp {
    class SrpConfig: public sigrok::ParentOwned<SrpConfig, SrpDevice>
    {
    public:
        explicit SrpConfig(uint32_t key, struct sr_dev_driver *drv, struct sr_dev_inst *sdi, struct sr_channel_group *cg);
        ~SrpConfig();
        std::string id();
        uint32_t key();

        std::variant<std::string, uint32_t, uint64_t> value();

        py::handle get_value();
        void set_value(py::handle &pyval);
        py::handle list();
        std::map<int, std::string> caps();
        
    private:
        const struct sr_key_info *info_;
        struct sr_dev_driver *conf_drv_;
        struct sr_dev_inst *conf_sdi_;
        struct sr_channel_group *conf_cg_;
        
        friend class SrpDevice;
        friend class SrpChGroup;
        friend struct std::default_delete<SrpConfig>;
    }; 
}
