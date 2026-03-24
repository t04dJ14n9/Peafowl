# syntax=docker/dockerfile:1
# Peafowl — reproducible build image
# Base: Ubuntu 22.04, Python 3.11, GCC 12, MPICH 4.1.1
#
# Build:  docker build -t peafowl .
# Run:    docker run --rm -it peafowl
# Quick:  docker run --rm peafowl bash run.sh

FROM ubuntu:22.04

# ── Prevent interactive prompts ────────────────────────────────────────────────
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# ── 1. System packages ─────────────────────────────────────────────────────────
RUN apt-get update && apt-get install -y --no-install-recommends \
    # build tools
    gcc-12 g++-12 make cmake \
    # MPI
    mpich libmpich-dev \
    # Python 3.11
    python3.11 python3.11-dev python3-pip \
    # cryptography dependencies
    libgmp-dev libssl-dev \
    # misc
    git curl ca-certificates \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.11 100 \
    && rm -rf /var/lib/apt/lists/*

# ── 2. Python packages ─────────────────────────────────────────────────────────
COPY requirements.txt /tmp/requirements.txt
RUN pip3 install --no-cache-dir -r /tmp/requirements.txt

# ── 3. Copy project source ─────────────────────────────────────────────────────
WORKDIR /app
COPY . /app

# ── 4. Download Boost (JFrog CDN is broken; use archives.boost.io) ─────────────
RUN cd src/libSSS/libOTe/cryptoTools/thirdparty && \
    curl -L -o boost_1_75_0.tar.bz2 \
      "https://archives.boost.io/release/1.75.0/source/boost_1_75_0.tar.bz2"

# ── 5. Build Boost ─────────────────────────────────────────────────────────────
RUN cd /app/src/libSSS/libOTe && python3 build.py --setup --boost

# ── 6. Build RELIC (blake2 fix already applied in source) ─────────────────────
RUN cd /app/src/libSSS/libOTe/cryptoTools/thirdparty/relic && \
    cmake -S . -B build_linux \
          -DCMAKE_BUILD_TYPE=Release \
          -DMULTI=PTHREAD && \
    cmake --build build_linux --parallel "$(nproc)" && \
    cmake --install build_linux --prefix ../unix

# ── 7. Build and install libOTe ───────────────────────────────────────────────
RUN cd /app/src/libSSS/libOTe && \
    python3 build.py --install=out/install/linux \
      -- -D ENABLE_RELIC=ON \
         -D ENABLE_NP=ON \
         -D ENABLE_KOS=ON \
         -D ENABLE_IKNP=ON \
         -D ENABLE_SILENTOT=ON

# ── 8. Register shared library paths ──────────────────────────────────────────
RUN echo "/app/src/libSSS/libOTe/cryptoTools/thirdparty/unix/lib" \
        > /etc/ld.so.conf.d/libSSS.conf && \
    echo "/app/src/libSSS/libOTe/out/install/linux/lib64" \
        >> /etc/ld.so.conf.d/libSSS.conf && \
    echo "/app/src/libSSS/libOTe/out/install/linux/lib" \
        >> /etc/ld.so.conf.d/libSSS.conf && \
    ln -sf /usr/lib/x86_64-linux-gnu/libmpich.so.12 \
           /usr/lib/x86_64-linux-gnu/libmpi.so.12 && \
    ldconfig

# ── 9. Build libSSS Python extension (SSS + libosn) ───────────────────────────
RUN mkdir -p /app/src/libSSS/build && \
    cd /app/src/libSSS/build && \
    cmake .. \
      -Dpybind11_DIR="$(python3 -c 'import pybind11; print(pybind11.get_cmake_dir())')" && \
    make -j"$(nproc)" && \
    SITE="$(python3 -c 'import sysconfig; print(sysconfig.get_path("platlib"))')" && \
    cp SSS.cpython-*.so "$SITE/" && \
    cp libosn.so "$SITE/"

# ── 10. Build LWR pybind11 C++ extension ──────────────────────────────────────
RUN cd /app/src/utils/crypto && \
    SITE="$(python3 -c 'import sysconfig; print(sysconfig.get_path("platlib"))')" && \
    PY_INC="$(python3 -m pybind11 --includes)" && \
    EXT="$(python3 -c 'import sysconfig; print(sysconfig.get_config_var("EXT_SUFFIX"))')" && \
    eval "c++ -O3 -Wall -shared -std=c++11 -fPIC $PY_INC lwr.cpp -o lwr_cpp${EXT}" && \
    cp lwr_cpp.cpython-*.so "$SITE/"

# ── 11. Create required runtime directories ────────────────────────────────────
RUN mkdir -p /app/data/log /app/data/prg /app/data/lprof

# ── 12. Smoke-test all imports ─────────────────────────────────────────────────
RUN python3 -c 'import mpi4py, numpy, torch, h5py, cryptography, SSS, lwr_cpp; \
print("All imports OK"); \
print("  mpi4py:", mpi4py.__version__); \
print("  numpy:", numpy.__version__); \
print("  torch:", torch.__version__); \
print("  SSS:", dir(SSS)); \
print("  lwr_cpp:", dir(lwr_cpp))'

# ── Runtime ────────────────────────────────────────────────────────────────────
ENV PATH="/usr/bin:/usr/lib/x86_64-linux-gnu/mpich/bin:${PATH}"

WORKDIR /app
CMD ["bash", "run.sh"]
