
#include <startup/load_applications.h>
#include <menabrea/common.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

static int LoadApplication(SAppLib * handle, char * token);
static SAppDependency * LoadDependencies(char * depList);
static void UnloadDependencies(SAppDependency * depList);
static int LoadLibrary(const char * name, SAppLib * handle);

SAppLibsSet * LoadApplicationLibraries(void) {

    SAppLibsSet * handles = calloc(1, sizeof(SAppLibsSet));
    AssertTrue(handles != NULL);

    /* TODO: Use command-line and not environment to define the applications list */

    /* Fetch library list environment variable */
    const char * libListEnv = getenv(APP_LIB_LIST_ENV);
    if (libListEnv) {

        LogPrint(ELogSeverityLevel_Info, "Loading application libraries...");

        size_t libListLen = strlen(libListEnv);
        /* Allocate a copy on the heap to tokenize the string and leave
         * the environment intact */
        char * libList = malloc(libListLen + 1);
        AssertTrue(libList != NULL);
        (void) strcpy(libList, libListEnv);

        /* Tokenize - break at colon */
        char * saveptr = NULL;
        char * token = strtok_r(libList, ":", &saveptr);
        while (token != NULL && handles->Count < MAX_APP_LIB_NUM) {

            SAppLib * appHandle = &handles->Libs[handles->Count];

            handles->Count += LoadApplication(appHandle, token);
            token = strtok_r(NULL, ":", &saveptr);
        }

        if (token != NULL) {

            LogPrint(ELogSeverityLevel_Warning, "Reached maximum number of application libraries: %d. Not loading '%s'...", \
                MAX_APP_LIB_NUM, token);
        }

        LogPrint(ELogSeverityLevel_Info, "Number of loaded application libraries: %d", \
            handles->Count);

        free(libList);

    } else {

        LogPrint(ELogSeverityLevel_Info, \
            "Environment variable '%s' not set. Not loading any applications", \
            APP_LIB_LIST_ENV);
    }

    return handles;
}

void UnloadApplicationLibraries(SAppLibsSet * appLibs) {

    for (int i = 0; i < appLibs->Count; i++) {

        SAppLib * appLib = &appLibs->Libs[i];
        UnloadDependencies(appLib->Dependencies);
        if (dlclose(appLib->Handle)) {

            const char * error = dlerror();
            LogPrint(ELogSeverityLevel_Warning, "Error when releasing application library '%s': %s", \
                appLib->Name, error);
        }
    }

    free(appLibs);
}

static int LoadApplication(SAppLib * handle, char * token) {

    /* Return the number of loaded applications, i.e., one on success, and zero on failure */

    /* The token is expected to be of the form "<application_name>(<dependency_1>,<dependency_2>)", so start with the application name */
    char * appName = token;

    /* Dependencies are optional, check if they are present */
    char * depStart = strchr(token, '(');
    char * depEnd = strrchr(token, ')');
    if (depStart && depEnd) {

        /* Found dependencies */

        /* Split token "<application_name>(<dependency_1>,<dependency_2>)" into "<application_name>" and "<dependency_1>,<dependency_2>" */
        *depStart++ = '\0';
        *depEnd = '\0';
        LogPrint(ELogSeverityLevel_Debug, "Loading dependencies for '%s': %s", \
            appName, depStart);
        /* It is ok to have duplicate dependencies among different applications since the linker maintains reference counts */
        handle->Dependencies = LoadDependencies(depStart);
        if (NULL == handle->Dependencies) {

            /* TODO: Rethink, maybe we should raise an exception and abort platform startup on application loading error */
            LogPrint(ELogSeverityLevel_Error, "Failed to resolve dependencies for '%s'", appName);
            /* Zero applications loaded */
            return 0;
        }

    } else {

        LogPrint(ELogSeverityLevel_Debug, "No dependencies found for '%s'", appName);
    }

    LogPrint(ELogSeverityLevel_Debug, "Loading library '%s'", appName);
    if (LoadLibrary(appName, handle)) {

        LogPrint(ELogSeverityLevel_Error, "Failed to load library '%s'", appName);
        UnloadDependencies(handle->Dependencies);
        /* Zero applications loaded */
        return 0;
    }

    /* One application successfully loaded */
    return 1;
}

static SAppDependency * LoadDependencies(char * depList) {

    SAppDependency * depStack = NULL;

    char * saveptr;
    char * token = strtok_r(depList, ",", &saveptr);
    while (token != NULL) {

        SAppDependency * entry = malloc(sizeof(SAppDependency));
        AssertTrue(entry != NULL);

        entry->Handle = dlopen(token, RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);
        if (entry->Handle == NULL) {

            LogPrint(ELogSeverityLevel_Error, "Failed to load dependency '%s': %s", \
                token, dlerror());
            /* Unwind and release whatever got loaded so far */
            UnloadDependencies(depStack);
            return NULL;
        }

        /* Copy the name and ensure proper NULL-termination (see strncpy manpage) */
        (void) strncpy(entry->Name, token, sizeof(entry->Name) - 1);
        entry->Name[sizeof(entry->Name) - 1] = '\0';

        /* Push the entry onto the list */
        entry->Next = depStack;
        depStack = entry;

        token = strtok_r(NULL, ",", &saveptr);
    }

    return depStack;
}

static void UnloadDependencies(SAppDependency * depList) {

    while (depList) {

        (void) dlclose(depList->Handle);
        SAppDependency * next = depList->Next;
        free(depList);
        depList = next;
    }
}

static int LoadLibrary(const char * name, SAppLib * handle) {

    char * error;
    /* Allow application libraries to export symbols to each other, but prefer local symbols
     * over already defined global ones */
    /* TODO: Use RTLD_GLOBAL for dependencies, but not for applications themselves (will require changes in libtestframework) */
    void * libHandle = dlopen(name, RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);
    if (libHandle == NULL) {

        LogPrint(ELogSeverityLevel_Error, "Failed to open library '%s': %s", name, dlerror());
        return -1;
    }

    /* Clear any previous error */
    (void) dlerror();

    /* Work around ISO C standard disallowing casting between
     * function pointers and 'void *' */
    *(void **)(&handle->GlobalInit) = dlsym(libHandle, __APPLICATION_GLOBAL_INIT_SYM);
    error = dlerror();
    if (error) {

        LogPrint(ELogSeverityLevel_Error, "Failed to resolve symbol '%s' in library '%s': %s", \
            __APPLICATION_GLOBAL_INIT_SYM, name, error);
        (void) dlclose(libHandle);
        return -1;
    }

    *(void **)(&handle->LocalInit) = dlsym(libHandle, __APPLICATION_LOCAL_INIT_SYM);
    error = dlerror();
    if (error) {

        LogPrint(ELogSeverityLevel_Error, "Failed to resolve symbol '%s' in library '%s': %s", \
            __APPLICATION_LOCAL_INIT_SYM, name, error);
        (void) dlclose(libHandle);
        return -1;
    }

    *(void **)(&handle->LocalExit) = dlsym(libHandle, __APPLICATION_LOCAL_EXIT_SYM);
    error = dlerror();
    if (error) {

        LogPrint(ELogSeverityLevel_Error, "Failed to resolve symbol '%s' in library '%s': %s", \
            __APPLICATION_LOCAL_EXIT_SYM, name, error);
        (void) dlclose(libHandle);
        return -1;
    }

    *(void **)(&handle->GlobalExit) = dlsym(libHandle, __APPLICATION_GLOBAL_EXIT_SYM);
    error = dlerror();
    if (error) {

        LogPrint(ELogSeverityLevel_Error, "Failed to resolve symbol '%s' in library '%s': %s", \
            __APPLICATION_GLOBAL_EXIT_SYM, name, error);
        (void) dlclose(libHandle);
        return -1;
    }

    handle->Handle = libHandle;
    /* Copy the name and ensure proper NULL-termination (see strncpy manpage) */
    (void) strncpy(handle->Name, name, sizeof(handle->Name) - 1);
    handle->Name[sizeof(handle->Name) - 1] = '\0';
    return 0;
}
