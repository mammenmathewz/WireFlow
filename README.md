# WireFlow

A non-blocking TCP proxy written in C using Linux epoll.

Built from scratch to understand how production proxies (nginx, envoy, haproxy)
handle high-concurrency I/O at the systems level.

## Architecture

- Single-threaded epoll edge-triggered event loop
- Bidirectional client ↔ upstream byte forwarding
- Ring buffer (8KB) per connection
- O(1) fd-indexed connection table
- TCP_NODELAY on all sockets
- Graceful SIGINT shutdown

## Performance (WSL2, localhost loopback, Python echo backend)

| Concurrent clients | Throughput (req/s) | p50 latency | p99 latency |
|--------------------|-------------------|-------------|-------------|
| 1                  | 6,476             | 96μs        | 2,399μs     |
| 10                 | 13,125            | 605μs       | 2,787μs     |
| 50                 | 14,144            | 3,091μs     | 6,002μs     |
| 100                | 13,570            | 6,449μs     | 24,612μs    |

> Benchmarked with a custom Python TCP client (benchmarks/bench.py)
> against a Python echo backend (benchmarks/backend.py).
> Loopback only — real network latency will be higher.

## Build

make && ./build/wireflow

## Usage

WireFlow listens on :8080 and proxies to 127.0.0.1:9000.
Configure upstream in include/server.h:

  #define UPSTREAM_HOST "127.0.0.1"
  #define UPSTREAM_PORT 9000

## What I learned building this

- How epoll edge-triggered mode works and why you must drain to EAGAIN
- Why O_NONBLOCK is mandatory with EPOLLET
- How proxies map client_fd ↔ upstream_fd bidirectionally
- Why blocking accept() kills concurrency (built Phase 1→2→3 progressively)
- How ring buffers handle partial reads/writes without malloc