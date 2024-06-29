<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# The blkhash library

The `blkhash` C library implements the block based hash construction, using
zero detection and multiple threads to speed up the computation.

The library provides 2 APIs: the simple API and the async API.

## The simple API

The simple API is similar to other cryptographic libraries, providing an
easy to use way to create a hash, update it with data and get the
digest.

### Creating a hash

Create a hash using the default options:

```C
struct blkhash *h = blkhash_new();
```

### Adding data to a hash

Add data to the hash:

```C
unsigned char *buf = malloc(BUF_SIZE);
memset(buf, 'A', BUF_SIZE);

blkhash_update(h, buf, BUF_SIZE);
```

The `blkhash_update()` function uses multiple threads and zero detection
to speed up hashing.

When you know that some areas of the image are unallocated (read as
zeros), you can add the unallocated range to the hash without reading
anything from storage:

```C
blkhash_zero(h, 1024 * 1024 * 1024);
```

This can be 3 orders of magnitude faster compared with
`blkhash_update()`.

### Finalizing a hash

When done, finalize the hash to get the digest:

```C
unsigned char md[BLKHASH_MAX_MD_SIZE];
unsigned int len;

blkhash_final(h, md, &len);
```

To free the hash use:

```C
blkhash_free(h);
```

See the [example program](../man/example.c) for example of using the
simple API.

## The async API

The async API provides the best possible performance by keeping multiple
requests in flight and getting events when requests are complete,
without copying the image data.

### Creating a hash

To use the async API, create a `blkhash_opts` and set the `queue_depth`
to number of inflight requests, and optionally the number of threads.

```C
struct blkhash_opts *opts = blkhash_opts_new(“sha256”);
blkhash_opts_set_queue_depth(opts, 16);
blkhash_opts_set_threads(opts, 32);
```

Create a hash with the `blkhash_opts` and free it:

```C
struct blkhash *hash = blkhash_new_opts(opts);
blkhash_opts_free(opts);
```

Finally get the completion file descriptor and start watching for
events. In this example we use `poll()` and we watch `POLLIN` events.

```C
poll_fds[0].fd = blkhash_completion_fd(hash);
poll_fds[0].events = POLLIN;
```

### Submitting requests

When a reading a buffer from storage completes, you submit the buffer
to blkhash for async update:

```C
struct request *req;

while ((req = next_completed_read())) {
    req->completed = false;
    blkhash_aio_update(hash, req->buffer, req->length, req);
}
```

`blkhash_aio_update()` adds the buffer to workers queues and returns
quickly before the data is processed. You must not modify or free the
buffered passed to `blkhash_aio_update()` until the async update
completes.

To find the completed request when processing completions, we pass a
pointer to the request struct.

### Waiting for completions

When all update request were submitted, you need to wait for update
completion. In this example we use `poll()` to wait until the completion
fd becomes readable:

```C
poll(poll_fds, ARRAY_SIZE(poll_fds), -1);

if (poll_fds[0].fd & POLLIN) {
    /* Read at least 8 bytes from the completion fd. */
    char sink[queue_depth < 8 ? 8 : queue_depth];
    read(poll_fd[0].fd, &sink, sizeof(sink));

    /* One or more requests have completed! */
    process_completions();
}
```

Note that you must read at least 8 bytes from the completion fd. On
`Linux` blkhash uses `eventfd`, so reading will always return 8 bytes.
When `eventfd` is not available it uses `pipe` and read may return up to
queue depth bytes (set by `blkhash_opts_set_queue_depth()`).

After we read the pending data from the completion fd, we need to
process the completions.

### Processing completions

We get the completions using `blkhash_aio_completions()`, and then we
iterate on the completions, fetch the completed request and complete it.

```C
struct blkhash_completion completions[queue_depth];
int count;

count = blkhash_aio_completions(hash, completions, queue_depth);

for (int i = 0; i < count; i++) {
    struct request *req = &completions[i].user_data;
    complete_request(req);
}
```

The request is the `user_data` pointer we passed to
`blkhash_aio_update()` before.

At this point `blkhash` finished to read the data and we can free the
request or use it to read more data from storage.

### Submitting zero requests

`blkhash_zero` is fast and does not access image data, so there is no
`blkhash_aio_zero()` function. You can safely call `blkhash_zero()` in
your IO event loop.

### Example code

See the [async example program](../man/aio-example.c) for example of
using the async API.

## Manual page

See [blkhash(3)](../man/blkhash.3.adoc).
