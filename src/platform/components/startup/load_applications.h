
#ifndef PLATFORM_COMPONENTS_STARTUP_LOAD_APPLICATIONS_H
#define PLATFORM_COMPONENTS_STARTUP_LOAD_APPLICATIONS_H

#include <limits.h>

#define MAX_APP_LIB_NAME_LEN        PATH_MAX
#define MAX_APP_LIB_NUM             8
#define APP_LIB_LIST_ENV            "MENABREA_APP_LIST"
#define MAX_DEP_NAME_LEN            PATH_MAX

typedef struct SAppDependency {
    void * Handle;
    struct SAppDependency * Next;
    char Name[MAX_DEP_NAME_LEN];
} SAppDependency;

typedef struct SAppLib {
    void * Handle;
    void (* GlobalInit)(void);
    void (* LocalInit)(int core);
    void (* LocalExit)(int core);
    void (* GlobalExit)(void);
    SAppDependency * Dependencies;
    char Name[MAX_APP_LIB_NAME_LEN];
} SAppLib;

typedef struct SAppLibsSet {
    SAppLib Libs[MAX_APP_LIB_NUM];
    int Count;
} SAppLibsSet;

SAppLibsSet * LoadApplicationLibraries(void);
void UnloadApplicationLibraries(SAppLibsSet * appLibs);

#endif /* PLATFORM_COMPONENTS_STARTUP_LOAD_APPLICATIONS_H */
