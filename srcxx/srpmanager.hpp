#ifndef SRP_MANAGER_HPP
#define SRP_MANAGER_HPP

#include "srpsession.hpp"
#include <string>
#include <memory>
#include <map>


namespace srp {
    class SrpDriver;

    class SrpManager : public std::enable_shared_from_this<SrpManager> {
    public:
        SrpManager();
        ~SrpManager();

        std::map<std::string, std::shared_ptr<SrpSession> > sessions();
        std::shared_ptr<SrpSession> add_session(std::string ses_id);
        std::shared_ptr<SrpSession> add_session();

        void remove_session(std::string name);
        void remove_all();
        std::map <std::string, std::shared_ptr<SrpDriver> > drivers();

        void loglevel_set(int lvl);
        
    private:
        std::map<std::string, std::shared_ptr<SrpSession> > sessions_;
        std::map<std::string, std::unique_ptr<SrpDriver> > drivers_;
        struct sr_context *ctx_;


        friend class SrpDriver;
        friend struct std::default_delete<SrpManager>;
    };
}
#endif
