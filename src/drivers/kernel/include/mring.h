
#ifndef DRIVERS_KERNEL_MRING_H
#define DRIVERS_KERNEL_MRING_H

struct mring_ctrl_file {
    unsigned long read_index;
    unsigned long write_index;

    unsigned long order;
    unsigned long mask;
};

#endif /* DRIVERS_KERNEL_MRING_H */
