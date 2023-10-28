#ifndef SRP_CONFIG_HPP
#define SRP_CONFIG_HPP

#include <libsigrokcxx/libsigrokcxx.hpp>

#include <variant>
#include <memory>
#include <map>

#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace srp {
    class SrpConfig: public sigrok::UserOwned<SrpConfig> {
    public:
        explicit SrpConfig(uint32_t key, struct sr_dev_driver *drv, struct sr_dev_inst *sdi, struct sr_channel_group *cg);
        ~SrpConfig();

        const std::string id() const;
        const uint32_t key() const;
        const std::string name() const;

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
        
        friend struct std::default_delete<SrpConfig>;
    };
}

#endif
