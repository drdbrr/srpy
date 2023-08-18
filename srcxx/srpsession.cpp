#include "srpsession.hpp"
#include "srpsamples.hpp"
//#include "srpsamplesheap.hpp"
#include "srpdevice.hpp"
#include "srpchannels.hpp"
#include "srpconfig.hpp"
#include "utils.hpp"
#include <atomic>
#include <cstdint>
#include <iostream>
#include <pybind11/gil.h>
#include <vector>
#include <memory>
#include <algorithm>

using std::move;
using std::shared_ptr;
using std::unique_ptr;
using std::string;
using std::vector;
using std::thread;
using std::bad_alloc;

namespace srp {
    SrpSession::SrpSession(struct sr_context *ctx, string ses_id):
        ses_id_(ses_id),
        session_(nullptr),
        ctx_(move(ctx)),
        run_coro_ts{py::module_::import("asyncio").attr("run_coroutine_threadsafe")},
        capture_state_{Stopped}
    {
        srcheck(sr_session_new(ctx_, &session_));
        std::cout << "Session created" << std::endl;
    }

    SrpSession::~SrpSession()
    {
        stop_capture();
        srcheck(sr_session_destroy(session_));
        std::cout << "Session destroyed" << std::endl;
    }

    static void data_feed_in(const struct sr_dev_inst *sdi, const struct sr_datafeed_packet *packet, void *cb_data)
    {
        SrpSession *ses = static_cast<SrpSession *>(cb_data);
        std::shared_ptr<SrpSamples> stor = ses->current_storage();

        switch (packet->type) {
            case SR_DF_HEADER:
            {
                //py::gil_scoped_acquire gil{};
                //py::object exp_pi = ses->run_coro_ts(ses->coro_, ses->loop_);
                stor->feed_header();
                break;
            }
                
            case SR_DF_META:
                break;
                    
            case SR_DF_TRIGGER:
                break;
                
            case SR_DF_LOGIC:
                stor->append_logic( (sr_datafeed_logic*)packet->payload );
                break;

            case SR_DF_ANALOG:
                stor->append_analog( (sr_datafeed_analog*)packet->payload );
                break;

            case SR_DF_END:
            {
                //py::gil_scoped_acquire gil{};
                //py::object exp_pi = ses->run_coro_ts(ses->coro_stop_, ses->loop_);
                stor->feed_end();
                break;
            }
            default:
                break;
        }
    }

    /*
    void SrpSession::add_decoder(std::string dId){

    }
    */


    void SrpSession::add_device(shared_ptr<SrpDevice> device)
    {
        assert(device);
        stop_capture();
        device_ = move(device);
        //reset_device();
        
        //device_ = move(device); //ATTENTION ATTENTION ATTENTION
        
        try{
            device_->open();
            //srcheck(sr_session_dev_add(session_, device->sdi_));
            srcheck(sr_session_dev_add(session_, device_->sdi_));
        } catch (sigrok::Error &e) {
            device_.reset();
            std::cout << e.what() << std::endl;
        }

        srcheck(sr_session_datafeed_callback_add(session_, &data_feed_in, this));
    }

    void SrpSession::reset_device()
    {
        if (device_->device_open_)
            device_->close();
        
        device_.reset(); //Reset shared_ptr
        srcheck(sr_session_dev_remove_all(session_));
        srcheck(sr_session_datafeed_callback_remove_all(session_));
    }

    void SrpSession::sampl_proc_th()
    {
        try {
            srcheck(sr_session_start(session_));
        } catch (sigrok::Error& e) {
            std::cout << e.what() << std::endl;
            return;
        }
        set_capture_state(Running);

        try {
            srcheck(sr_session_run(session_));
        } catch (sigrok::Error& e) {
            std::cout << e.what() << std::endl;
            set_capture_state(Stopped);
            return;
        }

        set_capture_state(Stopped);
    }

    void SrpSession::start_capture()
    {

        if (!device_){
            //TODO: throw here
            return;
        }

        if (get_capture_state() != Stopped){
            //TODO: throw here
            return;
        }

        //ATTENTION: create new storage every new acquisition
        new_storage();

        sampl_th_ = std::thread(&SrpSession::sampl_proc_th, this);
    }

    void SrpSession::stop_capture()
    {
        if (get_capture_state() != Stopped)
            srcheck(sr_session_stop(session_));
        
        if (sampl_th_.joinable())
            sampl_th_.join();
    }

    SrpSession::Capture SrpSession::get_capture_state() const {
        return capture_state_.load(std::memory_order_relaxed);
    }

    void SrpSession::set_capture_state(Capture state){
        if (state == capture_state_.load(std::memory_order_relaxed))
            return;
        if (state == Stopped){
            std::cout << "Asquision Stopped" << std::endl;
        }
        if (state == Running){
            std::cout << "Asquision Running" << std::endl;
        }

        capture_state_.store(state, std::memory_order_release);
    }

    shared_ptr<SrpDevice> SrpSession::device()
    {
        return device_;
    }

    //TODO
    //shared_ptr<SrpSamples>
    void SrpSession::new_storage()
    {
        //std::unordered_map<uint8_t, AChStat*> analog;
        //std::unordered_map<uint8_t, AChControl*> analog;

        vector<uint8_t> analog;
        vector<string> logic;

        for (auto const& [name, ch] : device_->channels() ){
            if (ch->enabled()){
                if(ch->type() == SR_CHANNEL_ANALOG)
                    analog.push_back( ch->index() );

                else if (ch->type() == SR_CHANNEL_LOGIC)
                    logic.push_back(name);
            }
        }

        shared_ptr<SrpConfig> smpls_lim_cfg = device_->config()["limit_samples"];
        const uint64_t smpls_lim = std::get<uint64_t>(smpls_lim_cfg->value());

        shared_ptr<SrpConfig> smplrate_cfg = device_->config()["samplerate"];
        const uint64_t smplrate = std::get<uint64_t>(smplrate_cfg->value());

        cur_storid_ = mcuuid();
        storage_.emplace(cur_storid_, new SrpSamples{cur_storid_, smplrate, smpls_lim, analog });
        //storage_[cur_storid_] = shared_ptr<SrpSamples>{ new SrpSamples{cur_storid_, analog, logic, smplrate, smpls_lim}, std::default_delete<SrpSamples>{} };
    }

    void SrpSession::remove_storage(string stor_id)
    {
        storage_.erase(stor_id);
    }

    void SrpSession::remove_storage()
    {
        remove_storage(cur_storid_);
        cur_storid_.clear();
    }

    shared_ptr<SrpSamples> SrpSession::get_storage(string stor_id)
    {
        return storage_[stor_id];
    }

    shared_ptr<SrpSamples> SrpSession::current_storage()
    {
        return storage_[cur_storid_];
    }


    void SrpSession::set_loop(py::object &coro, py::object &coro_stop, py::object &loop)
    {
        coro_ = coro;
        coro_stop_ = coro_stop;
        loop_ = loop;
        std::cout << "LOOP SET" << std::endl;
    }
}
