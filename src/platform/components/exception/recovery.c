#include <menabrea/exception.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>

#define RECOVERY_ACTIONS_SCRIPT_PATH  "/tmp/.recovery_actions"

void OnDisgracefulShutdown(const char * cmd, ...) {

    /* Open the recovery/cleanup script, creating it if need be */
    int fd = open(RECOVERY_ACTIONS_SCRIPT_PATH, O_WRONLY | O_APPEND | O_CREAT, S_IRWXU);
    AssertTrue(fd != -1);

    /* Write the action command to the file */
    va_list args;
    va_start(args, cmd);
    int charsWritten = vdprintf(fd, cmd, args);
    AssertTrue(0 <= charsWritten);
    va_end(args);
    charsWritten = dprintf(fd, "\n");
    AssertTrue(0 < charsWritten);

    /* Close the descriptor */
    AssertTrue(0 == close(fd));
}

void DeclareShutdownGraceful(void) {

    /* Shutdown clean, no recovery actions needed, remove the script */
    AssertTrue(0 == unlink(RECOVERY_ACTIONS_SCRIPT_PATH));
}
