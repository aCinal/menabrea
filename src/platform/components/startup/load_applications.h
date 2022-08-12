
#ifndef PLATFORM_COMPONENTS_STARTUP_LOAD_APPLICATIONS_H
#define PLATFORM_COMPONENTS_STARTUP_LOAD_APPLICATIONS_H

#include <limits.h>

#define MAX_APP_LIB_NAME_LEN        PATH_MAX
#define MAX_APP_LIB_NUM             8
#define APP_LIB_LIST_ENV            "MENABREA_APP_LIST"
/* Init before the fork to establish identical view of address spaces */
#define APP_LIB_GLOBAL_INIT_SYMBOL  "ApplicationGlobalInit"
/* Per-core initialization */
#define APP_LIB_LOCAL_INIT_SYMBOL   "ApplicationLocalInit"
/* Per-core teardown */
#define APP_LIB_LOCAL_EXIT_SYMBOL   "ApplicationLocalExit"
/* Global teardown */
#define APP_LIB_GLOBAL_EXIT_SYMBOL  "ApplicationGlobalExit"

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
