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
    simInt dataLength;
    simChar *pdata = simPersistentDataRead((boost::format(fmt) % name).str().c_str(), &dataLength);
    if(!pdata)
    {
#ifdef DEBUG_PERSISTENT_OPTIONS
        sim::addLog(sim_verbosity_debug, "LuaCommander: Could not load persistent option '%s': null pointer error", name);
#endif
        return false;
    }
    bool ok = dataLength == sizeof(T);
    if(ok)
    {
        memcpy(value, pdata, sizeof(T));
#ifdef DEBUG_PERSISTENT_OPTIONS
        std::stringstream ss; ss < *value;
        sim::addLog(sim_verbosity_debug, "LuaCommander: Loaded persistent option '%s': %s", name, ss.str());
#endif
    }
    else
    {
#ifdef DEBUG_PERSISTENT_OPTIONS
        sim::addLog(sim_verbosity_debug, "LuaCommander: Could not load persistent option '%s': incorrect data length %d, should be %d", name, dataLength, sizeof(T));
#endif
    }
    simReleaseBuffer(pdata);
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
    bool ok = simPersistentDataWrite((boost::format(fmt) % name).str().c_str(), reinterpret_cast<const char*>(value), sizeof(T), 1) != -1;
#ifdef DEBUG_PERSISTENT_OPTIONS
    if(!ok)
    {
        sim::addLog(sim_verbosity_debug, "LuaCommander: Could not save persistent option '%s': error -1", name);
    }
#endif
    return ok;
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

