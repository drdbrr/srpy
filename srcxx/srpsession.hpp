#ifndef SRP_SESSION_HPP
#define SRP_SESSION_HPP

#include "srpsamples_segmented.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <map>
#include <vector>
#include <string>

namespace srp {
    class SrpManager;
    class SrpDevice;
    using SrpSamples = SrpSamplesSegmented;

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

        std::map<std::string, std::shared_ptr<SrpSamples>> samples_stor();


        std::vector<std::shared_ptr<SrpDevice>> getScan();
        void setScan(std::vector<std::shared_ptr<SrpDevice>> devs);

        const std::string id() const;
        const std::string name() const;
        const std::string type() const;
        //const std::string sourcename();


        /*
        void set_loop(py::object &coro, py::object &coro_stop, py::object &loop);
        const py::function run_coro_ts;

        py::object coro_;
        py::object coro_stop_;
        py::object loop_;
        */



    private:
        const std::string ses_id_;
        static int nameCnt_;
        const std::string name_;

        struct sr_session *session_;
        struct sr_context *ctx_;
        std::shared_ptr<SrpDevice> device_;
        
        std::atomic<Capture> capture_state_;
        void set_capture_state(Capture state);

        //TODO
        std::map<std::string, std::shared_ptr<SrpSamples>> storage_;
        std::string cur_storid_;
        
        std::thread sampl_th_;
        void sampl_proc_th();

        bool out_of_memory_;

        std::vector<std::shared_ptr<SrpDevice> > scanned_;
        
        friend struct std::default_delete<SrpSession>;
    };
}
#endif
