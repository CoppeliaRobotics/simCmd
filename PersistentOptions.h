#ifndef PERSISTENT_OPTIONS_H__INCLUDED
#define PERSISTENT_OPTIONS_H__INCLUDED

struct PersistentOptions
{
    bool enabled = true;
    int arrayMaxItemsDisplayed = 20;
    int stringLongLimit = 160;
    bool mapSortKeysByName = true;
    bool mapSortKeysByType = true;
    bool mapShadowLongStrings = true;
    bool mapShadowBufferStrings = true;
    bool mapShadowSpecialStrings = true;

    const char * dataTag();
    void dump();
    bool load();
    bool save();
};

#endif // PERSISTENT_OPTIONS_H__INCLUDED
