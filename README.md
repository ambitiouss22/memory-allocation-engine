# Memory Allocation Engine (MAE)

A high-performance, lock-free, concurrent memory allocator written from scratch in C++20.

Designed to replace standard system allocators (`malloc`/`free`) in highly concurrent environments, this engine utilizes thread-local caching, slab allocation, and 128-bit atomic Compare-And-Swap (CAS) instructions to bypass OS-level mutex locks. Under heavy concurrent loads (16 threads), MAE **outperforms the native Windows `malloc` by ~7%**, achieving over 106 Million operations per second.

## 🚀 Performance Benchmarks

Tested on a 16-thread load, performing 3.2 million randomized allocations and deallocations (sizes 16-256 bytes) with forced memory writes (`volatile`) to prevent compiler Dead Code Elimination (DCE).

| Allocator | Total Time | Throughput (ops/sec) |
| :--- | :--- | :--- |
| **MAE (Custom)** | **30.165 ms** | **106,084,000** |
| Windows UCRT `malloc` | 32.426 ms | 98,685,000 |

*Result: **~7% latency reduction** compared to the highly optimized Windows native allocator.*

## 🧠 Architectural Deep Dive

This engine circumvents the standard concurrency bottleneck (multiple threads fighting for a single OS memory lock) through a tiered, lock-free architecture.

### 1. Thread-Local Caching (`ThreadCache`)
Every active thread is assigned its own private cache. When a thread requests memory, it pulls from its local cache in `O(1)` time with **zero contention**. No atomic operations or locks are required for the hot path.

### 2. Lock-Free Central Pool (`CentralPool`)
When a thread depletes its local cache, it fetches a new batch of memory from the global `CentralPool`. Instead of using standard `std::mutex` locks which stall threads, the Central Pool handles concurrent requests using **Hardware 16-byte Atomics**.
* **The ABA Problem Solved:** Lock-free popping from a concurrent stack is notoriously vulnerable to the ABA problem. This is mitigated using a 128-bit `TaggedHead` structure (a pointer paired with a modification counter) and utilizing the x86-64 `cmpxchg16b` hardware instruction.

### 3. Slab Allocation
Memory is requested from the OS in massive blocks (Slabs) and partitioned into fixed-size classes (16, 32, 64, 128, 256 bytes). This eliminates memory fragmentation and avoids the overhead of traversing free-lists to find fitting blocks.

### 4. False-Sharing Prevention
All heavily contended data structures are explicitly aligned using `alignas(64)` to strictly fit within modern CPU L1 cache lines, preventing cache-invalidation ping-pong between cores.

## 🛠️ Tech Stack & Build Requirements

* **Language:** C++20
* **Build System:** CMake (3.10+)
* **Compiler:** GCC 15 (Requires MSYS2 UCRT64 environment on Windows)
* **Architecture:** x86-64 (Requires `-mcx16` flag for 128-bit atomic hardware support)

## ⚙️ How to Build and Run

This project relies on strict memory alignment rules (`std::align_val_t`) and atomic linking, requiring a modern C++17/20 compliant toolchain.

**1. Clone the repository**
```bash
git clone [https://github.com/ambitiouss22/memory-allocation-engine.git](https://github.com/ambitiouss22/memory-allocation-engine.git)
cd memory-allocation-engine
```
**2. Configure the build via CMake**
```
Bash
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
```
**3. Compile the Engine (links libatomic)**
```
Bash
cmake --build build
```
**4. Run the Stress Test Benchmark**
```
Bash
./build/benchmark.exe
```
** Quick Start: How to Use MAE**
The Memory Allocation Engine is designed to be a drop-in replacement or a dedicated allocator for high-performance contexts.

```C++
#include "mae/allocator.h"
#include <iostream>

int main() {
    // 1. Initialize the central memory pool (e.g., 1GB capacity)
    MAE::CentralPool pool(1024 * 1024 * 1024);

    // 2. Allocate memory via the Thread Cache (lock-free path)
    void* ptr = MAE::allocate(256); 
    
    if (ptr) {
        std::cout << "Successfully allocated 256 bytes!" << std::endl;
        
        // ... perform high-speed operations ...

        // 3. Deallocate memory back to the slab/pool
        MAE::deallocate(ptr, 256); 
    }

    return 0;
}
```
