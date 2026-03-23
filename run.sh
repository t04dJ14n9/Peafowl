#!/usr/bin/env bash
# run.sh — One-click runner for Peafowl MPC project
# Usage:  ./run.sh [examples] [features] [nodes]
# Defaults: examples=60, features=60, nodes=3  (so mpiexec uses nodes+1=4 processes)

set -euo pipefail

# ── Configurable parameters ──────────────────────────────────────────────────
NODES="${3:-3}"
SUB_EXAMPLES="${1:-50}"
EXAMPLES=$(( SUB_EXAMPLES * 6 / 5 ))
SUB_FEATURES="${2:-20}"
FEATURES=$(( NODES * SUB_FEATURES ))
MPI_PROCS=$(( NODES + 1 ))

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── Colour helpers ────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "${CYAN}[run.sh]${NC} $*"; }
ok()    { echo -e "${GREEN}[run.sh] ✓${NC} $*"; }
warn()  { echo -e "${YELLOW}[run.sh] ⚠${NC} $*"; }
die()   { echo -e "${RED}[run.sh] ✗${NC} $*" >&2; exit 1; }

# ── 1. PATH: ensure mpiexec is reachable ─────────────────────────────────────
if ! command -v mpiexec &>/dev/null; then
    for candidate in /usr/lib64/mpich/bin /usr/lib/mpich/bin /usr/local/bin; do
        if [[ -x "$candidate/mpiexec" ]]; then
            export PATH="$candidate:$PATH"
            info "Added $candidate to PATH"
            break
        fi
    done
    command -v mpiexec &>/dev/null || die "mpiexec not found. Install mpich: sudo dnf install -y mpich"
fi
ok "mpiexec: $(command -v mpiexec)"

# ── 2. Python sanity checks ───────────────────────────────────────────────────
info "Checking Python dependencies..."
MISSING=()
for mod in mpi4py h5py numpy torch SSS lwr_cpp; do
    python3 -c "import $mod" 2>/dev/null || MISSING+=("$mod")
done
if [[ ${#MISSING[@]} -gt 0 ]]; then
    die "Missing Python modules: ${MISSING[*]}\nRun the full setup described in CLAUDE.md first."
fi
ok "All Python modules present"

# ── 3. Create required data directories ──────────────────────────────────────
mkdir -p data/log data/prg data/lprof
ok "Data directories ready"

# ── 4. Generate dataset if not already present ───────────────────────────────
DATASET_DIR="data/SVM_${EXAMPLES}_${FEATURES}"
SPLIT_FILE="${DATASET_DIR}/SVM_${EXAMPLES}_${FEATURES}_0-${NODES}.hdf5"

if [[ -f "$SPLIT_FILE" ]]; then
    ok "Dataset already exists at $DATASET_DIR — skipping generation"
else
    info "Generating dataset (examples=$EXAMPLES, features=$FEATURES, nodes=$NODES)..."
    python3 - <<PYEOF
import os, math, random
import numpy as np
from src.utils.h5dataset import HDF5Dataset, preprocess_linearSVM, save_subset_h5

examples   = $EXAMPLES
features   = $FEATURES
nodes      = $NODES
sub_examples = $SUB_EXAMPLES
chunk      = 100

folder = "./data/SVM_{}_{}".format(examples, features)
os.makedirs(folder, exist_ok=True)
file_path = "{}/SVM_{}_{}.hdf5".format(folder, examples, features)

print("  Generating HDF5 dataset ...")
dataset = HDF5Dataset.new(file_path=file_path, data_shape=(features,),
                          target_shape=(), dtype=np.float32)
for i in range(math.ceil(examples / chunk)):
    data, targets = preprocess_linearSVM(examples=min(chunk, examples - i*chunk),
                                         features=features)
    dataset.add(data=data, targets=targets)
dataset.close()

print("  Splitting dataset across {} nodes ...".format(nodes))
dataset = HDF5Dataset(file_path=file_path)
slice_features = features // nodes
for i in range(nodes):
    sub_path = "{}/SVM_{}_{}_{}-{}.hdf5".format(folder, examples, features, i, nodes)
    indices = random.sample(range(examples), sub_examples)
    save_subset_h5(dataset=dataset, file_path=sub_path,
                   indices=indices,
                   slice=slice(slice_features*i, slice_features*(i+1)),
                   slice_features=slice_features,
                   with_targets=(i == 0), dtype=np.float32)
dataset.close()

for subdir in ("temp", "tgt"):
    os.makedirs("{}/{}".format(folder, subdir), exist_ok=True)
print("  Dataset ready.")
PYEOF
    ok "Dataset generated at $DATASET_DIR"
fi

# ── 5. Launch MPI job ─────────────────────────────────────────────────────────
info "Launching: mpiexec -n $MPI_PROCS python3 main2.py $EXAMPLES $FEATURES"
echo ""
mpiexec -n "$MPI_PROCS" python3 main2.py "$EXAMPLES" "$FEATURES"
echo ""
ok "Done. Profile logs written to data/lprof/"
