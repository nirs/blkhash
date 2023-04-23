// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "event.h"

#ifdef HAVE_EVENTFD

static int init_event(struct event *e, int flags)
{
    int fd = eventfd(0, flags);
    if (fd == -1)
        return -errno;

    e->read_fd = fd;
    e->write_fd = fd;
    return 0;
}

#else

static void set_flags(int fd, int flags)
{
    int cur = fcntl(fd, F_GETFL, 0);
    if (cur == -1)
        return -errno;

    if (fcntl(fd, F_SETFL, cur | flags))
        return -errno;

    return 0;
}

static int init_event(struct event *e, int flags)
{
    int fds[2];
    int err;

    if (pipe(fds))
        return -errno;

    if (set_flags(fds[0], flags))
        goto error;

    if (set_flags(fds[1], flags))
        goto error;

    e->read_fd = fds[0];
    e->write_fd = fds[1];
    return 0;

error:
    err = -errno;
    close(fds[0]);
    close(fds[1]);
    return err;
}

#endif /* HAVE_EVENTFD */

int event_open(struct event **ep, int flags)
{
    struct event *res;
    int err;

    res = malloc(sizeof(*res));
    if (res == NULL)
        return -errno;

    err = init_event(res, flags);
    if (err) {
        free(res);
        return err;
    }

    *ep = res;
    return 0;
}

int event_signal(struct event *e)
{
#ifdef HAVE_EVENTFD
    uint64_t event = 1;
#else
    char event = 1;
#endif
    ssize_t n;

    do {
        n = write(e->write_fd, &event, sizeof(event));
    } while (n == -1 && errno == EINTR);

    if (n < 0) {
        if (errno != EAGAIN)
            return -errno;

        /* EAGAIN: file is non-bloking and value will overflow (linux), or room
         * to write to the pipe. In both cases it means the event fd is already
         * signaled so we are ok. */
        return 0;
    }

#ifdef HAVE_EVENTFD
    assert(n == sizeof(event));
#endif

    return 0;
}

int event_wait(struct event *e)
{
#ifdef HAVE_EVENTFD
    uint64_t event;
#else
    uint8_t event[128];
#endif
    ssize_t n;

    do {
        n = read(e->read_fd, &event, sizeof(event));
    } while (n == -1 && errno == EINTR);

    if (n < 0) {
        if (errno != EAGAIN)
            return -errno;

        /* EAGAIN: the file descriptor is non-blocking and no singal was
         * recevied yet. */
        return 0;
    }

#ifdef HAVE_EVENTFD
    assert(n == sizeof(event));
    return 1;
#else
    return n > 0 ? 1 : 0;
#endif
}

void event_close(struct event *e)
{
    if (e == NULL)
        return;

    close(e->read_fd);
#ifndef HAVE_EVENTFD
    close(e->write_fd);
#endif
    free(e);
}
