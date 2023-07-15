#include <libsigrokcxx/libsigrokcxx.hpp>
#include <libsigrok/libsigrok.h>
#include <uuid/uuid.h>
#include <pybind11/pybind11.h>

#define chk_str(input) ((input) ? input : "")

static inline std::string mcuuid(){
    uuid_t r_uuid;
    char s[UUID_STR_LEN];
    uuid_generate(r_uuid); //uuid_generate_random(_uuid);
    uuid_unparse(r_uuid, s);
    return s;
}

static inline void srcheck(int result){
    if (result != SR_OK)
        throw sigrok::Error(result);
}

static inline PyObject *gvar_to_py(GVariant *gvar){
    gsize i, sz;
    PyObject *pydata = NULL;
    
    if (g_variant_is_container(gvar)){
        GVariant *value;
        if (g_variant_is_of_type(gvar, G_VARIANT_TYPE_VARIANT)){
            value = g_variant_get_variant(gvar);
            pydata = gvar_to_py(value);
        }
            
        else if (g_variant_is_of_type(gvar, G_VARIANT_TYPE_TUPLE)){
            sz = g_variant_n_children(gvar);
            pydata = PyTuple_New(sz);
            for (i = 0; i < sz; i++){
                value = g_variant_get_child_value(gvar, i);
                PyObject *pyitem = gvar_to_py(value);
                PyTuple_SetItem(pydata, i, pyitem);
            }
        }
        
        else if (g_variant_is_of_type(gvar, G_VARIANT_TYPE_DICTIONARY)){
            sz = g_variant_n_children(gvar);
            pydata = PyDict_New();
            for (i = 0; i < sz; i++){
                value = g_variant_get_child_value(gvar, i);
                if (g_variant_is_of_type(value, G_VARIANT_TYPE_DICT_ENTRY)){
                    GVariant *key = g_variant_get_child_value(value, 0);
                    GVariant *item = g_variant_get_child_value(value, 1);
                    PyObject *pykey = gvar_to_py(key);
                    PyObject *pyitem = gvar_to_py(item);
                    PyDict_SetItem(pydata, pykey, pyitem);
                }
            }
        }
        
        else if (g_variant_is_of_type(gvar, G_VARIANT_TYPE_ARRAY)){
            sz = g_variant_n_children(gvar);
            pydata = PyList_New(sz);
            for (i = 0; i < sz; i++){
                value = g_variant_get_child_value(gvar, i);
                PyObject *pyitem = gvar_to_py(value);
                PyList_SetItem(pydata, i, pyitem);
            }
        }
    }
    else {
        if (g_variant_is_of_type(gvar, G_VARIANT_TYPE_STRING))
            pydata = PyUnicode_FromString(g_variant_get_string(gvar, NULL));
            
        else if (g_variant_is_of_type(gvar, G_VARIANT_TYPE_UINT64))
            pydata = PyLong_FromUnsignedLongLong(g_variant_get_uint64(gvar));
            
        else if (g_variant_is_of_type(gvar, G_VARIANT_TYPE_INT32))
            pydata = PyLong_FromLong(g_variant_get_int32(gvar));
            
        else if (g_variant_is_of_type(gvar, G_VARIANT_TYPE_BOOLEAN))
            pydata = g_variant_get_boolean(gvar) ? Py_True : Py_False;                
            
        else if (g_variant_is_of_type(gvar, G_VARIANT_TYPE_DOUBLE))
            pydata = PyFloat_FromDouble(g_variant_get_double(gvar));
    }
    return pydata;
}

static inline sr_config *parse_scan_opts(std::string key, std::string value){
    const struct sr_key_info *srci;
    double tmp_double;
    uint64_t tmp_u64, p, q;

    if (!(srci = sr_key_info_name_get(SR_KEY_CONFIG, key.c_str()))) {
        return nullptr;
    }

    if (value.empty() && (srci->datatype != SR_T_BOOL)) {
        return nullptr;
    }

    auto *src = g_new(struct sr_config, 1);
    src->key = srci->key;
    
    switch (srci->datatype) {
    case SR_T_UINT64:
        if ( (sr_parse_sizestring(value.c_str(), &tmp_u64)) != 0)
            break;
        src->data = g_variant_new_uint64(tmp_u64);
        break;
    case SR_T_INT32:
        if ( (sr_parse_sizestring(value.c_str(), &tmp_u64)) != 0)
            break;
        src->data = g_variant_new_int32(tmp_u64);
        break;
    case SR_T_STRING:
        src->data = g_variant_new_string(value.c_str());
        break;
    case SR_T_BOOL:
        gboolean tmp_bool;
        if (value.empty())
            tmp_bool = TRUE;
        else
            tmp_bool = sr_parse_boolstring(value.c_str());
        src->data = g_variant_new_boolean(tmp_bool);
        break;
    case SR_T_FLOAT:
        tmp_double = strtof(value.c_str(), NULL);
        src->data = g_variant_new_double(tmp_double);
        break;
    case SR_T_RATIONAL_PERIOD:
        if ((sr_parse_period(value.c_str(), &p, &q)) != SR_OK)
            break;
        src->data = g_variant_new("(tt)", p, q);
        break;
    case SR_T_RATIONAL_VOLT:
        if ((sr_parse_voltage(value.c_str(), &p, &q)) != SR_OK)
            break;
        src->data = g_variant_new("(tt)", p, q);
        break;

    default:
        src = nullptr;
    }
    return src;
}


