#include "PersistentOptions.h"

#include <iostream>
#include <boost/format.hpp>

#include "simLib.h"
#include "plugin.h"
#include "stubs.h"
#include "config.h"

void PersistentOptions::dump()
{
#define OPTION(name, type, def) \
    sim::addLog(sim_verbosity_debug, "LuaCommander:     %s = %s", #name, name);
#include "PersistentOptions.list"
#undef OPTION
}

static const char *fmt = "LuaCommander.options.%s";

template<typename T>
inline bool readOption(const char *name, T *value, T def)
{
    *value = def;
    std::string pdata;
    try
    {
        pdata = sim::persistentDataRead((boost::format(fmt) % name).str());
    }
    catch(sim::api_error &ex)
    {
#ifdef DEBUG_PERSISTENT_OPTIONS
        sim::addLog(sim_verbosity_debug, "LuaCommander: Could not load persistent option '%s': %s", name, ex.what());
#endif
        return false;
    }
    bool ok = pdata.length() == sizeof(T);
    if(ok)
    {
        memcpy(value, pdata.c_str(), sizeof(T));
#ifdef DEBUG_PERSISTENT_OPTIONS
        std::stringstream ss; ss << *value;
        sim::addLog(sim_verbosity_debug, "LuaCommander: Loaded persistent option '%s': %s", name, ss.str());
#endif
    }
    else
    {
#ifdef DEBUG_PERSISTENT_OPTIONS
        sim::addLog(sim_verbosity_debug, "LuaCommander: Could not load persistent option '%s': incorrect data length %d, should be %d", name, dataLength, sizeof(T));
#endif
    }
    return ok;
}

bool PersistentOptions::load()
{
#ifdef DEBUG_PERSISTENT_OPTIONS
    sim::addLog(sim_verbosity_debug, "LuaCommander: Loading persistent options...");
#endif
    bool ok = true;
#define OPTION(name, type, def) ok = readOption<type>(#name, &name, def) && ok;
#include "PersistentOptions.list"
#undef OPTION
    return ok;
}

template<typename T>
inline bool writeOption(const char *name, T *value)
{
    try
    {
        std::string data(reinterpret_cast<const char*>(value), sizeof(T));
        sim::persistentDataWrite((boost::format(fmt) % name).str(), data, 1);
    }
    catch(sim::api_error &ex)
    {
#ifdef DEBUG_PERSISTENT_OPTIONS
        sim::addLog(sim_verbosity_debug, "LuaCommander: Could not save persistent option '%s': %s", name, ex.what());
#endif
        return false;
    }
    return true;
}

bool PersistentOptions::save()
{
#ifdef DEBUG_PERSISTENT_OPTIONS
    sim::addLog(sim_verbosity_debug, "LuaCommander: Saving persistent options:");
    dump();
#endif
    bool ok = true;
#define OPTION(name, type, def) ok = writeOption<type>(#name, &name) && ok;
#include "PersistentOptions.list"
#undef OPTION
    return ok;
}

