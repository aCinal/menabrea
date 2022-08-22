#include <menabrea/cores.h>
#include <event_machine.h>

int GetCurrentCore(void) {

    return em_core_id();
}

int GetCurrentCoreMask(void) {

    return (1 << em_core_id());
}

int GetSharedCoreMask(void) {

    /* Core 0 is always shared */
    return 0b1;
}

int GetIsolatedCoresMask(void) {

    int cores = em_core_count();
    /* Start with all bits set and shift them left by the number
     * of cores in the system. This will result in 'cores' rightmost
     * bits being zero. */
    int invertedMask = (-1) << cores;
    /* Add the LSB to the inverted mask */
    invertedMask |= 0b1;
    /* Invert the mask to be left only with the isolated cores */
    return ~invertedMask;
}
