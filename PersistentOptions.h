#ifndef PERSISTENT_OPTIONS_H__INCLUDED
#define PERSISTENT_OPTIONS_H__INCLUDED

struct PersistentOptions
{
#define OPTION(name, type, def) type name = def;
#include "PersistentOptions.list"
#undef OPTION

    void dump();
    bool load();
    bool save();
};

#endif // PERSISTENT_OPTIONS_H__INCLUDED
