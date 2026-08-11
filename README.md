# RTSyn SPSC

Lock-free single-producer/single-consumer queues used to connect the realtime process with
non-realtime control/telemetry processes.

The queue operations are non-blocking and allocate no memory. For communication between different
processes, create the queue with the shared-memory helpers in this package instead of allocating the
queue with `malloc`.

## Shared Memory Usage

Command queue, control process produces and realtime process consumes:

```c
rtsyn_spsc_command_shared_t commands;

if (rtsyn_spsc_command_shared_create(&commands, "/rtsyn_commands") != 0) {
  return 1;
}

/* A different process opens the same name. */
rtsyn_spsc_command_shared_t realtime_commands;
if (rtsyn_spsc_command_shared_open(&realtime_commands, "/rtsyn_commands") != 0) {
  return 1;
}

rtsyn_spsc_command_message_t msg = {0};
rtsyn_spsc_command_try_push(commands.queue, &msg);
rtsyn_spsc_command_try_pop(realtime_commands.queue, &msg);

rtsyn_spsc_command_shared_close(&realtime_commands);
rtsyn_spsc_command_shared_close(&commands);
rtsyn_spsc_command_shared_unlink("/rtsyn_commands");
```

Telemetry queue uses the same lifecycle with `rtsyn_spsc_telemetry_shared_*`.

Create exactly one producer and one consumer per queue. `*_shared_create` initializes a new POSIX
shared-memory object with `O_CREAT | O_EXCL`; `*_shared_open` maps an existing one without resetting
it. Call `*_shared_unlink` once during shutdown/cleanup after all processes have opened or closed
their mappings.

## Usage

### Update

Make sure you have last version of the dependencies:

```bash
xrepo update-repo
xmake require --upgrade
```

For development you may need to run:

```bash
xmake require --upgrade -fy <dependency_name>
```

### Compiling

For compiling:

```bash
xmake
```

### Tests

For running test:

```bash
xmake test
```

For enabling valgrind, before running tests:

```bash
xmake f --valgrind=y
```

For disabling valgrind, just replace `y` for `n`.

### Local development

If you want to test your changes locally from different parts of RTSyn, export the path where you have all the repos:

```bash
export RTSYN_WORKSPACE=<PATH>
```

> [!WARNING]
> This expects you also the `rtsyn-xmake-repo`.

### Cleaning

To remove all generated build artifacts:

```bash
xmake clean --all
```

To also reset cached configuration and tool detection:

```bash
xmake f -c
```
