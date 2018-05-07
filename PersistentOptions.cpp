#include "PersistentOptions.h"

#include <iostream>

#include "v_repLib.h"
#include "plugin.h"
#include "stubs.h"
#include "config.h"
#include "debug.h"

const char * PersistentOptions::dataTag()
{
    return "LuaCommanderOptions";
}

void PersistentOptions::dump()
{
    std::cout << "LuaCommander:     enabled=" << enabled << std::endl;
    std::cout << "LuaCommander:     arrayMaxItemsDisplayed=" << arrayMaxItemsDisplayed << std::endl;
    std::cout << "LuaCommander:     stringLongLimit=" << stringLongLimit << std::endl;
    std::cout << "LuaCommander:     mapShadowBufferStrings=" << mapShadowBufferStrings << std::endl;
    std::cout << "LuaCommander:     mapShadowSpecialStrings=" << mapShadowSpecialStrings << std::endl;
    std::cout << "LuaCommander:     mapShadowLongStrings=" << mapShadowLongStrings << std::endl;
    std::cout << "LuaCommander:     mapSortKeysByName=" << mapSortKeysByName << std::endl;
    std::cout << "LuaCommander:     mapSortKeysByType=" << mapSortKeysByType << std::endl;
}

bool PersistentOptions::load()
{
#ifdef DEBUG_PERSISTENT_OPTIONS
    std::cout << "LuaCommander: Loading persistent options..." << std::endl;
#endif
    simInt dataLength;
    simChar *pdata = simPersistentDataRead(dataTag(), &dataLength);
    if(!pdata)
    {
#ifdef DEBUG_PERSISTENT_OPTIONS
        std::cout << "LuaCommander: Could not load persistent options: null pointer error" << std::endl;
#endif
        return false;
    }
    bool ok = dataLength == sizeof(*this);
    if(ok)
    {
        memcpy(this, pdata, sizeof(*this));
#ifdef DEBUG_PERSISTENT_OPTIONS
        std::cout << "LuaCommander: Loaded persistent options:" << std::endl;
        dump();
#endif
    }
    else
    {
#ifdef DEBUG_PERSISTENT_OPTIONS
        std::cout << "LuaCommander: Could not load persistent options: incorrect data length " << dataLength << ", should be " << sizeof(*this) << std::endl;
#endif
    }
    simReleaseBuffer(pdata);
    return ok;
}

bool PersistentOptions::save()
{
#ifdef DEBUG_PERSISTENT_OPTIONS
    std::cout << "LuaCommander: Saving persistent options:" << std::endl;
    dump();
#endif
    return simPersistentDataWrite(dataTag(), (simChar*)this, sizeof(*this), 1) != -1;
}

