# Installing and building BioNeuronCore

## System requirements

- **OS:** Linux (Ubuntu 22.04/24.04 recommended)
- **Compiler:** Intel oneAPI DPC++ (dpcpp/icpx) with SYCL 2020 support; or native CUDA (nvcc) for NVIDIA GPU
- **CMake:** 3.20+
- **Standard:** C++17

## Installing dependencies

### Step 1: Intel oneAPI Base Toolkit

Install the Intel oneAPI Base Toolkit (contains the DPC++ compiler and SYCL runtime):

```bash
# Adding the Intel repository
wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | \
  gpg --dearmor | sudo tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null
echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/oneapi all main" | \
  sudo tee /etc/apt/sources.list.d/oneAPI.list

# Installation
sudo apt update
sudo apt install intel-basekit
```

Installation path: `/opt/intel/oneapi/`

### Step 2: Activating the environment

```bash
source /opt/intel/oneapi/setvars.sh
```

For automatic activation, add to `~/.bashrc`:
```bash
echo 'source /opt/intel/oneapi/setvars.sh > /dev/null 2>&1' >> ~/.bashrc
```

### Step 3: Verification

```bash
which dpcpp    # /opt/intel/oneapi/compiler/latest/bin/dpcpp
dpcpp --version
```

If `dpcpp` is not found, CMake will try to use `icpx -fsycl`.

### Alternative: Native CUDA (NVIDIA GPU)

For NVIDIA GPU use native nvcc (install nvidia-cuda-toolkit):

```bash
# Install via your distribution's package manager or build from source
# CUDA (NVIDIA) or ROCm (AMD) is required
```

CMake will automatically detect nvcc and build the native CUDA backend.

## Dependencies (fetched automatically)

- **Catch2 v2.13.10** — testing framework (FetchContent, no manual installation required)

All other dependencies are SYCL headers from oneAPI.

## Building

```bash
cd BioNeuronCore
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Build targets

| Target | Type | Description |
|------|-----|----------|
| `biocore` | Static library | CPU version (only HH + receptors + .bion loader) |
| `biocore_sycl` | Static library | SYCL version (includes GPU kernels, simulation, .bion) |
| `test_hh` | Executable | Hodgkin-Huxley tests (CPU) |
| `test_synapses` | Executable | Synaptic receptor tests (CPU) |
| `test_bion` | Executable | Binary .bion format tests (CPU) |
| `test_consolidation` | Executable | Consolidation tests: Nucleus, modifiers 87-92, gene→protein→structure |
| `test_nb_receptors` | Executable | NB_ receptor tests (24 tests, Neuroblox-compatible) |
| `test_gpu` | Executable | GPU simulation integration test (only with SYCL) |

### Building a specific target

```bash
make test_hh            # build + link the CPU HH test
make test_synapses      # build + link the CPU synapse test
make test_plugins       # build + link the CPU plugin test
make test_nb_receptors  # build + link NB_ receptors (24 tests)
make test_bion          # build + link the CPU .bion test
make test_gpu           # build + link the GPU test (requires SYCL)
make biocore_sycl       # build the SYCL library
```

## Running tests

```bash
# Individual tests
./test_hh
./test_synapses
./test_bion
./test_gpu           # Requires a GPU or will fail on CPU with fallback

# All tests via CTest
ctest                # runs test_hh + test_synapses + test_bion [+ test_gpu with SYCL]
```

## Expected output

```
$ ./test_hh
===============================================================================
All tests passed (10 assertions in 7 test cases)

$ ./test_gpu
GPU device: Intel(R) UHD Graphics 630
Queue created.
Running simulation for 500 ticks...
Simulation complete.
After 500 ticks (SYCL): V_m = -67.11 mV
Check: V_m in range [-75, -55]: OK
```

## Possible issues

### "dpcpp: command not found"
oneAPI is not activated. Run `source /opt/intel/oneapi/setvars.sh`.

### "SYCL: not found — CPU tests only"
The SYCL compiler was not detected. Install Intel oneAPI or NVIDIA CUDA Toolkit. Without SYCL only CPU tests are built (`test_hh`, `test_synapses`, `test_bion`).

### "GPU not found, trying CPU..." (in test_gpu)
A normal situation when no GPU is present. The test will run on the CPU via `cpu_selector_v`.

### "error: SYCL kernel cannot call undefined function"
The function is not marked `BIOCORE_DEVICE_CALLABLE`. Add the macro to the definition and extern declaration of the function called from a SYCL kernel.

### Choosing the GPU backend

MindCopy supports three GPU acceleration options:

| Backend | GPU | License | Compiler |
|--------|-----|----------|------------|
| **Intel oneAPI** (default) | Intel UHD/Iris/Xe | Free | `icpx -fsycl` |
| **Native CUDA (nvcc)** | NVIDIA | Free (CUDA Toolkit) | `nvcc` |
| **Codeplay oneAPI for NVIDIA** | NVIDIA | Community (free for non-commercial use) | `icpx -fsycl` |

#### Intel oneAPI (default)
```bash
source /opt/intel/oneapi/setvars.sh
cmake .. -DCMAKE_CXX_COMPILER=icpx -DCMAKE_CXX_FLAGS="-fsycl"
```

#### Native CUDA (NVIDIA)
```bash
# Installation: sudo apt install nvidia-cuda-toolkit
cmake .. -DCMAKE_BUILD_TYPE=Release
```
The MindCopy code requires no changes — nvcc supports C++17 with CUDA extensions.

#### Codeplay oneAPI for NVIDIA (Community Edition)
```bash
# Download from developer.codeplay.com (free for non-commercial use)
# After installing the plugin, the NVIDIA GPU becomes visible via sycl-ls
source /opt/intel/oneapi/setvars.sh
cmake .. -DCMAKE_CXX_COMPILER=icpx -DCMAKE_CXX_FLAGS="-fsycl"
```


## Rights and intellectual property

© 2024–2026 **Feshin Vitaly Alexandrovich** / Vitaly Alexandrovich Feshin. All rights reserved.
E-mail: feshin2004@mail.ru · vfeshin@mindcopy.ru · Tel.: +7(918)345-75-18. Krasnodar, Russia / Краснодар, Россия.

State registration certificate of a computer program (Rospatent) **No. 727894_683451**.
Commercial use, distribution and sublicensing without the author's written permission are prohibited.
For the full text, see `LICENSE` and `DISCLAIMER.md` in the platform root.
