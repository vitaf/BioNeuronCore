# 安装与构建 BioNeuronCore

## 系统要求

- **操作系统：** Linux（推荐 Ubuntu 22.04/24.04）
- **编译器：** 支持 SYCL 2020 的 Intel oneAPI DPC++（dpcpp/icpx）；或用于 NVIDIA GPU 的原生 CUDA（nvcc）
- **CMake：** 3.20+
- **标准：** C++17

## 安装依赖

### 步骤 1：Intel oneAPI Base Toolkit

安装 Intel oneAPI Base Toolkit（包含 DPC++ 编译器与 SYCL 运行时）：

```bash
# 添加 Intel 仓库
wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | \
  gpg --dearmor | sudo tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null
echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/oneapi all main" | \
  sudo tee /etc/apt/sources.list.d/oneAPI.list

# 安装
sudo apt update
sudo apt install intel-basekit
```

安装路径：`/opt/intel/oneapi/`

### 步骤 2：激活环境

```bash
source /opt/intel/oneapi/setvars.sh
```

如需自动激活，请将其加入 `~/.bashrc`：
```bash
echo 'source /opt/intel/oneapi/setvars.sh > /dev/null 2>&1' >> ~/.bashrc
```

### 步骤 3：验证

```bash
which dpcpp    # /opt/intel/oneapi/compiler/latest/bin/dpcpp
dpcpp --version
```

若未找到 `dpcpp`，CMake 将尝试使用 `icpx -fsycl`。

### 替代方案：原生 CUDA（NVIDIA GPU）

对于 NVIDIA GPU，请使用原生 nvcc（安装 nvidia-cuda-toolkit）：

```bash
# 通过发行版的包管理器安装，或从源码构建
# 需要 CUDA（NVIDIA）或 ROCm（AMD）
```

CMake 将自动检测 nvcc 并构建原生 CUDA 后端。

## 依赖（自动获取）

- **Catch2 v2.13.10** — 测试框架（FetchContent，无需手动安装）

其余所有依赖均来自 oneAPI 的 SYCL 头文件。

## 构建

```bash
cd BioNeuronCore
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 构建目标

| 目标 | 类型 | 说明 |
|------|-----|----------|
| `biocore` | 静态库 | CPU 版本（仅 HH + 受体 + .bion 加载器） |
| `biocore_sycl` | 静态库 | SYCL 版本（包含 GPU 内核、模拟、.bion） |
| `test_hh` | 可执行文件 | Hodgkin-Huxley 测试（CPU） |
| `test_synapses` | 可执行文件 | 突触受体测试（CPU） |
| `test_bion` | 可执行文件 | 二进制 .bion 格式测试（CPU） |
| `test_consolidation` | 可执行文件 | 巩固测试：Nucleus、修饰符 87-92、基因→蛋白质→结构 |
| `test_nb_receptors` | 可执行文件 | NB_ 受体测试（24 项测试，Neuroblox 兼容） |
| `test_gpu` | 可执行文件 | GPU 模拟集成测试（仅当启用 SYCL 时） |

### 构建特定目标

```bash
make test_hh            # 构建 + 链接 CPU HH 测试
make test_synapses      # 构建 + 链接 CPU 突触测试
make test_plugins       # 构建 + 链接 CPU 插件测试
make test_nb_receptors  # 构建 + 链接 NB_ 受体（24 项测试）
make test_bion          # 构建 + 链接 CPU .bion 测试
make test_gpu           # 构建 + 链接 GPU 测试（需要 SYCL）
make biocore_sycl       # 构建 SYCL 库
```

## 运行测试

```bash
# 单独测试
./test_hh
./test_synapses
./test_bion
./test_gpu           # 需要 GPU，否则将在 CPU 上以回退方式失败

# 通过 CTest 运行全部测试
ctest                # 运行 test_hh + test_synapses + test_bion [+ 启用 SYCL 时 test_gpu]
```

## 预期输出

```
$ ./test_hh
===============================================================================
All tests passed (10 assertions in 7 test cases)

$ ./test_gpu
GPU 设备：Intel(R) UHD Graphics 630
队列已创建。
正在运行 500 个 tick 的模拟……
模拟完成。
500 个 tick 后（SYCL）：V_m = -67.11 mV
检查：V_m 处于 [-75, -55] 范围内：OK
```

## 可能的问题

### "dpcpp: command not found"
oneAPI 未激活。请运行 `source /opt/intel/oneapi/setvars.sh`。

### "SYCL: 未找到 — 仅 CPU 测试"
未检测到 SYCL 编译器。请安装 Intel oneAPI 或 NVIDIA CUDA Toolkit。若无 SYCL，仅构建 CPU 测试（`test_hh`、`test_synapses`、`test_bion`）。

### "GPU 未找到，正在尝试 CPU……"（test_gpu 中）
无 GPU 时的正常情况。测试将通过 `cpu_selector_v` 在 CPU 上执行。

### "error: SYCL kernel cannot call undefined function"
函数未标记 `BIOCORE_DEVICE_CALLABLE`。请为从 SYCL 内核调用的函数定义与 extern 声明添加该宏。

### 选择 GPU 后端

MindCopy 支持三种 GPU 加速方案：

| 后端 | GPU | 许可 | 编译器 |
|--------|-----|----------|------------|
| **Intel oneAPI**（默认） | Intel UHD/Iris/Xe | 免费 | `icpx -fsycl` |
| **原生 CUDA (nvcc)** | NVIDIA | 免费（CUDA Toolkit） | `nvcc` |
| **Codeplay oneAPI for NVIDIA** | NVIDIA | Community（非商业用途免费） | `icpx -fsycl` |

#### Intel oneAPI（默认）
```bash
source /opt/intel/oneapi/setvars.sh
cmake .. -DCMAKE_CXX_COMPILER=icpx -DCMAKE_CXX_FLAGS="-fsycl"
```

#### 原生 CUDA（NVIDIA）
```bash
# 安装：sudo apt install nvidia-cuda-toolkit
cmake .. -DCMAKE_BUILD_TYPE=Release
```
MindCopy 代码无需修改 — nvcc 支持带 CUDA 扩展的 C++17。

#### Codeplay oneAPI for NVIDIA（Community Edition）
```bash
# 从 developer.codeplay.com 下载（非商业用途免费）
# 安装插件后，NVIDIA GPU 将通过 sycl-ls 可见
source /opt/intel/oneapi/setvars.sh
cmake .. -DCMAKE_CXX_COMPILER=icpx -DCMAKE_CXX_FLAGS="-fsycl"
```


## 权利与知识产权

© 2024–2026 **费申·维塔利·亚历山德罗维奇** / Vitaly Alexandrovich Feshin。保留所有权利。
电子邮箱：feshin2004@mail.ru · vfeshin@mindcopy.ru · 电话：+7(918)345-75-18。俄罗斯克拉斯诺达尔 / Krasnodar, Russia。

计算机程序国家注册证书（俄罗斯专利局）**编号 727894_683451**。
未经作者书面许可，禁止商业使用、传播和再许可。
全文请参见平台根目录中的 `LICENSE` 与 `DISCLAIMER.md`。
