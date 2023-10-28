//#include <libsigrok/libsigrok.h>
//#include <libsigrokcxx/libsigrokcxx.hpp>

#include "srpdriver.hpp"
#include "utils.hpp"

// using std::list;
// using std::vector;
// using std::map;
// using std::shared_ptr;
// using std::string;

namespace srp {
    SrpDriver::SrpDriver(struct sr_dev_driver *drv): 
        driver_(drv),
        initialized_(false)
    {};

    SrpDriver::~SrpDriver(){};

    std::string SrpDriver::name() const {
        return driver_->name;
    };

    std::string SrpDriver::longname() const {
        return driver_->longname;
    };

    std::map<std::string, std::shared_ptr<SrpConfig>> SrpDriver::config() {
        if (!initialized_) {
            srcheck(sr_driver_init(_parent->ctx_, driver_));
            initialized_ = true;
        }

        GArray *opts = sr_dev_options(driver_, nullptr, nullptr);
        if (opts) {
            for (guint i = 0; i < opts->len; i++){
                uint32_t key = g_array_index(opts, uint32_t, i);
                std::shared_ptr<SrpConfig> dev_conf {new SrpConfig{key, driver_, nullptr, nullptr}};
                confs_.emplace(dev_conf->name(), std::move(dev_conf));      //<------ ATTENTION: TODO select Config dev_conf->[field] to be a key of map.
            }
            g_array_free(opts, true);
        }

        return confs_;//result;
    };

    std::list<std::string> SrpDriver::scan_options()
    {
        if (!initialized_) {
            srcheck(sr_driver_init(_parent->ctx_, driver_));
            initialized_ = true;
        }
        
        const struct sr_key_info *srci;
        std::list<std::string> result;
        GArray *opts = sr_driver_scan_options_list(driver_);
        
        if (opts) {
            for (int i = 0; i < opts->len; i++){
                srci = sr_key_info_get(SR_KEY_CONFIG, g_array_index(opts, uint32_t, i));
                result.push_back(srci->id);
            }
            g_array_free(opts, TRUE);
        }
        return result;
    };

    std::vector<std::shared_ptr<SrpDevice>> SrpDriver::scan(std::map<std::string, std::string> opts)
    {
        if (!initialized_) {
            srcheck(sr_driver_init(_parent->ctx_, driver_));
            initialized_ = true;
        }
        
        GSList *option_list = nullptr;
        GArray *scan_opts = sr_driver_scan_options_list(driver_);
        
        const struct sr_key_info *srci;
        
        for (int i = 0; i < scan_opts->len; i++){
            srci = sr_key_info_get(SR_KEY_CONFIG, g_array_index(scan_opts, uint32_t, i));
            if ( opts.count(srci->id) ){
                std::string key = srci->id;
                std::string value = opts[srci->id];
                struct sr_config *cnf = parse_scan_opts(key, value);
                option_list = g_slist_append(option_list, cnf);
            }
        }
        
        GSList *device_list = sr_driver_scan(driver_, option_list);
        g_slist_free_full(option_list, g_free);
        
        std::vector<std::shared_ptr<SrpDevice>> result;
        for (GSList *device = device_list; device; device = device->next) {
            auto *const sdi = static_cast<struct sr_dev_inst *>(device->data);
            std::shared_ptr<SrpDevice> hwdev {
                new SrpDevice{shared_from_this(), sdi},
                std::default_delete<SrpDevice>{}};
                
            result.push_back(move(hwdev));
            //result.push_back(hwdev);
        }
        
        g_slist_free(device_list);
        return result;
    };
}
