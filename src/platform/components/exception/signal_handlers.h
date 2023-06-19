
#ifndef PLATFORM_COMPONENTS_EXCEPTION_SIGNAL_HANDLERS_H
#define PLATFORM_COMPONENTS_EXCEPTION_SIGNAL_HANDLERS_H

#include <signal.h>
#include <stdbool.h>

/**
 * @typedef Define a listener for a given signal
 * @note Return true to indicate the signal has been handled and the default
 *       behaviour should not be triggered
 */
typedef bool (* TSignalListener)(int signo, const siginfo_t * siginfo);

void InstallSignalHandlers(void);
void ListenForSignal(int signo, TSignalListener callback);

#endif /* PLATFORM_COMPONENTS_EXCEPTION_SIGNAL_HANDLERS_H */
