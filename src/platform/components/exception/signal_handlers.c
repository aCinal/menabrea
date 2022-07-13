#define _GNU_SOURCE
#include "signal_handlers.h"
#include "callstack.h"
#include <menabrea/log.h>
#include <ucontext.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>

#define MAX_LISTENERS_PER_SIGNAL           8
#define USER_LISTENERS_MAP_SIZE            ( sizeof(s_userListeners) / sizeof(s_userListeners[0]) )
#define OFFENDING_INSTRUCTION_BUFFER_SIZE  128

typedef struct SUserListenerMap {
    int Signal;
    TUserSignalListener Listeners[MAX_LISTENERS_PER_SIGNAL];
} SUserListenerMap;

static SUserListenerMap s_userListeners[] = {
    { .Signal = SIGTERM, .Listeners = {} },
    { .Signal = SIGCHLD, .Listeners = {} },
    { .Signal = SIGINT, .Listeners = {} },
    { .Signal = SIGQUIT, .Listeners = {} }
};

static void CommonHandler(int signo, siginfo_t * siginfo, void * ucontext);
static bool CallUserListeners(int signo, siginfo_t * siginfo);
static void SigbusHandler(siginfo_t * siginfo, ucontext_t * ucontext);
static void SigfpeHandler(siginfo_t * siginfo, ucontext_t * ucontext);
static void SigillHandler(siginfo_t * siginfo, ucontext_t * ucontext);
static void SigsegvHandler(siginfo_t * siginfo, ucontext_t * ucontext);
static void SigabrtHandler(siginfo_t * siginfo, ucontext_t * ucontext);
static void SigchldHandler(siginfo_t * siginfo);
static void SigtermHandler(siginfo_t * siginfo);
static void SigintHandler(siginfo_t * siginfo);
static void SigquitHandler(siginfo_t * siginfo);
static const char * GetOffendingInstruction(ucontext_t * ucontext, char * buffer, size_t size);
static void PrintProcessInfo(void);
static void RestoreDefaultHandlerAndRaise(int signo);

void InstallSignalHandlers(void) {

    struct sigaction act = {
        .sa_sigaction = CommonHandler,
        .sa_flags = SA_SIGINFO | SA_NOCLDSTOP | SA_RESTART,
    };
    sigemptyset(&act.sa_mask);

    assert(0 == sigaction(SIGBUS, &act, NULL));
    assert(0 == sigaction(SIGILL, &act, NULL));
    assert(0 == sigaction(SIGSEGV, &act, NULL));
    assert(0 == sigaction(SIGFPE, &act, NULL));
    assert(0 == sigaction(SIGABRT, &act, NULL));
    assert(0 == sigaction(SIGCHLD, &act, NULL));
    assert(0 == sigaction(SIGTERM, &act, NULL));
    assert(0 == sigaction(SIGINT, &act, NULL));
}

int ListenForSignal(int signo, TUserSignalListener callback) {

    /* Note that this function is not thread-safe */

    for (int i = 0; i < USER_LISTENERS_MAP_SIZE; i++) {

        if (s_userListeners[i].Signal == signo) {

            /* Find free space */
            for (int j = 0; j < MAX_LISTENERS_PER_SIGNAL; j++) {

                if (NULL == s_userListeners[i].Listeners[j]) {

                    s_userListeners[i].Listeners[j] = callback;
                    return 0;
                }
            }

            LogPrint(ELogSeverityLevel_Warning, "%s(): Already installed maximum number of listeners for signal %d", \
                __FUNCTION__, signo);
            return -1;
        }
    }

    LogPrint(ELogSeverityLevel_Warning, "%s(): Attempted to register listener for unsupported signal %d", \
        __FUNCTION__, signo);
    return -1;
}

static void CommonHandler(int signo, siginfo_t * siginfo, void * ucontext) {

    ucontext_t * context = (ucontext_t *) ucontext;
    bool handledByUser = CallUserListeners(signo, siginfo);

    /* Call platform handler */
    switch (signo) {
    case SIGBUS:
        SigbusHandler(siginfo, context);
        break;

    case SIGFPE:
        SigfpeHandler(siginfo, context);
        break;

    case SIGILL:
        SigillHandler(siginfo, context);
        break;

    case SIGSEGV:
        SigsegvHandler(siginfo, context);
        break;

    case SIGABRT:
        SigabrtHandler(siginfo, context);
        break;

    case SIGCHLD:
        SigchldHandler(siginfo);
        break;

    case SIGTERM:
        SigtermHandler(siginfo);
        break;

    case SIGINT:
        SigintHandler(siginfo);
        break;

    case SIGQUIT:
        SigquitHandler(siginfo);
        break;

    default:
        /* Should never get here */
        LogPrint(ELogSeverityLevel_Warning, "Signal %d missing a handler in %s", \
            signo, __FILE__);
        break;
    }

    /* Restore default behaviour for the signal and raise it again
     * unless handled by one of users' listeners */
    if (!handledByUser) {

        LogPrint(ELogSeverityLevel_Info, "Restoring default behaviour for signal %d (%s) and raising it again...", \
            signo, strsignal(signo));
        RestoreDefaultHandlerAndRaise(signo);
    }
}

static bool CallUserListeners(int signo, siginfo_t * siginfo) {

    bool handled = false;
    /* Call user listeners if any registered */
    for (int i = 0; i < USER_LISTENERS_MAP_SIZE; i++) {

        if (s_userListeners[i].Signal == signo) {

            for (int j = 0; j < MAX_LISTENERS_PER_SIGNAL; j++) {

                if (s_userListeners[i].Listeners[j]) {

                    /* User handlers should return true value when the signal has been
                     * handled. If no handlers declare they have handled the signal,
                     * then default behaviour shall be triggered */
                    handled |= s_userListeners[i].Listeners[j](signo, siginfo);
                }
            }
        }
    }

    return handled;
}

static void SigbusHandler(siginfo_t * siginfo, ucontext_t * ucontext) {

    char instruction[OFFENDING_INSTRUCTION_BUFFER_SIZE];
    GetOffendingInstruction(ucontext, instruction, sizeof(instruction));
    const char * reason;

    switch (siginfo->si_code) {
    case BUS_ADRALN:
        reason = "invalid address alignment";
        break;

    case BUS_ADRERR:
        reason = "nonexistent physical address";
        break;

    case BUS_OBJERR:
        reason = "object-specific hardware error";
        break;

    case BUS_MCEERR_AR:
        /* Action required */
        reason = "hardware memory error consumed on a machine check";
        break;

    case BUS_MCEERR_AO:
        /* Actiona optional */
        reason = "hardware memory error detected in process but not consumed";
        break;

    default:
        reason = "???";
        break;
    }

    LogPrint(ELogSeverityLevel_Error, "%s(): Memory bus error (%s) at %p (%s) (offending address: %p)", \
        __FUNCTION__, reason, (const void *) ucontext->uc_mcontext.pc, instruction, siginfo->si_addr);
    PrintProcessInfo();
    PrintCallstack();
}

static void SigfpeHandler(siginfo_t * siginfo, ucontext_t * ucontext) {

    char instruction[OFFENDING_INSTRUCTION_BUFFER_SIZE];
    GetOffendingInstruction(ucontext, instruction, sizeof(instruction));
    const char * reason;

    switch (siginfo->si_code) {
    case FPE_INTDIV:
        reason = "integer divide by zero";
        break;

    case FPE_INTOVF:
        reason = "integer overflow";
        break;

    case FPE_FLTDIV:
        reason = "floating-point divide by zero";
        break;

    case FPE_FLTOVF:
        reason = "floating-point overflow";

    case FPE_FLTUND:
        reason = "floating-point underflow";
        break;

    case FPE_FLTRES:
        reason = "floating-point inexact result";
        break;

    case FPE_FLTINV:
        reason = "floating-point invalid operation";
        break;

    case FPE_FLTSUB:
        reason = "subscript out of range";
        break;

    default:
        reason = "???";
        break;
    }

    LogPrint(ELogSeverityLevel_Error, "%s(): Floating-point exception (%s) at %p (%s) (offending address: %p)", \
        __FUNCTION__, reason, (const void *) ucontext->uc_mcontext.pc, instruction, siginfo->si_addr);
    PrintProcessInfo();
    PrintCallstack();
}

static void SigillHandler(siginfo_t * siginfo, ucontext_t * ucontext) {

    char instruction[OFFENDING_INSTRUCTION_BUFFER_SIZE];
    GetOffendingInstruction(ucontext, instruction, sizeof(instruction));
    const char * reason;

    switch (siginfo->si_code) {
    case ILL_ILLOPC:
        reason = "illegal opcode";
        break;

    case ILL_ILLOPN:
        reason = "illegal operand";
        break;

    case ILL_ILLADR:
        reason = "illegal addressing mode";
        break;

    case ILL_ILLTRP:
        reason = "illegal trap";
        break;

    case ILL_PRVOPC:
        reason = "privileged opcode";
        break;

    case ILL_PRVREG:
        reason = "privileged register";
        break;

    case ILL_COPROC:
        reason = "coprocessor error";
        break;

    case ILL_BADSTK:
        reason = "internal stack error";
        break;

    default:
        reason = "???";
        break;
    }

    LogPrint(ELogSeverityLevel_Error, "%s(): Illegal instruction error (%s) at %p (%s) (offending address: %p)", \
        __FUNCTION__, reason, (const void *) ucontext->uc_mcontext.pc, instruction, siginfo->si_addr);
    PrintProcessInfo();
    PrintCallstack();
}

static void SigsegvHandler(siginfo_t * siginfo, ucontext_t * ucontext) {

    char instruction[OFFENDING_INSTRUCTION_BUFFER_SIZE];
    GetOffendingInstruction(ucontext, instruction, sizeof(instruction));

    switch (siginfo->si_code) {
    case SEGV_MAPERR:
        LogPrint(ELogSeverityLevel_Error, "%s(): Segmentation violation (address not mapped to object) at %p (%s) (offending address: %p)", \
            __FUNCTION__, (const void *) ucontext->uc_mcontext.pc, instruction, siginfo->si_addr);
        break;

    case SEGV_ACCERR:
        LogPrint(ELogSeverityLevel_Error, "%s(): Segmentation violation (invalid permissions) at %p (%s) (offending address: %p)", \
            __FUNCTION__, (const void *) ucontext->uc_mcontext.pc, instruction, siginfo->si_addr);
        break;

    default: /* Do not expect SEGV_BNDERR or SEGV_PKUERR */
        LogPrint(ELogSeverityLevel_Error, "%s(): Segmentation violation (si_code=%d) at %p (%s) (offending address: %p)", \
            __FUNCTION__, siginfo->si_code, (const void *) ucontext->uc_mcontext.pc, instruction, siginfo->si_addr);
        break;
    }

    PrintProcessInfo();
    PrintCallstack();
}

static void SigabrtHandler(siginfo_t * siginfo, ucontext_t * ucontext) {

    char instruction[OFFENDING_INSTRUCTION_BUFFER_SIZE];
    GetOffendingInstruction(ucontext, instruction, sizeof(instruction));

    /* Assume SIGABRT is only raised by the process itself, but print the PID
     * nonetheless. */
    LogPrint(ELogSeverityLevel_Error, "%s(): SIGABRT raised by %d at %p (%s)", \
        __FUNCTION__, siginfo->si_code == SI_USER ? siginfo->si_pid : -1, \
        (const void *) ucontext->uc_mcontext.pc, instruction);

    PrintProcessInfo();
    PrintCallstack();
}

static void SigchldHandler(siginfo_t * siginfo) {

    /* Log the event - note that we assume SA_NOCLDSTOP as passed to sigaction,
     * so that we only receive SIGCHLD on termination of children */
    switch (siginfo->si_code) {
    case CLD_EXITED:
        LogPrint(ELogSeverityLevel_Info, "%s(): Child %d exited with status %d", \
            __FUNCTION__, siginfo->si_pid, siginfo->si_status);
        break;

    case CLD_KILLED:
        LogPrint(ELogSeverityLevel_Info, "%s(): Child %d was killed by signal %d", \
            __FUNCTION__, siginfo->si_pid, siginfo->si_status);
        break;

    case CLD_DUMPED:
        LogPrint(ELogSeverityLevel_Info, "%s(): Child %d terminated abnormally with status %d", \
            __FUNCTION__, siginfo->si_pid, siginfo->si_status);
        break;

    default:
        LogPrint(ELogSeverityLevel_Warning, "%s(): Received SIGCHLD for child %d with si_code=%d (si_status=%d)", \
            __FUNCTION__, siginfo->si_pid, siginfo->si_code, siginfo->si_status);
        break;
    }
}

static void SigtermHandler(siginfo_t * siginfo) {

    /* Nothing to do here - block the signal and allow users to install a handler for it */

    LogPrint(ELogSeverityLevel_Info, "%s(): Received SIGTERM from %d", \
        __FUNCTION__, siginfo->si_code == SI_USER ? siginfo->si_pid : -1);  /* Use -1 to indicate kernel */
}

static void SigintHandler(siginfo_t * siginfo) {

    /* Nothing to do here - block the signal and allow users to install a handler for it */

    LogPrint(ELogSeverityLevel_Info, "%s(): Received SIGINT from %d", \
        __FUNCTION__, siginfo->si_code == SI_USER ? siginfo->si_pid : -1);  /* Use -1 to indicate kernel */
}

static void SigquitHandler(siginfo_t * siginfo) {

    /* Nothing to do here - block the signal and allow users to install a handler for it */

    LogPrint(ELogSeverityLevel_Info, "%s(): Received SIGQUIT from %d", \
        __FUNCTION__, siginfo->si_code == SI_USER ? siginfo->si_pid : -1);  /* Use -1 to indicate kernel */
}

static const char * GetOffendingInstruction(ucontext_t * ucontext, char * buffer, size_t size) {

    int status;
    Dl_info dlinfo;
    const char * pathname;
    const char * symbolname;
    /* Recover the program counter from ucontext */
    const void * pc = (const void *) ucontext->uc_mcontext.pc;
    /* Try resolving the symbol */
    status = dladdr(pc, &dlinfo);
    /* Non-zero return value indicates success */
    if (status) {

        pathname = dlinfo.dli_fname;
        symbolname = dlinfo.dli_sname;

    } else {

        pathname = "?";
        symbolname = "?";
    }

    (void) snprintf(buffer, size, "%s:%s", pathname, symbolname);
    return buffer;
}

static void PrintProcessInfo(void) {

    char procName[16] = "<unknown>";
    LogPrint(ELogSeverityLevel_Critical, "%s(): Implement me!", __FUNCTION__);

    (void) prctl(PR_GET_NAME, procName, 0, 0, 0);

    LogPrint(ELogSeverityLevel_Info, "============== PROCESS INFO ==============");
    LogPrint(ELogSeverityLevel_Info, "    Name:  %s", procName);
    LogPrint(ELogSeverityLevel_Info, "    PID:   %d", getpid());
    LogPrint(ELogSeverityLevel_Info, "    PPID:  %d", getppid());
    LogPrint(ELogSeverityLevel_Info, "==========================================");
}

static void RestoreDefaultHandlerAndRaise(int signo) {

    struct sigaction act = {
        .sa_handler = SIG_DFL,
    };
    assert(0 == sigaction(signo, &act, NULL));
    raise(signo);
}
