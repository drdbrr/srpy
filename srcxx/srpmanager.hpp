#include <libsigrokcxx/libsigrokcxx.hpp>
#include <libsigrok/libsigrok.h>
//#include <map>
//#include <memory>
//#include <string>
#include "srpcxx.hpp"

namespace srp {
    class SrpManager : public sigrok::UserOwned<SrpManager> //std::enable_shared_from_this<SrpManager> 
    {
    public:
        SrpManager();
        ~SrpManager();
        std::map<std::string, std::shared_ptr<SrpSession> > sessions();
        std::shared_ptr<SrpSession> add_session(std::string ses_id);
        std::shared_ptr<SrpSession> add_session();
        void remove_session(std::string name);
        void remove_all();
        std::map <std::string, std::shared_ptr<SrpDriver> > drivers();
        
    private:
        std::map<std::string, std::shared_ptr<SrpSession> > sessions_;
        std::map<std::string, std::unique_ptr<SrpDriver> > drivers_;
        struct sr_context *ctx_;
        
        friend class SrpDriver;
        friend class SrpSession;
        friend struct std::default_delete<SrpManager>;
    };
};
