#include "dbus.h"
#include <iostream>
#include <algorithm>

#define XCONCATENATE(x, y) x ## y
#define CONCATENATE(x, y) XCONCATENATE(x, y)
#define UNIQ_T(x, uniq) CONCATENATE(__unique_prefix_, CONCATENATE(x, uniq))
#define UNIQ __COUNTER__
#define _STRV_FOREACH(s, l, i) for(typeof(*(l)) *s, *i = (l); (s = i) && *i; i++)
#define STRV_FOREACH(s, l) _STRV_FOREACH(s, l, UNIQ_T(i, UNIQ))

char** strv_free(char** v) {
    if(!v){
        return NULL;
    }
    for(char** i = v; *i; i++){
        free(*i);
    }
    free(v);
    return NULL;
}

namespace Blight {
    DBusException::DBusException(std::string message)
    : std::runtime_error(message.c_str())
    {}

    DBus::DBus(bool use_system)
    : m_bus(nullptr)
    {
        int res;
        if(use_system){
            res = sd_bus_default_system(&m_bus);
        }else{
            res = sd_bus_default_user(&m_bus);
        }
        if(res < 0){
            throw DBusException("Could not connect to bus");
        }
    }

    DBus::~DBus(){
        sd_bus_unref(m_bus);
    }

    sd_bus* DBus::bus(){ return m_bus; }

    std::vector<std::string> DBus::services(){
        char** names = NULL;
        char** activatable = NULL;
        std::vector<std::string> services;
        if(sd_bus_list_names(m_bus, &names, &activatable) < 0){
            strv_free(names);
            strv_free(activatable);
            return services;
        }
        STRV_FOREACH(i, names){
            services.push_back(std::string(*i));
        }
        STRV_FOREACH(i, activatable){
            auto name = std::string(*i);
            if(std::find(services.begin(), services.end(), name) != services.end()){
                services.push_back(name);
            }
        }
        strv_free(names);
        strv_free(activatable);
        return services;
    }

    bool DBus::has_service(std::string service){
        auto services = this->services();
        return std::find(services.begin(), services.end(), service) != services.end();
    }
}
