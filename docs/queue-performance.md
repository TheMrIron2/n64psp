# Message Queue Hot Path

The lifetime-hardened queue implementation used a conservative registry and
counting-semaphore model. One successful uncontended send/jam/receive performed:

| Operation | Registry mutex | Queue mutex | Queue semaphores | PSP sync calls |
| --- | ---: | ---: | ---: | ---: |
| `osSendMesg(..., OS_MESG_NOBLOCK)` | 4 | 2 | 2 | 8 |
| `osSendMesg(..., OS_MESG_BLOCK)`, space available | 4 | 2 | 2 | 8 |
| `osRecvMesg(..., OS_MESG_NOBLOCK)` | 4 | 2 | 2 | 8 |
| `osRecvMesg(..., OS_MESG_BLOCK)`, item available | 4 | 2 | 2 | 8 |

On PSP those calls map to semaphore wait/poll/signal operations. That is too
expensive for SF64's high-frequency queue traffic.

The optimized queue path keeps the registry mutex out of ordinary queue
operations. A normal successful send/jam/receive now performs:

| Operation | Registry mutex | Queue semaphores | PSP semaphore calls | PSP critical section |
| --- | ---: | ---: | ---: | ---: |
| `osSendMesg(..., OS_MESG_NOBLOCK)` | 0 | 0 | 0 | 1 enter/leave pair |
| `osSendMesg(..., OS_MESG_BLOCK)`, space available | 0 | 0 | 0 | 1 enter/leave pair |
| `osRecvMesg(..., OS_MESG_NOBLOCK)` | 0 | 0 | 0 | 1 enter/leave pair |
| `osRecvMesg(..., OS_MESG_BLOCK)`, item available | 0 | 0 | 0 | 1 enter/leave pair |

The PSP critical section is implemented with interrupt suspend/resume and is
used only around direct ring-buffer state checks and mutation. It must not
contain logging, allocation, semaphore waits/signals, or other blocking work.

## Admission

Queue operations first check global admission, find a published state, acquire
an atomic active reference, then revalidate global admission, state admission,
state generation, queue identity, and public queue fields. Shutdown closes
global admission before checking active references. Reinitialisation closes
per-queue admission before checking active references. If a racing operation
already has a reference, shutdown/reinitialisation returns `N64PSP_ERROR_BUSY`;
otherwise the operation observes closed admission or a generation mismatch and
returns `-1` through the libultra-compatible API.

## Waiters

Semaphores are notification objects only. A blocking sender or receiver records
itself as a waiter while holding the queue critical section, exits the critical
section, then waits. A producer or consumer consumes one recorded waiter before
signaling the corresponding semaphore. That closes the lost-wakeup race,
including signal-before-wait, and prevents stale semaphore tokens from
accumulating when the queue later drains.

## Diagnostics And Benchmarks

Queue counters and timed benchmarks are opt-in diagnostics. Ordinary library
builds and ordinary PSP smoke builds do not collect counters, dump counters, or
run timed benchmarks. Enable counters with `N64PSP_QUEUE_COUNTERS=1`. Enable the
PSP smoke timed benchmarks with `N64PSP_PSP_BENCHMARKS=1`.

The host benchmark target is:

```sh
make N64PSP_QUEUE_COUNTERS=1 benchmark-host
```

The PSP smoke benchmark build is:

```sh
make N64PSP_QUEUE_COUNTERS=1 N64PSP_PSP_BENCHMARKS=1 psp
```

Counter values are diagnostic aggregates. They are maintained as 32-bit atomic
values on PSP and widened when read through `n64psp_queue_get_counters()`, so
very long counter-enabled runs can wrap.

## Current Validation Notes

Initial physical PSP measurements for the optimized queue path showed the
standalone uncontended queue benchmark around 1.2-1.4 million operations per
second, and the capacity-one two-thread blocking ping-pong benchmark around
115,000 queue operations per second.

In the SF64 integration, aggregate title-screen counters were coherent:
block/wake/retry ratios were approximately 1:1:1 for the blocking traffic that
actually slept, spurious wake-ups stayed at zero, and observed receiver waiter
depth stayed at one. This indicates that stale notification tokens and lost
wake-ups were not the source of the remaining performance investigation.

The same SF64 EBOOT behaved much worse in PPSSPP than on physical PSP hardware
despite healthy queue counters. Treat that as an emulator-specific scheduling
or timing discrepancy until proven otherwise; it is not currently evidence of a
generic queue correctness bug.
