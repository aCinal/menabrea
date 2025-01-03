#define _GNU_SOURCE
#include <menabrea/exception.h>
#include <exception/signal_handlers.h>
#include <exception/callstack.h>
#include <menabrea/log.h>
#include <ucontext.h>
#include <sys/prctl.h>
#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define OFFENDING_INSTRUCTION_BUFFER_SIZE  128

typedef struct SListenersListNode {
    struct SListenersListNode * Next;
    TSignalListener Callback;
    int Signal;
} SListenersListNode;

static SListenersListNode * s_signalListeners = NULL;

static void CommonHandler(int signo, siginfo_t * siginfo, void * ucontext);
static void CallListeners(int signo, siginfo_t * siginfo);
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
static bool RestoreDefaultHandlerAndRaise(int signo, const siginfo_t * siginfo);

void InstallSignalHandlers(void) {

    struct sigaction act = {
        .sa_sigaction = CommonHandler,
        .sa_flags = SA_SIGINFO | SA_NOCLDSTOP | SA_RESTART
    };
    sigemptyset(&act.sa_mask);
    AssertTrue(0 == sigaction(SIGBUS, &act, NULL));
    AssertTrue(0 == sigaction(SIGILL, &act, NULL));
    AssertTrue(0 == sigaction(SIGSEGV, &act, NULL));
    AssertTrue(0 == sigaction(SIGFPE, &act, NULL));
    AssertTrue(0 == sigaction(SIGABRT, &act, NULL));
    AssertTrue(0 == sigaction(SIGCHLD, &act, NULL));
    AssertTrue(0 == sigaction(SIGTERM, &act, NULL));
    AssertTrue(0 == sigaction(SIGINT, &act, NULL));

    /* Ignore the SIGPIPE signal */
    struct sigaction ign = {
        .sa_handler = SIG_IGN
    };
    sigemptyset(&ign.sa_mask);
    AssertTrue(0 == sigaction(SIGPIPE, &ign, NULL));

    /* At the bottom of the stack place the generic handler */
    ListenForSignal(0, RestoreDefaultHandlerAndRaise);
}

void ListenForSignal(int signo, TSignalListener callback) {

    SListenersListNode * node = malloc(sizeof(SListenersListNode));
    AssertTrue(node);
    /* Push the new node onto the stack */
    node->Callback = callback;
    node->Signal = signo;
    node->Next = s_signalListeners;
    s_signalListeners = node;
}

static void CommonHandler(int signo, siginfo_t * siginfo, void * ucontext) {

    ucontext_t * context = (ucontext_t *) ucontext;

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

    CallListeners(signo, siginfo);
}

static void CallListeners(int signo, siginfo_t * siginfo) {

    for (SListenersListNode * iter = s_signalListeners; iter != NULL; iter = iter->Next) {

        /* Check if listener matches the signal or is a catch-all */
        if (signo == iter->Signal || iter->Signal == 0) {

            if (iter->Callback(signo, siginfo)) {

                /* Signal handled, break out of the loop */
                break;
            }
        }
    }
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
        /* Action optional */
        reason = "hardware memory error detected in process but not consumed";
        break;

    default:
        reason = "???";
        break;
    }

    LogPrint(ELogSeverityLevel_Error, "%s(): Memory bus error (%d: %s) at %p (%s) (offending address: %p)", \
        __FUNCTION__, siginfo->si_code, reason, (const void *) ucontext->uc_mcontext.pc, instruction, siginfo->si_addr);
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
        break;

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

    LogPrint(ELogSeverityLevel_Error, "%s(): Floating-point exception (%d: %s) at %p (%s) (offending address: %p)", \
        __FUNCTION__, siginfo->si_code, reason, (const void *) ucontext->uc_mcontext.pc, instruction, siginfo->si_addr);
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

    LogPrint(ELogSeverityLevel_Error, "%s(): Illegal instruction error (%d: %s) at %p (%s) (offending address: %p)", \
        __FUNCTION__, siginfo->si_code, reason, (const void *) ucontext->uc_mcontext.pc, instruction, siginfo->si_addr);
    PrintProcessInfo();
    PrintCallstack();
}

static void SigsegvHandler(siginfo_t * siginfo, ucontext_t * ucontext) {

    char instruction[OFFENDING_INSTRUCTION_BUFFER_SIZE];
    GetOffendingInstruction(ucontext, instruction, sizeof(instruction));
    const char * reason;

    switch (siginfo->si_code) {
    case SEGV_MAPERR:
        reason = "address not mapped to object";
        break;

    case SEGV_ACCERR:
        reason = "invalid permissions";
        break;

    default: /* Do not expect SEGV_BNDERR or SEGV_PKUERR */
        reason = "???";
        break;
    }

    LogPrint(ELogSeverityLevel_Error, "%s(): Segmentation violation (%d: %s) at %p (%s) (offending address: %p)", \
        __FUNCTION__, siginfo->si_code, reason, (const void *) ucontext->uc_mcontext.pc, instruction, siginfo->si_addr);
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

    /* Log the event - note that we assume SA_NOCLDSTOP was passed to sigaction,
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
    (void) prctl(PR_GET_NAME, procName, 0, 0, 0);

    LogPrint(ELogSeverityLevel_Info, "============== PROCESS INFO ==============");
    LogPrint(ELogSeverityLevel_Info, "    Name:   %s", procName);
    LogPrint(ELogSeverityLevel_Info, "    PID:    %d", getpid());
    LogPrint(ELogSeverityLevel_Info, "    PPID:   %d", getppid());
    LogPrint(ELogSeverityLevel_Info, "    errno:  %d (%s)", errno, strerror(errno));
    LogPrint(ELogSeverityLevel_Info, "==========================================");
}

static bool RestoreDefaultHandlerAndRaise(int signo, const siginfo_t * siginfo) {

    (void) siginfo;

    LogPrint(ELogSeverityLevel_Info, "Restoring default behaviour for signal %d (%s) and raising it again...", \
        signo, strsignal(signo));

    struct sigaction act = {
        .sa_handler = SIG_DFL,
    };
    AssertTrue(0 == sigaction(signo, &act, NULL));
    raise(signo);

    /* Signal has been handled at this point as this is the default handler (not that this makes a difference anyway) */
    return true;
}
