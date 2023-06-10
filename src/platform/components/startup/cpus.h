
#ifndef PLATFORM_COMPONENTS_STARTUP_CPUS_H
#define PLATFORM_COMPONENTS_STARTUP_CPUS_H

int ClaimAllCpus(void);
void PinCurrentProcessToCpu(int cpu);

#endif /* PLATFORM_COMPONENTS_STARTUP_CPUS_H */
