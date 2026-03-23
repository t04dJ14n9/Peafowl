# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Peafowl is a secure multiparty computation (MPC) framework for privacy-preserving linear SVM training. It uses secret sharing (libSSS), MPI for distributed communication, and cryptographic primitives (LWR, PRF, SHPRG) to perform collaborative ML without exposing raw data.

**Tested on:** TencentOS Server 4.4, g++ 12.3.1, Python 3.11.6, cmake 3.26.5, mpich 4.1.1

---

## Quick Start (after full setup)

```bash
export PATH=$PATH:/usr/lib64/mpich/bin
mkdir -p data/log data/prg data/lprof
mpiexec -n 4 python3 main2.py 60 60
```

`dataset.py` also works and auto-calls `mpiexec main.py` via subprocess:
```bash
python3 dataset.py
```

---

## Full Setup Commands

### 1. System packages
```bash
sudo dnf install -y mpich mpich-devel cmake python3-devel gmp-devel openssl-devel
sudo bash -c 'echo /usr/lib64/mpich/lib > /etc/ld.so.conf.d/mpich.conf'
export PATH=$PATH:/usr/lib64/mpich/bin   # also add to ~/.bashrc
```

### 2. Python packages
```bash
pip3 install pybind11 mpi4py line_profiler torchvision
# h5py, numpy, torch, cryptography are also required (may already be installed)
```

### 3. Clone libSSS (submodule is SSH-only, use HTTPS)
```bash
cd src
git clone https://github.com/HangeTeng/libSSS.git
cd libSSS
git clone https://github.com/osu-crypto/libOTe
cd libOTe && git checkout 3a40823f0507710193d5b90e6917878853a2f836
git clone https://github.com/ladnir/cryptoTools
cd cryptoTools && git checkout 4a83de286d05669678364173f9fdfe45a44ddbc6
cd ../../../..
```

### 4. Patch STATIC→SHARED (required — see README)
```bash
BASE=src/libSSS/libOTe
sed -i 's/add_library(libOTe STATIC/add_library(libOTe SHARED/' $BASE/libOTe/CMakeLists.txt
sed -i 's/add_library(libOTe_Tests STATIC/add_library(libOTe_Tests SHARED/' $BASE/libOTe_Tests/CMakeLists.txt
sed -i 's/add_library(cryptoTools STATIC/add_library(cryptoTools SHARED/' $BASE/cryptoTools/cryptoTools/CMakeLists.txt
sed -i 's/add_library(tests_cryptoTools STATIC/add_library(tests_cryptoTools SHARED/' $BASE/cryptoTools/tests_cryptoTools/CMakeLists.txt
```

### 5. Download Boost manually (JFrog CDN is broken)
```bash
cd src/libSSS/libOTe/cryptoTools/thirdparty
rm -f boost_1_75_0.tar.bz2
curl -L -o boost_1_75_0.tar.bz2 "https://archives.boost.io/release/1.75.0/source/boost_1_75_0.tar.bz2"
cd ../../../..
```

### 6. Fix RELIC blake2 alignment bug (GCC 12+)
In `src/libSSS/libOTe/cryptoTools/thirdparty/relic/src/md/blake2.h`:
- Move `blake2s_state` and `blake2b_state` structs **outside** the `#pragma pack(push, 1)` block
- Add `uint8_t _pad[N]` fields to make each struct's size a multiple of 64
- Move `blake2sp_state` and `blake2bp_state` outside the pack block as well

Then build RELIC:
```bash
cd src/libSSS/libOTe/cryptoTools/thirdparty/relic
cmake -S . -B build_linux -DCMAKE_BUILD_TYPE=Release -DMULTI=PTHREAD
cmake --build build_linux --parallel $(nproc)
cmake --install build_linux --prefix ../unix
cd ../../../../../..
```

### 7. Build Boost and install libOTe
```bash
cd src/libSSS/libOTe
python3 build.py --setup --boost   # extracts and compiles boost
python3 build.py --install=out/install/linux \
  -- -D ENABLE_RELIC=ON -D ENABLE_NP=ON -D ENABLE_KOS=ON -D ENABLE_IKNP=ON -D ENABLE_SILENTOT=ON
cd ../../..
```

### 8. Register library paths
```bash
PATH_TO_SSS="$(pwd)/src/libSSS"
echo -e "$PATH_TO_SSS/libOTe/cryptoTools/thirdparty/unix/lib\n$PATH_TO_SSS/libOTe/out/install/linux/lib64\n$PATH_TO_SSS/libOTe/out/install/linux/lib" \
    | sudo tee /etc/ld.so.conf.d/libSSS.conf
sudo ldconfig
```

### 9. Add `lib64` to libSSS CMakeLists (RHEL/TencentOS fix)
```bash
# In src/libSSS/CMakeLists.txt, after the existing lib line, add:
link_directories(libOTe/out/install/linux/lib64)
```

### 10. Build libSSS Python extension
```bash
mkdir -p src/libSSS/build && cd src/libSSS/build
cmake .. -Dpybind11_DIR=$(python3 -c "import pybind11; print(pybind11.get_cmake_dir())")
make -j$(nproc)
SITE="/usr/local/lib/python3.11/site-packages"  # adjust for your Python version
sudo cp SSS.cpython-*.so "$SITE/"
sudo cp libosn.so "$SITE/"
cd ../../..
```

### 11. Build LWR extension (module name is `lwr_cpp`, not `lwr`)
```bash
cd src/utils/crypto
c++ -O3 -Wall -shared -std=c++11 -fPIC \
    $(python3 -m pybind11 --includes) \
    lwr.cpp -o lwr_cpp$(python3-config --extension-suffix)
sudo cp lwr_cpp.cpython-*.so /usr/local/lib/python3.11/site-packages/
cd ../../..
```

---

## Tests

```bash
python3 test.py
python3 testDS.py
python3 src/utils/crypto/matmul_test.py
mpiexec -n 4 python3 test/mpi_group.py
```

---

## Architecture

### Distributed computation model
- **MPI-based master-worker**: Rank 0–N-1 are data nodes (clients); Rank N is the server/coordinator.
- All `main*.py` scripts require MPI — there is no sequential mode.
- `src/communicator/node.py` — `Node` class wrapping MPI send/receive with size tracking.
- `src/communicator/STComm.py` — Secure Transport using libSSS (wraps `Sender`/`Receiver`).

### Cryptographic pipeline
1. **Encoding**: Floats → fixed-point integers via `FixedPointEncoder` (`src/utils/encoder.py`)
2. **Secret sharing**: SHPRG-based data splitting (`src/utils/crypto/shprg.py`) + libSSS OSN
3. **Secure matrix ops**: Modular arithmetic (mod 2^EP) across distributed shares
4. **Decoding**: Fixed-point integers → floats

### Key modules
| Module | Purpose |
|--------|---------|
| `src/utils/h5dataset.py` | HDF5 dataset management with PyTorch Dataset interface |
| `src/utils/encoder.py` | Fixed-point encoding/decoding, modular range |
| `src/utils/crypto/shprg.py` | AES-based SHPRG + LWR computation |
| `src/utils/crypto/prf.py` | SHA256/MD5 pseudo-random function |
| `src/utils/crypto/lwr.cpp` | LWR pybind11 C++ extension (module name: `lwr_cpp`) |
| `src/communicator/node.py` | MPI node + OSN sender/receiver wrapper |

### main.py variants
- `main.py`: Oldest version. Has `sys.exit()` debug gates — exits after PSI stage.
- `main2.py`: Most complete. Gates commented out. Runs through `share_tras` stage. **Use this one.**
- `main3.py`: Intermediate version.

### Data layout
- Datasets: `data/SVM_{examples}_{features}/SVM_...hdf5`
- Per-node splits: `.../{examples}_{features}_{rank}-{nodes}.hdf5`
- Timing/profile output: `data/lprof/`, `data/log/`, `data/prg/`

---

## Known Bugs (Already Fixed in This Repo)

| File | Bug |
|------|-----|
| `main.py` | `Node()` missing `is_server` argument |
| `main2.py` | `STinit()` called with `permutes=` (should be `permute=`) |
| `main2.py` | `@atimer` on `STrecv_thread` makes it unpicklable for multiprocessing |
| `main2.py` | `multiprocessing.Pool` used for local functions → switched to `ThreadPoolExecutor` |
| `node.py` | `STinit()` only created one sender; now creates a dict when server gets list of permutes |
| `src/libSSS/CMakeLists.txt` | Missing `lib64` in `link_directories` |
| `src/libSSS/libOTe/cryptoTools/thirdparty/relic/src/md/blake2.h` | GCC 12 alignment error |

## Known Constraints
- `src/libSSS/` is a git submodule using SSH URL — must clone manually via HTTPS
- The `.so` install paths assume Python 3.11 — rebuild extensions if using a different version
- CUDA sources (`*.cu`) are present but experimental/optional
- `main2.py` only implements through the share-transfer phase; later phases (matrix ops, SVM convergence) may have additional issues
