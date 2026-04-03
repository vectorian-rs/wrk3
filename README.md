# wrk3

A modern HTTP benchmarking tool with **constant throughput** and **accurate latency measurement**.

wrk3 generates a fixed request rate and measures latency from when each request _should_ have been sent, not when it _was_ sent. This avoids [Coordinated Omission](https://www.scylladb.com/2021/04/22/on-coordinated-omission/) — a flaw present in most load generators that causes them to dramatically underreport tail latency when the server slows down.

## What makes wrk3 different

Most HTTP benchmarking tools (ab, hey, wrk, vegeta in some modes) send the next request only after the previous response arrives. When a server stalls for 1 second, the load generator pauses too — and the resulting latency histogram misses all the requests that _would have_ been waiting during that stall.

wrk3 fixes this by:

1. **Constant throughput**: you specify exact requests/sec with `-R`, and wrk3 maintains that rate regardless of server response time
2. **Corrected latency**: response latency is measured from the _planned_ send time, so a 1-second server stall correctly shows up as 1+ second latency for all affected requests
3. **HdrHistogram recording**: full-resolution histograms capture latency accurately to the 99.9999th percentile

The result: wrk3 shows you what your users actually experience, not an optimistic fiction.

## Changes from wrk2

wrk3 is a fork of [wrk2](https://github.com/giltene/wrk2) (which itself is a fork of [wrk](https://github.com/wg/wrk)) with the following changes:

- **TLS**: replaced OpenSSL with [s2n-tls](https://github.com/aws/s2n-tls) + [AWS-LC](https://github.com/aws/aws-lc)
- **Build system**: replaced Make with CMake + Ninja
- **Platform support**: native ARM64 (Apple Silicon) and x86_64 support
- **Dependencies**: all dependencies are fetched and built automatically (no system OpenSSL required)

## Building

### Prerequisites

- CMake 3.14+
- Ninja
- A C/C++ compiler (clang or gcc)
- Make (for the bundled LuaJIT build)
- Go (for AWS-LC code generation)
- Perl (for AWS-LC code generation)

On macOS:

```bash
brew install cmake ninja
```

On Ubuntu/Debian:

```bash
sudo apt install cmake ninja-build build-essential golang perl
```

### Build

```bash
git clone https://github.com/l1x/wrk3.git
cd wrk3
cmake -G Ninja -B build
ninja -C build
```

The first build takes a few minutes as it fetches and compiles AWS-LC, s2n-tls, and LuaJIT. Subsequent builds are fast.

The binary is at `build/wrk`.

## Usage

```
wrk <options> <url>
```

### Options

| Flag                       | Description                                                   |
| -------------------------- | ------------------------------------------------------------- |
| `-t, --threads <N>`        | Number of threads (default: 2)                                |
| `-c, --connections <N>`    | Total connections to keep open (default: 10)                  |
| `-d, --duration <T>`       | Duration of test (default: 10s)                               |
| `-R, --rate <N>`           | **Required.** Target throughput in requests/sec (total)       |
| `-L, --latency`            | Print detailed latency percentiles (HdrHistogram)             |
| `-U, --u_latency`          | Print uncorrected latency (shows Coordinated Omission effect) |
| `-H, --header <H>`         | Add a custom header (repeatable)                              |
| `-s, --script <S>`         | Load a Lua script                                             |
| `-B, --batch_latency`      | Measure latency per batch instead of per request              |
| `-N, --lua-dont-pass-body` | Don't pass response body to Lua (improves performance)        |
| `--timeout <T>`            | Socket/request timeout (default: 2s)                          |
| `-v, --version`            | Print version                                                 |

Time values accept units: `2s`, `2m`, `2h`. Numeric values accept SI suffixes: `1k`, `1M`, `1G`.

### Examples

**Basic benchmark at 2000 req/sec:**

```bash
wrk -t2 -c100 -d30s -R2000 http://127.0.0.1:8080/
```

**With detailed latency histogram:**

```bash
wrk -t4 -c200 -d60s -R5000 --latency https://api.example.com/health
```

**Compare corrected vs uncorrected latency (demonstrates Coordinated Omission):**

```bash
wrk -t2 -c100 -d30s -R2000 --u_latency http://127.0.0.1:8080/
```

**POST with custom headers:**

```bash
wrk -t2 -c50 -d10s -R500 -H "Authorization: Bearer token" -s post.lua https://api.example.com/data
```

### Sample output

```
Running 30s test @ http://127.0.0.1:80/index.html
  2 threads and 100 connections
  Thread calibration: mean lat.: 9747 usec, rate sampling interval: 21 msec
  Thread calibration: mean lat.: 9631 usec, rate sampling interval: 21 msec
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.46ms    1.93ms  12.34ms   67.66%
    Req/Sec     1.05k     1.12k    2.50k    64.84%
  60017 requests in 30.01s, 19.81MB read
Requests/sec:   2000.15
Transfer/sec:    676.14KB
```

With `--latency`, you also get the full HdrHistogram percentile distribution:

```
  Latency Distribution (HdrHistogram - Recorded Latency)
   50.000%    6.67ms
   75.000%    7.78ms
   90.000%    9.14ms
   99.000%   11.18ms
   99.900%   12.30ms
   99.990%   12.45ms
   99.999%   12.50ms
  100.000%   12.50ms
```

## Lua scripting

wrk3 supports Lua scripts for custom request generation, response processing, and reporting via LuaJIT.

### API

```lua
-- Lifecycle hooks
function init(args)     -- called once per thread at startup
function request()      -- called for each request, returns HTTP request string
function response(status, headers, body)  -- optional, called for each response
function done(summary, latency, requests) -- optional, called once with results

-- Request builder
wrk = {
    scheme  = "http",
    host    = "localhost",
    port    = nil,
    method  = "GET",
    path    = "/",
    headers = {},
    body    = nil,
}

function wrk.format(method, path, headers, body)
-- Returns a formatted HTTP request string

-- Statistics objects (available in done())
latency.min              -- minimum value seen
latency.max              -- maximum value seen
latency.mean             -- average value seen
latency.stdev            -- standard deviation
latency:percentile(99.0) -- 99th percentile value

summary = {
    duration = N,  -- run duration in microseconds
    requests = N,  -- total completed requests
    bytes    = N,  -- total bytes received
    errors   = {
        connect = N, -- total socket connection errors
        read    = N, -- total socket read errors
        write   = N, -- total socket write errors
        status  = N, -- total HTTP status codes > 399
        timeout = N, -- total request timeouts
    },
}
```

### Example scripts

Scripts are in the `scripts/` directory:

| Script                                      | Description                                               |
| ------------------------------------------- | --------------------------------------------------------- |
| `addr.lua`                                  | Assign a random resolved server address to each thread    |
| `auth.lua`                                  | Add authentication headers                                |
| `cache-stresser.lua`                        | Stress-test download caches (CDN) across multiple servers |
| `counter.lua`                               | Count requests per thread                                 |
| `multi-server.lua`                          | Load-test multiple servers with multiple paths            |
| `multiple-endpoints.lua`                    | Rotate through multiple URL paths                         |
| `multiple-endpoints-prometheus-metrics.lua` | Multi-endpoint with Prometheus output                     |
| `pipeline.lua`                              | HTTP pipelining (multiple requests per connection)        |
| `post.lua`                                  | Send POST requests with a body                            |
| `report.lua`                                | Custom report formatting                                  |
| `setup.lua`                                 | Per-thread setup example                                  |
| `stop.lua`                                  | Programmatic early stop                                   |

**Example: POST requests**

```lua
wrk.method = "POST"
wrk.body   = '{"key": "value"}'
wrk.headers["Content-Type"] = "application/json"
```

**Example: multiple endpoints**

```lua
paths = { "/api/users", "/api/orders", "/api/health" }
counter = 0

function request()
    counter = counter + 1
    return wrk.format(nil, paths[(counter % #paths) + 1])
end
```

## Benchmarking tips

- **Calibration period**: wrk3 calibrates for ~10 seconds before measuring. Runs shorter than 20 seconds may not produce useful data.
- **Ephemeral ports**: the machine running wrk3 needs enough ephemeral ports for all connections. Check with `sysctl net.inet.ip.portrange` (macOS) or `cat /proc/sys/net/ipv4/ip_local_port_range` (Linux).
- **Server backlog**: the target server's `listen()` backlog should exceed the number of concurrent connections.
- **Latency resolution**: measured latencies are accurate to ~1ms granularity due to OS sleep behavior.
- **Static vs dynamic scripts**: scripts that only set method/path/headers/body have no performance impact. Scripts that use `response()` or generate requests dynamically in `request()` will reduce maximum throughput.
- **Connection count**: use at least as many connections as threads (`-c` >= `-t`).

## Coordinated Omission

The key insight behind wrk3's measurement model:

> If you plan to send 1000 requests/sec and the server stalls for 1 second, a traditional load generator measures 0 requests during that second (it was waiting). wrk3 correctly reports that 1000 requests experienced 1+ second latency — because that's what real users would have seen.

Use `--u_latency` alongside `--latency` to see both corrected and uncorrected distributions side by side. The difference is often dramatic — 99th percentile differences of 100x or more are common when the server has occasional stalls.

For a deeper explanation, see Gil Tene's talk [How NOT to Measure Latency](https://www.youtube.com/watch?v=lJ8ydIuPFeU).

## Architecture

```
wrk3
├── src/           C source (event loop, HTTP parser, TLS, stats)
├── scripts/       Example Lua scripts
├── CMakeLists.txt Build system (fetches all dependencies automatically)
└── build/         Build output (created by cmake)
    └── _deps/     Fetched dependencies
        ├── aws_lc-src/    AWS-LC (libcrypto)
        ├── s2n_tls-src/   s2n-tls (TLS)
        └── luajit-src/    LuaJIT (scripting)
```

## Acknowledgements

wrk3 is built on the work of:

- **Will Glozer** — [wrk](https://github.com/wg/wrk), the original HTTP benchmarking tool
- **Gil Tene and Mike Barker** — [wrk2](https://github.com/giltene/wrk2), constant throughput and Coordinated Omission correction, [HdrHistogram](http://hdrhistogram.org)

wrk3 also includes code from:

- [ae](https://github.com/redis/redis) — event loop from Redis, by Salvatore Sanfilippo
- [http-parser](https://github.com/nodejs/http-parser) — HTTP parser from nginx/Node.js
- [LuaJIT](https://luajit.org/) — Just-In-Time compiler for Lua, by Mike Pall
- [Tiny Mersenne Twister](http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/TINYMT/) — PRNG by Mutsuo Saito and Makoto Matsumoto
- [s2n-tls](https://github.com/aws/s2n-tls) — TLS implementation by AWS
- [AWS-LC](https://github.com/aws/aws-lc) — cryptographic library by AWS

See [NOTICE](NOTICE) for full licensing details.

## License

Apache License 2.0. See [LICENSE](LICENSE).
