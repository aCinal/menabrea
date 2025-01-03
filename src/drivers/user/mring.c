#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdlib.h>

#include "include/mring.h"
#include <mring.h>

struct mring {
    struct mring_ctrl_file *ctrl;
    unsigned char *buff;
    unsigned long size;
};

static struct mring_ctrl_file *open_ctrl_file(const char *filename);
static unsigned char *open_buffer(const char *filename, unsigned long size);

struct mring *mring_open(void)
{
    struct mring *mring;
    char ctrl_filename[32];
    char buff_filename[32];
    int cpuid = sched_getcpu();
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size < 0)
        return NULL;

    (void) snprintf(ctrl_filename, sizeof(ctrl_filename), "/dev/mring%d_ctrl", cpuid);
    (void) snprintf(buff_filename, sizeof(buff_filename), "/dev/mring%d_buff", cpuid);

    mring = malloc(sizeof(struct mring));
    if (!mring)
        return NULL;

    mring->ctrl = open_ctrl_file(ctrl_filename);
    if (!mring->ctrl)
        goto error;
    mring->size = (page_size << mring->ctrl->order);
    mring->buff = open_buffer(buff_filename, mring->size);
    if (!mring->buff)
        goto error;

    return mring;

error:
    if (mring->ctrl)
        (void) munmap(mring->ctrl, sizeof(*mring->ctrl));
    free(mring);
    return NULL;
}

void mring_close(struct mring *mring)
{
    (void) munmap(mring->buff, mring->size);
    (void) munmap(mring->ctrl, sizeof(*mring->ctrl));
    free(mring);
}

#define READ_ONCE(var)          ( *(volatile typeof(var) *)(&(var)) )
#define WRITE_ONCE(var, data)     *(volatile typeof(var) *)(&(var)) = (data)

unsigned char *mring_read_begin(struct mring *mring, unsigned long *readable)
{
    unsigned long read_index = mring->ctrl->read_index;
    unsigned long write_index = READ_ONCE(mring->ctrl->write_index);
    *readable = (write_index - read_index) & mring->ctrl->mask;
    return &mring->buff[read_index];
}

void mring_read_end(struct mring * mring, unsigned long bytes_read)
{
    unsigned long read_index = mring->ctrl->read_index;
    unsigned long new_read_index = (read_index + bytes_read) & mring->ctrl->mask;
    WRITE_ONCE(mring->ctrl->read_index, new_read_index);
}

static struct mring_ctrl_file *open_ctrl_file(const char *filename)
{
    struct mring_ctrl_file *ctrl;
    /* Open the control file */
    int fd = open(filename, O_RDWR);
    if (fd < 0)
        return NULL;

    /* Map the file */
    ctrl = mmap(
        NULL,
        sizeof(*ctrl),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    /* Close the descriptor no matter what the status is */
    (void) close(fd);

    if (ctrl == MAP_FAILED)
        return NULL;

    return ctrl;
}

static unsigned char *open_buffer(const char *filename, unsigned long size)
{
    void *anonymous_area = MAP_FAILED;
    void *first_mapping = MAP_FAILED;
    void *second_mapping;
    size_t anon_remaining = 2 * size;
    int fd;

    /* Find suitably-sized area to accommodate two copies
     * of the ringbuffer back to back */
    anonymous_area = mmap(
        NULL,
        anon_remaining,
        PROT_NONE,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );
    if (anonymous_area == MAP_FAILED)
        return NULL;

    /* Open the file */
    fd = open(filename, O_RDONLY);
    if (fd < 0)
        goto error;

    /* Map the ringbuffer once over the first half of the reserved area */
    first_mapping = mmap(
        anonymous_area,
        size,
        PROT_READ,
        MAP_SHARED | MAP_FIXED,
        fd,
        0
    );
    if (first_mapping == MAP_FAILED)
        goto error;
    /* The anonymous memory area has just shrunk */
    anonymous_area += size;
    anon_remaining -= size;

    /* Map the ringbuffer again over the second half */
    second_mapping = mmap(
        anonymous_area,
        size,
        PROT_READ,
        MAP_SHARED | MAP_FIXED,
        fd,
        0
    );
    if (second_mapping == MAP_FAILED)
        goto error;

    (void) close(fd);
    return first_mapping;

error:
    if (anon_remaining)
        (void) munmap(anonymous_area, anon_remaining);
    if (first_mapping != MAP_FAILED)
        (void) munmap(first_mapping, size);
    if (fd >= 0)
        (void) close(fd);
    return NULL;
}
