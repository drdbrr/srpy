#include "srpconfig.hpp"
#include "utils.hpp"

//namespace py = pybind11; ??????

using std::map;
using std::string;

namespace srp {
    SrpConfig::SrpConfig(uint32_t key, struct sr_dev_driver *drv, struct sr_dev_inst *sdi, struct sr_channel_group *cg) :
        conf_drv_(drv),
        conf_sdi_(sdi),
        conf_cg_(cg)
    {
        info_ = sr_key_info_get(SR_KEY_CONFIG, key);
    }

    SrpConfig::~SrpConfig()
    {
    }

    string SrpConfig::id()
    {
        return info_->id;
    }

    uint32_t SrpConfig::key()
    {
        return info_->key;
    }

    std::variant<std::string, uint32_t, uint64_t> SrpConfig::value()
    {
        GVariant *gvar;
        std::variant<std::string, uint32_t, uint64_t> result;

        if (sr_dev_config_capabilities_list(conf_sdi_, conf_cg_, info_->key) & SR_CONF_GET){
            srcheck(sr_config_get(conf_drv_, conf_sdi_, conf_cg_, info_->key, &gvar));

            if (g_variant_is_of_type(gvar, G_VARIANT_TYPE_STRING)){
                result = string(g_variant_get_string(gvar, NULL));
            }
            else if (g_variant_is_of_type(gvar, G_VARIANT_TYPE_UINT64)){
                result = (uint64_t)g_variant_get_uint64(gvar);
            }
            g_variant_unref(gvar);

        }
        return result;
    }

    py::handle SrpConfig::get_value(){
        PyObject *result;
        GVariant *gvar;
        
        if (sr_dev_config_capabilities_list(conf_sdi_, conf_cg_, info_->key) & SR_CONF_GET){
            srcheck(sr_config_get(conf_drv_, conf_sdi_, conf_cg_, info_->key, &gvar));
            result = gvar_to_py(gvar);
            g_variant_unref(gvar);
        }
        
        return py::handle(result);
    }


    void SrpConfig::set_value(py::handle &pyval){
        if (sr_dev_config_capabilities_list(conf_sdi_, conf_cg_, info_->key) & SR_CONF_SET){
            py::gil_scoped_acquire gil;
            GVariant *gvar;
            
            if (info_->datatype == SR_T_UINT64){
                guint64 data;
                if (py::isinstance<py::int_>(pyval))
                    data = pyval.cast<guint64>();
                
                else if (py::isinstance<py::str>(pyval)){
                    string sval = pyval.cast<string>();
                    srcheck(sr_parse_sizestring(sval.c_str(), &data));
                }
                gvar = g_variant_new_uint64(data);
            }
            
            else if (info_->datatype == SR_T_INT32){
                gint32 data;
                if (py::isinstance<py::int_>(pyval))
                    data = pyval.cast<gint32>();
                
                else if (py::isinstance<py::str>(pyval)){
                    string str = pyval.cast<string>();
                    data = std::atoi(str.c_str());
                }
                
                gvar = g_variant_new_int32(data);
            }
            
            else if (info_->datatype == SR_T_STRING){
                string data;
                if (py::isinstance<py::int_>(pyval))
                    data = std::to_string(pyval.cast<int>());
                
                else if (py::isinstance<py::str>(pyval))
                    data = pyval.cast<string>();
                gvar = g_variant_new_string(data.c_str());
            }
            
            else if (info_->datatype == SR_T_BOOL){
                bool data;
                if (py::isinstance<py::int_>(pyval))
                    data = pyval.cast<int>();
            
                else if (py::isinstance<py::str>(pyval))
                    std::istringstream(pyval.cast<string>()) >> std::boolalpha >> data;
                
                else if (py::isinstance<py::bool_>(pyval))
                    data = pyval.cast<bool>();
                    
                gvar = g_variant_new_boolean(data);
            }
            
            else if (info_->datatype == SR_T_FLOAT ){
                double data;
                if (py::isinstance<py::int_>(pyval))
                    data = pyval.cast<double>();
                
                else if (py::isinstance<py::str>(pyval))
                    data = std::stod(pyval.cast<string>());
                
                else if (py::isinstance<py::float_>(pyval))
                    data = pyval.cast<double>();
            
                gvar = g_variant_new_double( data );
            }
            
            else if ( (info_->datatype == SR_T_RATIONAL_VOLT) && PyTuple_Check(pyval.ptr()) && PyTuple_Size(pyval.ptr()) == 2) {
                PyObject *numObj = PyTuple_GetItem(pyval.ptr(), 0);
                PyObject *denomObj = PyTuple_GetItem(pyval.ptr(), 1);
                if (PyLong_Check(numObj) && PyLong_Check(denomObj)) {
                    guint64 q = PyLong_AsLong(numObj);
                    guint64 p = PyLong_AsLong(denomObj);
                    gvar = g_variant_new("(tt)", q, p);
                }
            }
            
            py::gil_scoped_release release;
            srcheck(sr_config_set(conf_sdi_, conf_cg_, info_->key, gvar));
        }
    }

    py::handle SrpConfig::list(){
        GVariant *gvar;
        PyObject *result;
        
        if (sr_dev_config_capabilities_list(conf_sdi_, conf_cg_, info_->key) & SR_CONF_LIST){
            srcheck(sr_config_list(conf_drv_, conf_sdi_, conf_cg_, info_->key, &gvar));
            result = gvar_to_py(gvar);
            g_variant_unref(gvar);
        }
        
        return py::handle(result);
    }

    map<int, string> SrpConfig::caps(){
        map<int, string> result;
        int cap = sr_dev_config_capabilities_list(conf_sdi_, conf_cg_, info_->key);

        if (cap & SR_CONF_GET)
            result.emplace(SR_CONF_GET, "GET");
        if (cap & SR_CONF_SET)
            result.emplace(SR_CONF_SET, "SET");
        if (cap & SR_CONF_LIST)
            result.emplace(SR_CONF_LIST, "LIST");
        return result;
    }
}
