#include <libsigrokcxx/libsigrokcxx.hpp>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>
#include <pybind11/pybind11.h>

#include <srcxx/srpconfig.hpp>
#include <srcxx/srpchannels.hpp>
#include <srcxx/srpdevice.hpp>
#include <srcxx/srpdriver.hpp>
#include <srcxx/srpsession.hpp>
#include <srcxx/srpmanager.hpp>
#include <srcxx/srpsamples_segmented.hpp>

namespace py = pybind11;

PYBIND11_MODULE(srpy, m) {
     m.doc() = "Sigrok pybind11 library";
     
     py::class_<srp::SrpChGroup, std::shared_ptr<srp::SrpChGroup> >(m, "SrpChGroup")
          .def("name", &srp::SrpChGroup::name)
          .def("config", &srp::SrpChGroup::config);
     
     py::class_<srp::SrpChannel, std::unique_ptr<srp::SrpChannel> >(m, "SrpChannel")
          .def("name", &srp::SrpChannel::name)
          .def("type", &srp::SrpChannel::type)
          .def("enabled", &srp::SrpChannel::enabled)
          .def("set_enabled", &srp::SrpChannel::set_enabled);
     
     py::class_<srp::SrpManager, std::shared_ptr<srp::SrpManager> > srpmng(m, "SrpManager")
          .def(py::init<>())
          .def_property_readonly("sessions", &srp::SrpManager::sessions)
          .def("add_session", py::overload_cast<>(&srp::SrpManager::add_session), py::return_value_policy::reference)
          .def("add_session", py::overload_cast<std::string>(&srp::SrpManager::add_session), py::return_value_policy::reference)
          .def("remove_session", &srp::SrpManager::remove_session)
          .def("remove_all", &srp::SrpManager::remove_all)
          .def("loglevel_set", &srp::SrpManager::loglevel_set)
          .def_property_readonly("drivers", &srp::SrpManager::drivers);
          
     py::class_<srp::SrpDriver, std::shared_ptr<srp::SrpDriver> >(m, "SrpDriver")
          .def_property_readonly("name", &srp::SrpDriver::name)
          .def_property_readonly("longname", &srp::SrpDriver::longname)
          .def("scan_options", &srp::SrpDriver::scan_options)
          .def("scan", &srp::SrpDriver::scan, "Driver scan for devices", py::arg("opts") = py::dict(), py::return_value_policy::reference);
     
     py::class_<srp::SrpConfig, std::shared_ptr<srp::SrpConfig> >(m, "SrpConfig")
          .def("id", &srp::SrpConfig::id)
          .def("key", &srp::SrpConfig::key)
          .def("get_value", &srp::SrpConfig::get_value)
          .def("set_value", &srp::SrpConfig::set_value, py::call_guard<py::gil_scoped_release>())
          .def("list", &srp::SrpConfig::list)
          .def("caps", &srp::SrpConfig::caps);
          
     py::class_<srp::SrpDevice, std::shared_ptr<srp::SrpDevice> >(m, "SrpDevice")
          .def_property_readonly("driver", &srp::SrpDevice::driver, py::return_value_policy::reference)
          .def("config", &srp::SrpDevice::config)
          .def("channels", &srp::SrpDevice::channels)
          .def("ch_groups", &srp::SrpDevice::ch_groups)
          .def_property_readonly("info", &srp::SrpDevice::info);

     py::class_<srp::SrpSession, std::shared_ptr<srp::SrpSession> >(m, "SrpSession")
          .def("capture_state", &srp::SrpSession::get_capture_state)
          .def("start_capture", &srp::SrpSession::start_capture, py::call_guard<py::gil_scoped_release>())
          .def("stop_capture", &srp::SrpSession::stop_capture, py::call_guard<py::gil_scoped_release>())
          .def("add_device", &srp::SrpSession::add_device, py::call_guard<py::gil_scoped_release>())
          .def("device", &srp::SrpSession::device)
          .def("remove_storage", py::overload_cast<>(&srp::SrpSession::remove_storage))
          .def_property_readonly("samples_stor", &srp::SrpSession::samples_stor)
          //.def("get_storage", &srp::SrpSession::get_storage)
          //.def_property_readonly("device", &srp::SrpSession::device)
          .def_property("scanned_devs", &srp::SrpSession::getScan, &srp::SrpSession::setScan)

          .def_property_readonly("id", &srp::SrpSession::id)
          .def_property_readonly("name", &srp::SrpSession::name)
          .def_property_readonly("type", &srp::SrpSession::type)


          .def("reset_device", &srp::SrpSession::reset_device);

     py::enum_<srp::SrpSession::Capture>(m, "Capture")
          .value("Stopped", srp::SrpSession::Capture::Stopped)
          .value("AwaitingTrigger", srp::SrpSession::Capture::AwaitingTrigger)
          .value("Running", srp::SrpSession::Capture::Running)
          .export_values();

     py::class_<srp::SrpSamples, std::shared_ptr<srp::SrpSamples> >(m, "SrpSamples")
          .def_property_readonly("channels_cache", &srp::SrpSamples::channels_cache);

     py::class_<srp::AChStat,  std::shared_ptr<srp::AChStat> >(m, "AChStat")
          .def_property_readonly("comp_samples", &srp::AChStat::data);

     py::class_<srp::CAnalog>(m, "CAnalog")
          .def_property_readonly("data", [](const srp::CAnalog &ca) {
               return py::memoryview::from_memory(ca.data_, ca.size_);
          });
}
