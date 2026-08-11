# RTSyn SPSC

RTSyn SPSC provides fixed-size single-producer/single-consumer queues for communication between the
RTSyn realtime process and separate control/telemetry processes.

## Public API

The public API is declared in:

- `include/rtsyn/spsc/command/spsc.h`: command queue operations and POSIX shared-memory lifecycle.
- `include/rtsyn/spsc/command/message.h`: command message payloads.
- `include/rtsyn/spsc/telemetry/spsc.h`: telemetry queue operations and POSIX shared-memory lifecycle.
- `include/rtsyn/spsc/telemetry/message.h`: telemetry message payloads.

## Basic Usage

```c
#include "rtsyn/spsc/command/spsc.h"

int main(void) {
  rtsyn_spsc_command_shared_t commands;
  if (rtsyn_spsc_command_shared_create(&commands, "/rtsyn_commands") != 0) {
    return 1;
  }

  rtsyn_spsc_command_shared_close(&commands);
  rtsyn_spsc_command_shared_unlink("/rtsyn_commands");
  return 0;
}
```

## Generating Documentation

Run:

```sh
xmake doxygen
```

Then open `build/html/index.html`.
