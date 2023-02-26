
#ifndef PLATFORM_COMPONENTS_STARTUP_LOAD_APPLICATIONS_H
#define PLATFORM_COMPONENTS_STARTUP_LOAD_APPLICATIONS_H

#include <limits.h>

#define MAX_APP_LIB_NAME_LEN        PATH_MAX
#define MAX_APP_LIB_NUM             8
#define APP_LIB_LIST_ENV            "MENABREA_APP_LIST"

typedef struct SAppLib {
    char Name[MAX_APP_LIB_NAME_LEN];
    void * Handle;
    void (* GlobalInit)(void);
    void (* LocalInit)(int core);
    void (* LocalExit)(int core);
    void (* GlobalExit)(void);
} SAppLib;

typedef struct SAppLibsSet {
    int Count;
    SAppLib Libs[MAX_APP_LIB_NUM];
} SAppLibsSet;

SAppLibsSet * LoadApplicationLibraries(void);
void UnloadApplicationLibraries(SAppLibsSet * appLibs);

#endif /* PLATFORM_COMPONENTS_STARTUP_LOAD_APPLICATIONS_H */
