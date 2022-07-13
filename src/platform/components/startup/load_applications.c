
#include "load_applications.h"
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

static int LoadLibrary(const char * name, SAppLib * handle);

SAppLibsSet * LoadApplicationLibraries(void) {

    SAppLibsSet * handles = malloc(sizeof(SAppLibsSet));
    AssertTrue(handles != NULL);
    handles->Count = 0;
    (void) memset(&handles->Libs, 0, sizeof(handles->Libs));

    /* Fetch library list environment variable */
    const char * libListEnv = getenv(APP_LIB_LIST_ENV);
    if (libListEnv) {

        size_t libListLen = strlen(libListEnv);
        /* Allocate a copy on the heap to tokenize the string and leave
         * the environment intact */
        char * libList = malloc(libListLen + 1);
        AssertTrue(libList != NULL);
        (void) strcpy(libList, libListEnv);

        /* Tokenize - break at colon */
        char * saveptr = NULL;
        char * token = strtok_r(libList, ":", &saveptr);
        while (token != NULL) {

            LogPrint(ELogSeverityLevel_Debug, "%s(): Trying to load library: %s", \
                __FUNCTION__, token);

            if (0 == LoadLibrary(token, &handles->Libs[handles->Count])) {

                handles->Count++;
            }

            token = strtok_r(NULL, ":", &saveptr);
        }

        LogPrint(ELogSeverityLevel_Info, "%s(): Loaded %d application libraries", \
            __FUNCTION__, handles->Count);

        free(libList);

    } else {

        LogPrint(ELogSeverityLevel_Info, "%s(): Environment variable %s not set. Not loading any applications", \
            __FUNCTION__, APP_LIB_LIST_ENV);
    }

    return handles;
}

static int LoadLibrary(const char * name, SAppLib * handle) {

    char * error;
    void * libHandle = dlopen(name, RTLD_NOW | RTLD_DEEPBIND);
    if (libHandle == NULL) {

        LogPrint(ELogSeverityLevel_Error, "Failed to open library %s: %s", name, dlerror());
        return -1;
    }

    /* Clear any previous error */
    (void) dlerror();

    /* Work around ISO C standard disallowing casting between
     * function pointers and 'void *' */
    *(void **)(&handle->GlobalInit) = dlsym(libHandle, APP_LIB_GLOBAL_INIT_SYMBOL);
    error = dlerror();
    if (error) {

        LogPrint(ELogSeverityLevel_Error, "Failed to resolve symbol %s in library %s: %s", \
            APP_LIB_GLOBAL_INIT_SYMBOL, name, error);
        (void) dlclose(libHandle);
        return -1;
    }

    *(void **)(&handle->LocalInit) = dlsym(libHandle, APP_LIB_LOCAL_INIT_SYMBOL);
    error = dlerror();
    if (error) {

        LogPrint(ELogSeverityLevel_Error, "Failed to resolve symbol %s in library %s: %s", \
            APP_LIB_LOCAL_INIT_SYMBOL, name, error);
        (void) dlclose(libHandle);
        return -1;
    }

    *(void **)(&handle->LocalExit) = dlsym(libHandle, APP_LIB_LOCAL_EXIT_SYMBOL);
    error = dlerror();
    if (error) {

        LogPrint(ELogSeverityLevel_Error, "Failed to resolve symbol %s in library %s: %s", \
            APP_LIB_LOCAL_EXIT_SYMBOL, name, error);
        (void) dlclose(libHandle);
        return -1;
    }

    *(void **)(&handle->GlobalExit) = dlsym(libHandle, APP_LIB_GLOBAL_EXIT_SYMBOL);
    error = dlerror();
    if (error) {

        LogPrint(ELogSeverityLevel_Error, "Failed to resolve symbol %s in library %s: %s", \
            APP_LIB_GLOBAL_EXIT_SYMBOL, name, error);
        (void) dlclose(libHandle);
        return -1;
    }

    handle->Handle = libHandle;
    /* Copy the name and ensure proper NULL-termination (see strncpy manpage) */
    (void) strncpy(handle->Name, name, sizeof(handle->Name) - 1);
    handle->Name[sizeof(handle->Name) - 1] = '\0';
    return 0;
}
