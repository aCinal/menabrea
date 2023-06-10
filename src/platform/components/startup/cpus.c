#define _GNU_SOURCE
#include <startup/cpus.h>
#include <menabrea/exception.h>
#include <sched.h>
#include <sys/sysinfo.h>

int ClaimAllCpus(void) {

    cpu_set_t mask;
    CPU_ZERO(&mask);
    /* Get the number of cores available in the system */
    int cores = get_nprocs();
    /* Enable running on all cores */
    for (int i = 0; i < cores; i++) {

        CPU_SET(i, &mask);
    }
    AssertTrue(0 == sched_setaffinity(0, sizeof(cpu_set_t), &mask));

    /* Return the number of cores to the caller */
    return cores;
}

void PinCurrentProcessToCpu(int cpu) {

    cpu_set_t cpuMask;
    CPU_ZERO(&cpuMask);
    CPU_SET(cpu, &cpuMask);
    /* Pin to a given core */
    AssertTrue(0 == sched_setaffinity(0, sizeof(cpu_set_t), &cpuMask));
}
