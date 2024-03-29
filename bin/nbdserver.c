// SPDX-FileCopyrightText: Red Hat Inc
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "blkhash-config.h"
#ifdef HAVE_NBD

#define _GNU_SOURCE     /* For asprintf */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>

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

    execvpe(argv[0], argv, env);

    /* execvpe failed. */

    if (saved_stderr != -1)
        dup2(saved_stderr, STDERR_FILENO);

    ERROR("execvpe: %s", strerror(errno));
    if (errno == ENOENT)
        _exit(127);
    else
        _exit(126);
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
