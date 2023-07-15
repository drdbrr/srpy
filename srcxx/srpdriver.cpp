#include "srpdriver.hpp"
#include "srpdevice.hpp"
#include "srpmanager.hpp"
#include "utils.hpp"

using std::list;
using std::vector;
using std::map;
using std::shared_ptr;
using std::string;
using std::default_delete;

namespace srp {
    SrpDriver::SrpDriver(struct sr_dev_driver *drv): 
        driver_(drv),
        initialized_(false)
    {}

    SrpDriver::~SrpDriver(){}

    string SrpDriver::name() const
    {
        return driver_->name;
    }

    string SrpDriver::longname() const
    {
        return driver_->longname;
    }

    list<string> SrpDriver::scan_options()
    {
        if (!initialized_) {
            srcheck(sr_driver_init(_parent->ctx_, driver_));
            initialized_ = true;
        }
        
        const struct sr_key_info *srci;
        list<string> result;
        GArray *opts = sr_driver_scan_options_list(driver_);
        
        if (opts) {
            for (int i = 0; i < opts->len; i++){
                srci = sr_key_info_get(SR_KEY_CONFIG, g_array_index(opts, uint32_t, i));
                result.push_back(srci->id);
            }
            g_array_free(opts, TRUE);
        }
        return result;
    }

    vector<shared_ptr<SrpDevice>> SrpDriver::scan(map<string, string> opts)
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
                string key = srci->id;
                string value = opts[srci->id];
                struct sr_config *cnf = parse_scan_opts(key, value);
                option_list = g_slist_append(option_list, cnf);
            }
        }
        
        GSList *device_list = sr_driver_scan(driver_, option_list);
        g_slist_free_full(option_list, g_free);
        
        vector<shared_ptr<SrpDevice>> result;
        for (GSList *device = device_list; device; device = device->next) {
            auto *const sdi = static_cast<struct sr_dev_inst *>(device->data);
            shared_ptr<SrpDevice> hwdev {
                new SrpDevice{shared_from_this(), sdi},
                default_delete<SrpDevice>{}};
                
            result.push_back(move(hwdev));
            //result.push_back(hwdev);
        }
        
        g_slist_free(device_list);
        return result;
    }
}
