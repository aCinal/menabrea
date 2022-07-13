
#include "callstack.h"
#include <menabrea/log.h>
#include <execinfo.h>
#include <stdlib.h>

#define MAX_CALLSTACK_DEPTH  128

void PrintCallstack(void) {

    void * callstack[MAX_CALLSTACK_DEPTH];

    int depth = backtrace(callstack, MAX_CALLSTACK_DEPTH);
    if (depth > 0) {

        char ** symbols = backtrace_symbols(callstack, depth);
        if (symbols) {

            LogPrint(ELogSeverityLevel_Info, "=============== CALL STACK ===============");
            for (int i = 0; i < depth; i++) {

                LogPrint(ELogSeverityLevel_Info, "    %s", symbols[i]);
            }
            LogPrint(ELogSeverityLevel_Info, "==========================================");

            free(symbols);
        }
    }
}
