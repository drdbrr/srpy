#include "srpmanager.hpp"
#include "srpdriver.hpp"
#include "utils.hpp"
#include <iostream>


namespace srp {
    SrpManager::SrpManager():
        ctx_(nullptr)
    {
        std::cout << "SrpManager constructed" << std::endl;

        srcheck(sr_init(&ctx_));
        if ( sr_dev_driver **driver_list = sr_driver_list(ctx_)){
            for (int i = 0; driver_list[i]; i++) {
                std::unique_ptr<SrpDriver> driver {new SrpDriver{driver_list[i]}};
                drivers_.emplace(driver->name(), std::move(driver));
            }
        }
    };

    SrpManager::~SrpManager() {
        srcheck(sr_exit(ctx_));
        std::cout << "SrpManager destructed" << std::endl;
    };

    void SrpManager::loglevel_set(int lvl) {
        sr_log_loglevel_set(lvl);
    };

    std::shared_ptr<SrpSession> SrpManager::add_session(std::string ses_id) {
        std::shared_ptr<SrpSession> session {new SrpSession{ctx_, ses_id}};
        sessions_.emplace(ses_id, session);
        return session;
    };

    std::shared_ptr<SrpSession> SrpManager::add_session() {
        std::string ses_id = mcuuid();
        return add_session(ses_id);
    };

    void SrpManager::remove_session(std::string ses_id) {
        sessions_.erase(ses_id);
    };

    void SrpManager::remove_all() {
        sessions_.clear();
    };

    std::map<std::string, std::shared_ptr<SrpSession> > SrpManager::sessions() {
        return sessions_;
    };

    std::map<std::string, std::shared_ptr<SrpDriver> > SrpManager::drivers() {
        std::map<std::string, std::shared_ptr<SrpDriver>> result;
        for (const auto &entry: drivers_) {
            const auto &name = entry.first;
            const auto &driver = entry.second;
            result.emplace(name, driver->share_owned_by(shared_from_this()));
        }
        
        return result;
    };
}
