#include "srpmanager.hpp"
#include "srpsession.hpp"
#include "srpdriver.hpp"
#include "utils.hpp"
#include <iostream>

using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::map;

namespace srp {
    SrpManager::SrpManager():
        ctx_(nullptr)
    {
        std::cout << "SrpManager constructed" << std::endl;

        srcheck(sr_init(&ctx_));
        if ( sr_dev_driver **driver_list = sr_driver_list(ctx_)){
            for (int i = 0; driver_list[i]; i++) {
                unique_ptr<SrpDriver> driver {new SrpDriver{driver_list[i]}};
                drivers_.emplace(driver->name(), move(driver));
            }
        }
    }

    SrpManager::~SrpManager()
    {
        srcheck(sr_exit(ctx_));
        std::cout << "SrpManager destructed" << std::endl;
    }

    shared_ptr<SrpSession> SrpManager::add_session(string ses_id)
    {
        shared_ptr<SrpSession> session {new SrpSession{ctx_, ses_id}};
        sessions_.emplace(ses_id, session);
        return session;
    }

    shared_ptr<SrpSession> SrpManager::add_session()
    {
        string ses_id = mcuuid();
        return add_session(ses_id);
    }

    void SrpManager::remove_session(string ses_id)
    {
        sessions_.erase(ses_id);
    }

    void SrpManager::remove_all()
    {
        sessions_.clear();
    }

    map<string, shared_ptr<SrpSession> > SrpManager::sessions()
    {
        return sessions_;
    }

    map<string, shared_ptr<SrpDriver> > SrpManager::drivers(){
        map<string, shared_ptr<SrpDriver>> result;
        for (const auto &entry: drivers_) {
            const auto &name = entry.first;
            const auto &driver = entry.second;
            result.emplace(name, driver->share_owned_by(shared_from_this()));
        }
        
        return result;
    }
}
