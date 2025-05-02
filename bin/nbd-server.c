// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "blkhash-config.h"
#ifdef HAVE_NBD

#define _GNU_SOURCE     /* For asprintf */

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include "blksum.h"

/* This is baked into the systemd socket activation API. */
#define FIRST_SOCKET_ACTIVATION_FD 3

/* == strlen ("LISTEN_PID=") | strlen ("LISTEN_FDS=") */
#define PREFIX_LENGTH 11

extern char **environ;

static void free_env(char **env)
{
    if (env == NULL)
        return;

    for (int i = 0; env[i] != NULL; i++)
        free(env[i]);

    free(env);
}

/*
 * Create environment for running qemu-nbd with systemd socket
 * activation. Copies current environment removing LISTEN_PID or
 * LISTEN_FDS and add new LISTEN_PID= and LISTEN_FDS=1 variables.
 */
static char **create_env(void)
{
    char **env;
    int len;

    /*
     * Allocate new env, adding room for LISTEN_* vars and NULL
     * terminator.
     */

    for (len = 0; environ[len] != NULL; len++);

    env = calloc(len + 3, sizeof(env[0]));
    if (env == NULL) {
        ERROR("calloc: %s", strerror(errno));
        return NULL;
    }

    /* Add LISTEN_* vars. */

    /* Allocate space for 64 bit pid (20 characters). */
    env[0] = strdup("LISTEN_PID=xxxxxxxxxxxxxxxxxxxx\0");
    if (env[0] == NULL) {
        ERROR("strdup: %s", strerror(errno));
        goto fail;
    }

    env[1] = strdup("LISTEN_FDS=1");
    if (env[1] == NULL) {
        ERROR("strdup: %s", strerror(errno));
        goto fail;
    }

    /*
     * Append the current environment skipping LISTEN_PID and
     * LISTEN_FDS.
     */
    for (int i = 0; i < len; i++) {
        if (strncmp(environ[i], "LISTEN_PID=", PREFIX_LENGTH) != 0 &&
            strncmp(environ[i], "LISTEN_FDS=", PREFIX_LENGTH) != 0) {
            env[i + 2] = strdup(environ[i]);
            if (env[i + 2] == NULL) {
                ERROR("strdup: %s", strerror(errno));
                goto fail;
            }
        }
    }

    return env;

fail:
    free_env(env);
    return NULL;
}

/*
 * Create temporary directory for qemu-nbd unix socket.
 *
 * Use /tmp instead of TMPDIR because we must ensure the path is
 * short enough to store in the sockaddr_un.
 */
static void create_tmpdir(struct nbd_server *s)
{
    char *path;

    path = strdup("/tmp/blksum-XXXXXX");
    if (path == NULL)
        FAIL_ERRNO("strdup");

    if (mkdtemp(path) == NULL)
        FAIL_ERRNO("mkdtemp");

    DEBUG("Created temporary directory %s", path);
    s->tmpdir = path;
}

/*
 * Remove temporary directory created in create_tmpdir. Since this is
 * called in cleanup flows, errors are logged and ignored.
 */
static void remove_tmpdir(struct nbd_server *s)
{
    if (s->sock) {
        DEBUG("Removing socket %s", s->sock);
        if (unlink(s->sock)) {
            ERROR("Cannot remove socket: %s", strerror(errno));
            return;
        }

        free(s->sock);
        s->sock = NULL;
    }

    if (s->tmpdir) {
        DEBUG("Removing temporary directory %s", s->tmpdir);
        if (rmdir(s->tmpdir)) {
            ERROR("Cannot remove tempoary directory: %s", strerror(errno));
            return;
        }

        free(s->tmpdir);
        s->tmpdir = NULL;
    }
}

/*
 * Setting listen socket buffers makes reading 8.8 times faster.  Connections
 * accepted by the listen socket inherit the socket buffers size. I tried to
 * set the buffer sizes on the NBD handle socket but it does not show any
 * effect. This should be fixed in qemu-nbd, setting the buffer size for
 * accepted sockets. Fixing it here make the fix available with older qemu-nbd
 * versions.
 *
 * This has no effect on Linux, so the change is limited to macOS. We ned to
 * test with other platforms.
 *
 * Setting socket buffer size is a performance optimization so we don't fail or
 * log user visible errors.
 */
static void set_socket_buffers(int fd)
{
#if __APPLE__
    const int value = 2 * 1024 * 1024;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value)) < 0) {
        DEBUG("setsockopt: %s", strerror(errno));
    }
#else
    (void)fd; /* Silence unused argument warning. */
#endif
}

/*
 * Create a listening socket for passing to qemu-nbd via systemd socket
 * activation. Using socket activavation solves the issue of waiting until
 * qemu-nbd is listening on the socket, and issues with limited backlog on
 * qemu-nbd created listen socket.
 */
static int create_listening_socket(struct nbd_server *s)
{
    int fd = -1;
    struct sockaddr_un addr;

    if (asprintf(&s->sock, "%s/sock", s->tmpdir) == -1) {
        ERROR("asprintf: %s", strerror(errno));
        return -1;
    }

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        ERROR("socket: %s", strerror(errno));
        goto fail;
    }

    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, s->sock, strlen(s->sock) + 1);

    set_socket_buffers(fd);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        ERROR("bind: %s", strerror(errno));
        goto fail;
    }

    if (listen(fd, SOMAXCONN) == -1) {
        ERROR("listen: %s", strerror(errno));
        goto fail;
    }

    DEBUG("Listening on %s", s->sock);

    return fd;

fail:
    free(s->sock);
    s->sock = NULL;

    if (fd != -1)
        close(fd);

    return -1;
}

static int set_cloexec(int fd, bool on)
{
    int flags = fcntl(fd, F_GETFD, 0);
    if (flags == -1)
        return -1;

    if (on)
        flags |= FD_CLOEXEC;
    else
        flags &= ~FD_CLOEXEC;

    if (fcntl(fd, F_SETFD, flags) == -1)
        return -1;

    return 0;
}

/*
 * Redirect stderr to /dev/null, and return the previous stderr fd so it
 * can be restored later.
 */
static int suppress_stderr()
{
    int saved, devnull;

    saved = dup(STDERR_FILENO);
    if (saved != -1)
        set_cloexec(saved, true);

    devnull = open("/dev/null", O_WRONLY);
    if (devnull == -1) {
        ERROR("Cannot open /dev/null for writing: %s", strerror(errno));
        close(saved);
        return -1;
    }

    dup2(devnull, STDERR_FILENO);
    close(devnull);

    return saved;
}

static void setup_signals(void)
{
    sigset_t blocked;

    /* Reset signals. */
    signal(SIGPIPE, SIG_DFL);

    /* Block SIGINT for clean shutdown on signals. */
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigprocmask(SIG_BLOCK, &blocked, NULL);
}

static void exec_qemu_nbd(int fd, char **env, struct server_options *opt)
{
    const char *cache = opt->cache ? "writeback" : "none";
    int saved_stderr = -1;

    char *const argv[] = {
        "qemu-nbd",
        "--read-only",
        "--persistent",
        "--cache", (char *)cache,
        "--aio", (char *)opt->aio,
        "--shared", "0",
        "--format", (char *)opt->format,
        (char *)opt->filename,
        NULL
    };

    /* Make sure listening socket is FIRST_SOCKET_ACTIVATION_FD -
     * required by systemd socket activation API. */

    if (fd != FIRST_SOCKET_ACTIVATION_FD) {
        /* dup2 does not set close-on-exec flag */
        dup2(fd, FIRST_SOCKET_ACTIVATION_FD);
        close(fd);
    } else {
        /* Unset close-on-exec flag on fd. */
        if (set_cloexec(fd, false))
            FAIL_ERRNO("set_cloexec");
    }

    /* Update LISTEN_PID= with child pid. */
    snprintf(env[0] + PREFIX_LENGTH, strlen(env[0]) - PREFIX_LENGTH, "%d",
             getpid());

    setup_signals();

    if (!debug)
        saved_stderr = suppress_stderr();

    /* execvpe is not available in macOS, so we need to pass the environment
     * using the environ global. */
    environ = env;

    execvp(argv[0], argv);

    /* execvpe failed. */

    if (saved_stderr != -1)
        dup2(saved_stderr, STDERR_FILENO);

    ERROR("execvpe: %s", strerror(errno));
    if (errno == ENOENT)
        _exit(127);
    else
        _exit(126);
}

static bool qemu_can_use_direct_io(const char *filename)
{
/* QEMU uses O_DSYNC if O_DIRECT isn't available. */
#ifndef O_DIRECT
#define O_DIRECT O_DSYNC
#endif
    int fd = open(filename, O_RDONLY | O_DIRECT);
    if (fd != -1)
        close(fd);
    return fd != -1;
}

void optimize_for_nbd_server(const char *filename, struct options *opt,
                             struct file_info *fi)
{
    if (fi->fs_name && strcmp(fi->fs_name, "nfs") == 0) {
        /*
         * cache=false aio=native can be up to 1180% slower. cache=false
         * aio=threads can be 36% slower, but may be wanted to avoid
         * polluting the page cache with data that is not going to be
         * used.
         */
        if (strcmp(opt->aio, "native") == 0) {
            opt->aio = "threads";
            DEBUG("Optimize for 'nfs': aio=threads");
        }

        /*
         * For raw format large queue and read sizes can be 2.5x times
         * faster. For qcow2, the default values give best performance.
         */
        if (strcmp(fi->format, "raw") == 0) {
            /* If user did not specify read size, use larger value. */
            if ((opt->flags & USER_READ_SIZE) == 0) {
                opt->read_size = 2 * 1024 * 1024;
                DEBUG("Optimize for 'raw' image on 'nfs': read_size=%ld",
                      opt->read_size);

                /* If user did not specify queue depth, adapt queue size to
                 * read size. */
                if ((opt->flags & USER_QUEUE_DEPTH) == 0) {
                    opt->queue_depth = 4;
                    DEBUG("Optimize for 'raw' image on 'nfs': queue_depth=%ld",
                          opt->queue_depth);
                }
            }

        }
    } else {
        /*
         * For other storage, direct I/O is required for correctness on
         * some cases (e.g. LUN connected to multiple hosts), and typically
         * faster and more consistent. However it is not supported on all
         * file systems so we must check if file can be used with direct
         * I/O.
         */
        if (!opt->cache && !qemu_can_use_direct_io(filename)) {
            opt->cache = true;
            DEBUG("Optimize for '%s' image on '%s': cache=yes",
                  fi->format, fi->fs_name);
        }

        /* Cache is not compatible with aio=native. */
        if (opt->cache && strcmp(opt->aio, "native") == 0) {
            opt->aio = "threads";
            DEBUG("Optimize for '%s' image on '%s': aio=threads",
                  fi->format, fi->fs_name);
        }
    }
}

struct nbd_server *start_nbd_server(struct server_options *opt)
{
    struct nbd_server *s;
    int fd = -1;
    char **env = NULL;
    pid_t pid = -1;

    s = calloc(1, sizeof(*s));
    if (s == NULL)
        FAIL_ERRNO("calloc");

    create_tmpdir(s);

    fd = create_listening_socket(s);
    if (fd == -1)
        goto fail;

    env = create_env();
    if (env == NULL)
        goto fail;

    pid = fork();
    if (pid == -1) {
        ERROR("fork: %s", strerror(errno));
        goto fail;
    }

    if (pid == 0) {
        /* Child - does not return. */
        exec_qemu_nbd(fd, env, opt);
    }

    /* Parent. */
    DEBUG("Started qemu-nbd pid=%d", pid);
    s->pid = pid;
    free_env(env);
    close(fd);

    return s;

fail:
    if (env != NULL)
        free_env(env);

    if (fd != -1)
        close(fd);

    remove_tmpdir(s);
    free(s);

    exit(1);
}

char *nbd_server_uri(struct nbd_server *s)
{
    char *uri;

    if (asprintf(&uri, "nbd+unix:///?socket=%s", s->sock) == -1)
        FAIL_ERRNO("asprintf");

    return uri;
}

void stop_nbd_server(struct nbd_server *s)
{
    DEBUG("Terminating qemu-nbd pid=%d", s->pid);
    kill(s->pid, SIGKILL);

    if (waitpid(s->pid, NULL, 0) == -1)
        DEBUG("Error waiting for qemu-nbd: %s", strerror(errno));

    s->pid = -1;

    remove_tmpdir(s);
}

#endif /* HAVE_NBD */
