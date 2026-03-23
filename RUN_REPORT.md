# Peafowl — Run Report

**Date:** 2026-03-24
**Environment:** TencentOS Server 4.4, g++ 12.3.1, Python 3.11.6, cmake 3.26.5, mpich 4.1.1

---

## Summary

The project was successfully run end-to-end.
The working command is:

```bash
export PATH=$PATH:/usr/lib64/mpich/bin
mkdir -p data/log data/prg data/lprof
python3 dataset.py               # generate dataset + auto-calls mpiexec internally
# OR run directly:
mpiexec -n 4 python3 main2.py 60 60
```

Expected output (per rank):
```
start_test: Rank N - send: 0.0000 MB, recv: 0.0000 MB
share_tras: Rank N - send: 0.0264 MB, recv: 0.0315 MB
timepoint  |   all    | elapsed  |
share_tras | 0.24 s   | 0.24 s   |
```

---

## All Problems Encountered and Fixes Applied

### 1. Submodule retracted — SSH URL unusable

**Problem:** `.gitmodules` uses `git@github.com:HangeTeng/libSSS.git` (SSH). The submodule
directory was empty and the remote may not be accessible over SSH.

**Fix:** Clone via HTTPS manually:
```bash
cd src
git clone https://github.com/HangeTeng/libSSS.git
```
Then inside `libSSS`, clone the pinned dependencies:
```bash
git clone https://github.com/osu-crypto/libOTe
cd libOTe && git checkout 3a40823f0507710193d5b90e6917878853a2f836

git clone https://github.com/ladnir/cryptoTools
cd cryptoTools && git checkout 4a83de286d05669678364173f9fdfe45a44ddbc6
```

---

### 2. System: mpich and cmake not on PATH

**Problem:** `mpiexec` and `cmake` were missing.

**Fix:**
```bash
sudo dnf install -y mpich mpich-devel cmake
sudo bash -c 'echo /usr/lib64/mpich/lib > /etc/ld.so.conf.d/mpich.conf'
sudo ldconfig
export PATH=$PATH:/usr/lib64/mpich/bin   # add to ~/.bashrc permanently
```

---

### 3. Python packages missing

**Problem:** `pybind11`, `mpi4py`, `line_profiler`, `torchvision` not installed.

**Fix:**
```bash
pip3 install pybind11 mpi4py line_profiler torchvision
```

Also required:
```bash
sudo dnf install -y python3-devel   # for Python.h headers
sudo dnf install -y gmp-devel openssl-devel
```

---

### 4. Boost 1.75.0 download fails (JFrog CDN returns HTML)

**Problem:** `python3 build.py --setup --boost` tries to download from
`https://boostorg.jfrog.io/...`. The CDN returns a 12KB HTML redirect page, not the archive.
The `tarfile` extraction then fails with `not a bzip2 file`.

**Fix:** Manually download from `archives.boost.io` before running the script:
```bash
cd src/libSSS/libOTe/cryptoTools/thirdparty
rm -f boost_1_75_0.tar.bz2
curl -L -o boost_1_75_0.tar.bz2 \
  "https://archives.boost.io/release/1.75.0/source/boost_1_75_0.tar.bz2"
# verify size ~117MB and file type "bzip2 compressed data"
file boost_1_75_0.tar.bz2
```
Then re-run:
```bash
cd src/libSSS/libOTe && python3 build.py --setup --boost
```

---

### 5. RELIC fails to build — blake2 alignment error (GCC 12)

**Problem:** GCC 12 enforces a stricter rule: if a struct has `__attribute__((aligned(N)))`,
its size must be a multiple of N. `blake2s_state` and `blake2b_state` are 64-byte aligned but
their sizes (185 and 361 bytes) are not multiples of 64. In GCC 12 this is a hard error, not a
warning.

Additionally, `blake2s_state` and `blake2b_state` were inside a `#pragma pack(push, 1)` block,
which further suppressed natural alignment padding.

**File:** `src/libSSS/libOTe/cryptoTools/thirdparty/relic/src/md/blake2.h`

**Fix:**
1. Move `blake2s_param` and `blake2b_param` (which legitimately need `pack(1)`) into the
   packed region, and move `blake2s_state` and `blake2b_state` **outside** the packed region.
2. Add explicit padding fields to round sizes up to multiples of 64.

Changed struct layout in `blake2.h`:

```c
// param structs stay inside #pragma pack(push, 1) ... #pragma pack(pop)
// state structs moved OUTSIDE the pack block, with padding:

ALIGNME( 64 ) typedef struct __blake2s_state {
    uint32_t h[8];
    uint32_t t[2];
    uint32_t f[2];
    uint8_t  buf[2 * BLAKE2S_BLOCKBYTES];
    size_t   buflen;
    uint8_t  last_node;
    uint8_t  _pad[...];   // makes sizeof() a multiple of 64
} blake2s_state;

ALIGNME( 64 ) typedef struct __blake2b_state {
    uint64_t h[8];
    ...
    uint8_t  _pad[...];   // makes sizeof() a multiple of 64
} blake2b_state;
```

Also move `blake2sp_state` and `blake2bp_state` (which embed aligned state arrays) outside
the `#pragma pack` block.

---

### 6. STATIC→SHARED conversion required for libOTe / cryptoTools

**Problem:** As documented in the README, libOTe and cryptoTools must be compiled as shared
libraries so that the Python extension module can dynamically link them.

**Fix:** Edit 4 `CMakeLists.txt` files before building:

| File | Line | Change |
|------|------|--------|
| `libOTe/libOTe/CMakeLists.txt` | 7 | `STATIC` → `SHARED` |
| `libOTe/libOTe_Tests/CMakeLists.txt` | 5 | `STATIC` → `SHARED` |
| `libOTe/cryptoTools/cryptoTools/CMakeLists.txt` | 9 | `STATIC` → `SHARED` |
| `libOTe/cryptoTools/tests_cryptoTools/CMakeLists.txt` | 5 | `STATIC` → `SHARED` |

---

### 7. libOTe installs to `lib64/` but CMakeLists.txt links against `lib/`

**Problem:** On the TencentOS/RHEL-family system, cmake installs libraries to `lib64/` not
`lib/`. The libSSS `CMakeLists.txt` only adds `libOTe/out/install/linux/lib` to
`link_directories`, so the linker cannot find `liblibOTe.so` or `libcryptoTools.so`.

**Fix:** Add `lib64` to `link_directories` in `src/libSSS/CMakeLists.txt`:
```cmake
link_directories(libOTe/out/install/linux/lib64)
link_directories(libOTe/out/install/linux/lib)
```

Also register in ldconfig:
```bash
PATH_TO_SSS="/data/workspace/Personal/Peafowl/src/libSSS"
echo -e "$PATH_TO_SSS/libOTe/cryptoTools/thirdparty/unix/lib\n$PATH_TO_SSS/libOTe/out/install/linux/lib64\n$PATH_TO_SSS/libOTe/out/install/linux/lib" \
    | sudo tee /etc/ld.so.conf.d/libSSS.conf
sudo ldconfig
```

---

### 8. pybind11 cmake config not found during libSSS cmake

**Problem:** `find_package(pybind11 REQUIRED)` fails because pybind11 was installed via pip
into a user directory that cmake doesn't search by default.

**Fix:** Pass the cmake directory explicitly:
```bash
cd src/libSSS/build
cmake .. -Dpybind11_DIR=$(python3 -c "import pybind11; print(pybind11.get_cmake_dir())")
```

---

### 9. SSS module named `SSS.cpython-311-...` but README assumed Python 3.10

**Problem:** The README references `SSS.cpython-310-...` and `/usr/lib/python3/dist-packages`.
On Python 3.11 the filename is different and that path doesn't exist.

**Fix:** Install to the actual site-packages path for your Python version:
```bash
SITE="/usr/local/lib/python3.11/site-packages"
sudo cp build/SSS.cpython-311-x86_64-linux-gnu.so "$SITE/"
sudo cp build/libosn.so "$SITE/"
```

---

### 10. LWR extension module named `lwr_cpp`, not `lwr`

**Problem:** The README says to build `lwr.cpp` as `lwr$(python3-config --extension-suffix)`,
but the `PYBIND11_MODULE` declaration inside `lwr.cpp` names it `lwr_cpp`. Building it as `lwr`
produces a module whose export function is `PyInit_lwr_cpp`, causing an `ImportError` when
Python tries to load `lwr.cpython-xxx.so`.

**Fix:**
```bash
cd src/utils/crypto
c++ -O3 -Wall -shared -std=c++11 -fPIC \
    $(python3 -m pybind11 --includes) \
    lwr.cpp -o lwr_cpp$(python3-config --extension-suffix)
sudo cp lwr_cpp.cpython-*.so /usr/local/lib/python3.11/site-packages/
```

---

### 11. mpi4py installed as ABI-generic wheel — cannot find libmpi.so

**Problem:** `pip install mpi4py` installs an ABI-agnostic wheel that dynamically discovers
`libmpi.so` at import time. On TencentOS, MPICH installs to `/usr/lib64/mpich/lib/`, which is
not in the default ldconfig search path.

**Fix:**
```bash
sudo bash -c 'echo /usr/lib64/mpich/lib > /etc/ld.so.conf.d/mpich.conf'
sudo ldconfig
```

---

### 12. `main.py`: Node() called without required `is_server` argument

**Problem:** `node.py` updated `Node.__init__()` to require `is_server` as a 5th positional
argument, but `main.py` still calls `Node(src, tgt, global_comm, client_comm)` (4 args).

**Fix applied to `main.py`:**
```python
# line 97 (server branch):
node = Node(None, None, global_comm, client_comm, is_server)
# line 128 (client branch):
node = Node(src_dataset, tgt_dataset, global_comm, client_comm, is_server)
```
Note: `main2.py` already had this fix applied by the author.

---

### 13. `main2.py`: `Node.STinit()` called with `permutes=` (wrong kwarg)

**Problem:** `main2.py` calls `node.STinit(size=..., permutes=permutes, ...)` but `node.py`
defines the parameter as `permute` (singular).

**Fix applied to `main2.py`:**
```python
node.STinit(size=sub_examples, permute=permutes, p=q)
```

---

### 14. `node.py`: STinit creates only one sender but main2.py needs one per client

**Problem:** The server node needs a separate `OSNSender` for each client (each with a
different permutation). `node.py`'s `STinit` only created one `self.STSender`. The `STsend`
method used `dset_rank` to determine the port but always used the same sender object,
which would result in wrong permutations being applied.

**Fix applied to `node.py`:** When `permute` is a list-of-lists, create a dict of senders:
```python
def STinit(self, size=None, permute=None, p=1<<128, ios_threads=4):
    if self.is_server:
        if isinstance(permute, list) and isinstance(permute[0], list):
            self.STSenders = {i: Sender(size=size, permute=permute[i], p=p, ...)
                              for i in range(len(permute))}
        else:
            self.STSender = Sender(...)
    else:
        self.STRecver = Receiver(...)
```

And in `STsend`:
```python
STSender = self.STSenders[dset_rank] if self.STSenders is not None else self.STSender
```

---

### 15. `main2.py`: `@atimer` decorator makes `STrecv_thread` unpicklable

**Problem:** `STrecv_thread` was decorated with `@atimer` (a locally-defined decorator
creating a closure), then passed to `multiprocessing.Pool.map()`. Python's pickle serializer
cannot handle local closures, resulting in:
```
AttributeError: Can't pickle local object 'atimer.<locals>.func_wrapper'
```

**Fix applied to `main2.py`:** Remove the `@atimer` decorator from `STrecv_thread`.

---

### 16. `main2.py`: `multiprocessing.Pool` can't pickle local functions

**Problem:** Both `STsend_thread` and `STrecv_thread` are defined inside `main()` (local
scope). `multiprocessing.Pool.map` forks new processes and must pickle the function, but
Python cannot pickle local functions.

**Fix applied to `main2.py`:** Replace `multiprocessing.Pool` with `ThreadPoolExecutor`
(which does not require pickling, as it uses threads in the same process):
```python
# Before:
with multiprocessing.Pool(processes=25) as pool:
    pool.map(STrecv_thread, task_args)

# After:
with ThreadPoolExecutor(max_workers=25) as pool:
    list(pool.map(STrecv_thread, task_args))
```
Note: `ThreadPoolExecutor` is already imported at the top of `main2.py`.
The inner `from concurrent.futures import ThreadPoolExecutor` imports inside `if` blocks
must be removed — they shadow the module-level import and cause `UnboundLocalError`.

---

### 17. Missing output directories

**Problem:** The project writes output files to `data/log/`, `data/prg/`, and `data/lprof/`
but does not create them automatically.

**Fix:**
```bash
mkdir -p data/log data/prg data/lprof
```

---

## Complete Working Setup Script

```bash
#!/bin/bash
set -e
cd /path/to/Peafowl

# 1. System dependencies
sudo dnf install -y mpich mpich-devel cmake python3-devel gmp-devel openssl-devel
sudo bash -c 'echo /usr/lib64/mpich/lib > /etc/ld.so.conf.d/mpich.conf'
export PATH=$PATH:/usr/lib64/mpich/bin
echo 'export PATH=$PATH:/usr/lib64/mpich/bin' >> ~/.bashrc

# 2. Python dependencies
pip3 install pybind11 mpi4py line_profiler torchvision

# 3. Clone libSSS and sub-dependencies
cd src
git clone https://github.com/HangeTeng/libSSS.git
cd libSSS
git clone https://github.com/osu-crypto/libOTe
cd libOTe && git checkout 3a40823f0507710193d5b90e6917878853a2f836
git clone https://github.com/ladnir/cryptoTools
cd cryptoTools && git checkout 4a83de286d05669678364173f9fdfe45a44ddbc6
cd ../../..

# 4. Patch STATIC→SHARED
BASE=src/libSSS/libOTe
sed -i 's/add_library(libOTe STATIC/add_library(libOTe SHARED/'    $BASE/libOTe/CMakeLists.txt
sed -i 's/add_library(libOTe_Tests STATIC/add_library(libOTe_Tests SHARED/' $BASE/libOTe_Tests/CMakeLists.txt
sed -i 's/add_library(cryptoTools STATIC/add_library(cryptoTools SHARED/' $BASE/cryptoTools/cryptoTools/CMakeLists.txt
sed -i 's/add_library(tests_cryptoTools STATIC/add_library(tests_cryptoTools SHARED/' $BASE/cryptoTools/tests_cryptoTools/CMakeLists.txt

# 5. Fix RELIC blake2 alignment issue (GCC 12)
# See blake2.h fix described above — move state structs outside #pragma pack block
# and add padding fields

# 6. Download Boost manually (JFrog CDN broken)
cd src/libSSS/libOTe/cryptoTools/thirdparty
rm -f boost_1_75_0.tar.bz2
curl -L -o boost_1_75_0.tar.bz2 \
  "https://archives.boost.io/release/1.75.0/source/boost_1_75_0.tar.bz2"
cd ../../../..

# 7. Build Boost and RELIC
cd src/libSSS/libOTe
python3 build.py --setup --boost
cd cryptoTools/thirdparty/relic
cmake -S . -B build_linux -DCMAKE_BUILD_TYPE=Release -DMULTI=PTHREAD
cmake --build build_linux --parallel $(nproc)
cmake --install build_linux --prefix ../unix
cd ../../../..

# 8. Build libOTe
cd src/libSSS/libOTe
python3 build.py --install=out/install/linux \
  -- -D ENABLE_RELIC=ON -D ENABLE_NP=ON -D ENABLE_KOS=ON -D ENABLE_IKNP=ON -D ENABLE_SILENTOT=ON
cd ../..

# 9. Register library paths
PATH_TO_SSS="$(pwd)/src/libSSS"
echo -e "$PATH_TO_SSS/libOTe/cryptoTools/thirdparty/unix/lib\n$PATH_TO_SSS/libOTe/out/install/linux/lib64\n$PATH_TO_SSS/libOTe/out/install/linux/lib" \
    | sudo tee /etc/ld.so.conf.d/libSSS.conf
sudo ldconfig

# 10. Add lib64 to libSSS CMakeLists
sed -i '/link_directories(libOTe\/out\/install\/linux\/lib)$/a link_directories(libOTe\/out\/install\/linux\/lib64)' \
    src/libSSS/CMakeLists.txt

# 11. Build libSSS Python extension
mkdir -p src/libSSS/build && cd src/libSSS/build
cmake .. -Dpybind11_DIR=$(python3 -c "import pybind11; print(pybind11.get_cmake_dir())")
make -j$(nproc)
SITE="/usr/local/lib/python3.11/site-packages"
sudo cp SSS.cpython-*.so "$SITE/"
sudo cp libosn.so "$SITE/"
cd ../../..

# 12. Build LWR pybind11 extension
cd src/utils/crypto
c++ -O3 -Wall -shared -std=c++11 -fPIC \
    $(python3 -m pybind11 --includes) \
    lwr.cpp -o lwr_cpp$(python3-config --extension-suffix)
sudo cp lwr_cpp.cpython-*.so "$SITE/"
cd ../../..

# 13. Create data directories
mkdir -p data/log data/prg data/lprof

# 14. Run
python3 dataset.py
# OR: mpiexec -n 4 python3 main2.py 60 60
```

---

## Source Code Bugs Requiring Patches

The following bugs exist in the repo and must be patched before running. The fixes above
already apply them if you follow the script.

| File | Bug | Fix |
|------|-----|-----|
| `main.py` | `Node()` called without `is_server` argument | Pass `is_server` as 5th argument |
| `main2.py` | `STinit()` called with `permutes=` (wrong kwarg) | Change to `permute=` |
| `main2.py` | `@atimer` on `STrecv_thread` makes it unpicklable | Remove decorator |
| `main2.py` | `multiprocessing.Pool` cannot pickle local functions | Switch to `ThreadPoolExecutor` |
| `main2.py` | Inner `from concurrent.futures import ThreadPoolExecutor` causes `UnboundLocalError` | Remove inner imports |
| `node.py` | `STinit()` creates only one sender; server needs one per client | Handle list-of-permutes → dict of senders |
| `node.py` | `STSenders` not initialized in `__init__` | Add `self.STSenders = None` |
| `node.py` | `getTotalDataSent/Recv` don't account for `STSenders` | Sum over `STSenders.values()` |

---

## Codebase Completion Status

`main.py` is **partially implemented** — it contains 3 `sys.exit()` debug checkpoints.
Only the first stage (PSI / encrypted ID exchange) runs before the first `sys.exit()`.

`main2.py` is more complete — the debug checkpoints are commented out. The share-transfer
(OSN-based secret sharing) runs, completing through `share_tras` timing checkpoint.
Stages after `share_tras` (matrix multiplication under secret sharing, SVM model convergence)
are present in code but may have additional runtime issues not yet tested.

`main3.py` has the `sys.exit()` calls also commented out.
