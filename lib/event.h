// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef EVENT_H
#define EVENT_H

#if defined (__linux__) || defined (__FreeBSD__)
    #include <sys/eventfd.h>
    #define EVENT_CLOEXEC EFD_CLOEXEC
    #define EVENT_NONBLOCK EFD_NONBLOCK
    #define HAVE_EVENTFD 1
#else
    #include <unistd.h>
    #include <fcntl.h>
    #define EVENT_CLOEXEC O_CLOEXEC
    #define EVENT_NONBLOCK O_NONBLOCK
    #undef HAVE_EVENTFD
#endif

struct event {
    int read_fd;
    int write_fd;
};

/*
 * Open an event fd. Return 0 on success and negative errno value on errors.
 */
int event_open(struct event **ep, int flags);

/*
 * Signal the event fd, waking up the thread waiting on event_wait(). Return
 * 0 on success and negative errno value on errors.
 */
int event_signal(struct event *e);

/*
 * Wait until the event fd is signalsed. Return 1 if a signal was received, 0
 * if the EVENT_NONBLOCK was specified and no signal was received yet, and
 * -errno value on errors.
 */
int event_wait(struct event *e);

void event_close(struct event *e);

#endif /* EVENT_H */
