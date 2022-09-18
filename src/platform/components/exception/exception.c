
#include <menabrea/exception.h>
#include <exception/callstack.h>
#include <menabrea/log.h>
#include <stdarg.h>
#include <signal.h>

void RaiseExceptionImpl(EExceptionFatality fatality, const char * file, int line, const char * function,
    const char * message, ...) {

    va_list ap;
    va_start(ap, message);
    /* Print common header */
    LogPrint(ELogSeverityLevel_Error, "%s EXCEPTION RAISED from %s:%d %s", \
        fatality == EExceptionFatality_Fatal ? "FATAL" : "NON-FATAL", file, line, function);
    /* Print user message */
    LogPrintV(ELogSeverityLevel_Error, message, ap);
    va_end(ap);

    if (fatality == EExceptionFatality_Fatal) {

        /* Kill self with SIGABRT */
        raise(SIGABRT);

    } else {

        /* Do not dump the callstack twice - it will also be printed from the SIGABRT handler
         * in the case of a fatal exception */
        PrintCallstack();
    }
}
