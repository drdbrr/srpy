#include <libsigrokcxx/libsigrokcxx.hpp>
#include <libsigrok/libsigrok.h>
#include <thread>
#include <atomic>
//#include <functional>
#include <map>
#include "srpcxx.hpp"

namespace srp {
    class SrpSession : public sigrok::ParentOwned<SrpSession, SrpManager>
    {
    public:
        enum Capture {
            Stopped,
            AwaitingTrigger,
            Running
        };
        SrpSession(struct sr_context *ctx, std::string ses_id);
        ~SrpSession();

        std::shared_ptr<SrpDevice> device();
        //std::shared_ptr<SrpDevice> get_device(const struct sr_dev_inst *sdi);
        void add_device(std::shared_ptr<SrpDevice> device);
        void reset_device();
        void start_capture();
        void stop_capture();
        Capture get_capture_state() const;

        void new_storage();
        void remove_storage(std::string stor_id); //ATTENTION
        void remove_storage();
        std::shared_ptr<SrpSamples> get_storage(std::string stor_id);
        std::shared_ptr<SrpSamples> current_storage();

    private:
        const std::string ses_id_;
        struct sr_session *session_;
        struct sr_context *ctx_;
        std::shared_ptr<SrpDevice> device_;

        
        std::atomic<Capture> capture_state_;
        void set_capture_state(Capture state);

        //TODO
        std::map<std::string, std::shared_ptr<SrpSamples> > storage_;
        std::string cur_storid_;
        
        std::thread sampl_th_;
        void sampl_proc_th();

        //mutable std::mutex sampling_mutex_;
        
        bool out_of_memory_;
        
        friend class SrpSamples;
        friend class SrpManager;
        friend class SrpDevice;
        friend struct std::default_delete<SrpSession>;
    };
};
