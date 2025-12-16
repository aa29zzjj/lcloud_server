# LionCloud Filesystem + Cache (CMPSC311 Assignment)

This repository implements a simplified **LionCloud filesystem stack**.
The system provides a complete abstraction layer on top of LionCloud block
devices, including caching, filesystem operations, and network communication.

The implementation includes:

- A block cache with an **LRU replacement policy**
- A filesystem interface (`open`, `read`, `write`, `seek`, `close`, `shutdown`)
- A network client that communicates with the LionCloud server using register frames
- A simulator driver that executes workload files to validate correctness

---

## Project Structure

### lcloud_cache.c — Block Cache (LRU)

This module implements an in-memory cache for LionCloud blocks.

Each cache entry stores:
- `(did, sec, blk)` as the block address key
- `data[LC_DEVICE_BLOCK_SIZE]` as the cached block payload
- An `lru` counter for replacement decisions

**Key behaviors**

- **Lookup (`lcloud_getcache`)**
  - Scans the cache for a matching `(device, sector, block)`
  - On hit: updates the entry’s LRU timestamp and returns the data pointer
  - On miss: increments the miss counter and returns `NULL`

- **Insert (`lcloud_putcache`)**
  - Writes a block into the cache
  - Uses an empty slot if available (`did == -1`)
  - Otherwise replaces the least-recently-used entry

- **Initialization / Close**
  - `lcloud_initcache` allocates cache memory
  - `lcloud_closecache` prints hit/miss statistics and frees memory

---

### lcloud_client.c — Network Client (Bus Requests)

This module implements the client-side LionCloud network protocol.

Its responsibilities include:
- Opening a TCP connection to the LionCloud server
- Sending 64-bit register frames in network byte order
- Optionally sending or receiving 256-byte block payloads for read/write
- Returning the response register frame to the caller

**Core entry point**

- `client_lcloud_bus_request(reg, buf)`

This function handles four operation types:
- **READ**: send register → receive register → receive block data
- **WRITE**: send register + block data → receive register
- **POWER OFF**: send register → receive register → close socket
- **Other operations**: send register → receive register

---

### lcloud_filesys.c — Filesystem Interface

This module implements the filesystem API exposed to the simulator.

Supported operations:
- `lcopen`
- `lcread`
- `lcwrite`
- `lcseek`
- `lcclose`
- `lcshutdown`

#### Device and Register Helpers

The filesystem constructs and parses LionCloud register frames using:
- `create_lcloud_registers(...)`
- `extract_lcloud_registers(...)`

#### Initialization Flow

On first use, the filesystem performs system initialization:
- Power on LionCloud
- Probe available devices
- Initialize device metadata
- Initialize the block cache

#### File Model

Each open file is tracked using in-memory metadata:
- File name
- File size
- Current position
- A dynamic list of allocated `(device, sector, block)` mappings

#### Read Path (`lcread`)

- Determines which block contains the current file position
- Checks the cache first
- On cache miss: reads from the device and inserts the block into the cache
- Copies requested bytes into the user buffer
- Advances the file cursor

#### Write Path (`lcwrite`)

- Writes data in block-sized segments
- Allocates new blocks as needed
- Uses a **write-through** strategy:
  - If cached: update cache and write to device
  - If not cached: read block, modify data, cache it, then write
- Updates file size and current position

#### Shutdown (`lcshutdown`)

- Powers off LionCloud
- Frees all file metadata
- Closes and reports cache statistics

---

### lcloudsim.c — Simulator / Workload Driver

This program is the provided simulator used to validate correctness.

It:
- Loads a workload file
- Executes operations such as `OPEN`, `READ`, `WRITE`, `SEEK`, and `CLOSE`
- Verifies that read data matches expected output
- Calls `lcshutdown()` at the end of execution

If the filesystem implementation is correct, the simulation completes successfully.

---

## End-to-End Data Flow

1. The simulator issues filesystem calls (`lcopen`, `lcread`, `lcwrite`, etc.)
2. The filesystem maps file offsets to `(device, sector, block)`
3. Reads and writes pass through the block cache first
4. On cache miss or write-through, device operations are issued
5. The network client sends register frames to the LionCloud server over TCP

---

## Build and Run

Exact commands may vary depending on the course-provided environment.

Typical usage:

- Compile the project:
make

Run the simulator with a workload file:

./lcloud_sim <workload-file>

Enable verbose logging:

./lcloud_sim -v <workload-file>

---

## Notes and Design Choices

- The cache uses a fixed-size array with linear lookup and LRU replacement.
- File metadata is stored in memory without persistent directories.
- Blocks are allocated sequentially across devices, sectors, and blocks.
- Correctness is validated through simulator workload comparisons.

