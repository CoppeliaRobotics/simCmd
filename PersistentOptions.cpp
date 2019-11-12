#include "PersistentOptions.h"

#include <iostream>
#include <boost/format.hpp>

#include "simLib.h"
#include "plugin.h"
#include "stubs.h"
#include "config.h"
#include "debug.h"

void PersistentOptions::dump()
{
#define OPTION(name, type, def) \
    std::cout << "LuaCommander:     " #name "=" << name << std::endl;
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
        std::cout << "LuaCommander: Could not load persistent option '" << name << "': null pointer error" << std::endl;
#endif
        return false;
    }
    bool ok = dataLength == sizeof(T);
    if(ok)
    {
        memcpy(value, pdata, sizeof(T));
#ifdef DEBUG_PERSISTENT_OPTIONS
        std::cout << "LuaCommander: Loaded persistent option '" << name << "': " << *value << std::endl;
#endif
    }
    else
    {
#ifdef DEBUG_PERSISTENT_OPTIONS
        std::cout << "LuaCommander: Could not load persistent option '" << name << "': incorrect data length " << dataLength << ", should be " << sizeof(T) << std::endl;
#endif
    }
    simReleaseBuffer(pdata);
    return ok;
}

bool PersistentOptions::load()
{
#ifdef DEBUG_PERSISTENT_OPTIONS
    std::cout << "LuaCommander: Loading persistent options..." << std::endl;
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
        std::cout << "LuaCommander: Could not save persistent option '" << name << "': error -1" << std::endl;
    }
#endif
    return ok;
}

bool PersistentOptions::save()
{
#ifdef DEBUG_PERSISTENT_OPTIONS
    std::cout << "LuaCommander: Saving persistent options:" << std::endl;
    dump();
#endif
    bool ok = true;
#define OPTION(name, type, def) ok = writeOption<type>(#name, &name) && ok;
#include "PersistentOptions.list"
#undef OPTION
    return ok;
}

