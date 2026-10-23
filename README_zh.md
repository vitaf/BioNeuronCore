# BioNeuronCore — 神经网络计算引擎

## 用途

BioNeuronCore 是基于 C++17 与 SYCL 2020（Intel oneAPI DPC++）的生物真实神经网络模拟引擎。计算在 GPU（Intel、NVIDIA、AMD）上执行，并可未来转译至 FPGA（Xilinx Vitis HLS）。代码围绕**分段神经元模型**这一原则构建：每个神经元由通过细胞内连接相连的独立电学区室（分段）组成。

**文件格式：** BION v12（MAX_MODIFIERS=32，向后兼容 v3-v11）。**版本：** v0.20.0。**测试：** 156。**修饰符：** 198。

## 核心能力

| 模式 | 常量 | 说明 |
|-------|-----------|----------|
| **BIOLOGICAL** | `MODE_BIOLOGICAL` (0) | 全部 198 种修饰符类型均使用经过生物学校准的模型（Hodgkin-Huxley、神经递质受体、泵、信号级联、Ca²⁺/K⁺/Cl⁻ 稳态、节律发生通道、46 个基因的基因表达、Cx36、TRP/伤害感受器、转运体、AQP4）。 |
| **ARBITRARY** | `MODE_ARBITRARY` (1) | 用户可以用自己的函数覆盖任意修饰符。用户修饰符（ID 0–127，整个范围为插件式）优先于生物修饰符。 |

### CPU/GPU 后端

| 后端 | 构建 | 说明 |
|--------|--------|----------|
| **CPU** | `libbiocore_gcc.a`（GCC） | 始终可用。Studio 默认使用。 |
| **GPU** | `libbiocore_gpu_backend.so`（DPC++） | 通过 dlopen 加载。需要 Intel oneAPI 运行时。在 Studio 工具栏中选择。USM 指针可从 CPU 访问。 |

### 修饰符插件

用户修饰符（ID 0–127）通过 `dlopen` 从 `.so` 文件加载。每个插件导出 `biocore_plugin_init()` 和 `biocore_plugin_desc()`（JSON 描述符）。API：`plugin_load()`、`plugin_load_directory()`、`register_user_modifier()`。示例：`plugins/example_ca_plugin.cpp`。

### 宿主加载器（无 SYCL）

`bion_loader_host.cpp` — 将 .bion 加载/保存到宿主内存（new[]/delete[]）。无需 Intel oneAPI。BioNeuronStudio 使用它来加载模型。

### 架构：分段、神经元、网络

```
.bion 文件 → NetworkHeader → Neuron[] → Segment[] → IntraConnection[] → Synapse[]
```

**分段（Segment）** — 基本的电学区室。具有：
- 几何：位置 (x, y, z)、长度、半径 → 膜面积 → 电容
- 膜电位 `V_m`（mV）
- 7 种胞内与 7 种胞外离子浓度（Na⁺、K⁺、Ca²⁺、Cl⁻、Mg²⁺、A⁻、H⁺）
- 胞外环境中的 15 种神经递质
- 最多 32 个修饰符（离子通道、受体、泵、髓鞘、传感器）
- ATP 水平和温度
- 转录信号 `transcription_signal`（CREB 依赖的可塑性）
- 工作电流缓冲 `I_buffer`（每个 tick 清零）

**神经元（Neuron）** — 分段的一种逻辑分组。包含：
- 分段范围 `[segment_start, segment_start + segment_count)`
- 胞体索引（`soma_index`）
- 尖峰阈值和类型

**细胞内连接（IntraConnection）** — 两个分段之间的电阻桥：
- 轴向电流：`I = (V_a − V_b) / R_coupling`
- 分段最多累积 16 条入向连接（通过 `intra_connection_ids[]` 快速访问）

**突触（Synapse）** — 神经元之间的化学/电学连接：
- 突触前自身受体（最多 8 个）— 调节释放概率
- 突触后受体（最多 8 个）— 在神经递质结合时产生离子电流
- 概率性释放（xorshift32 RNG，seed = id ⊕ tick ⊕ V_m）
- 突触间隙中的神经递质清除（指数衰减）
- 不应期
- 易化与抑制

### 修饰符：198 种类型

#### 基础（8 种类型）
| 类型 | 功能 |
|-----|---------|
| `MOD_RESERVED_0` (0) | 原 `MOD_IONOTROPIC_NAK`（HH 鱿鱼）— 已删除（第 4 阶段，由 Nav1.x 亚型替代） |
| `MOD_CAV13` (1) | Cav1.3 L 型 Ca²⁺ 通道（起搏） |
| `MOD_IONOTROPIC_CL` (2) | Cl⁻ 通道 |
| `MOD_METABOTROPIC` (3) | 通用代谢型受体 |
| `MOD_MYELIN` (4) | 髓鞘：阻断离子型电流，电容降低 100 倍 |
| `MOD_SYNAPTIC_RELEASE` (5) | 神经递质释放机制 |
| `MOD_PHYSICAL_SENSOR` (6) | 物理传感器（温度、压力） |
| `MOD_PUMP_NAK` (7) | Na⁺/K⁺-ATP 酶：每循环 3 Na⁺ 出、2 K⁺ 入，ATP 依赖 |

#### 离子型受体（9 种类型）
`AMPA`、`NMDA`、`KAINATE`、`GABA-A`、`GLYCINE`、`NICOTINIC`、`5-HT3`、`P2X`、`TRPV1`

它们在神经递质结合时直接打开离子通道。髓鞘阻断有髓分段上的离子型受体（郎飞结——无髓鞘分段——保持活跃）。

#### 代谢型受体（46 种类型，3 个家族）

**Gi/o（24 种类型，抑制性）：** `GABA-B`、`5-HT1A/1B/1D/1F`、`D2/D3/D4`、`M2/M4`、`A1/A3`、`mGluR2/3/4/6/7/8`、`α2`、`CB1/CB2`、`H3`、`MT1/MT2`

**Gs（11 种类型，经 cAMP 兴奋性）：** `5-HT4/6/7`、`D1/D5`、`A2A/A2B`、`β1/β2`、`H2`、`TAAR1`

**Gq/11（11 种类型，经 IP3/DAG 兴奋性）：** `5-HT2A/2B/2C`、`α1A/α1B`、`M1/M3/M5`、`mGluR1/5`、`H1`

### 突触前调节剂（12 种类型）

位于突触前膜上、调节神经递质释放概率的自身受体：
`5-HT1A`、`5-HT1B`、`D2`、`A1`、`A2A`、`D1`、`mGluR2`、`mGluR3`、`CB1`、`H3`、`NICOTINIC`、`α2`

### 神经递质（15 种类型）
谷氨酸、GABA、多巴胺、血清素、乙酰胆碱、腺苷、去甲肾上腺素、甘氨酸、组胺、内源性大麻素、ATP、褪黑素、Mg²⁺、尼古丁、辣椒素

### 离子（7 种类型）
Na⁺、K⁺、Ca²⁺、Cl⁻、Mg²⁺、A⁻（大阴离子）、H⁺（质子）— 追踪胞内与胞外浓度

## 模拟循环（每个 tick 5 步）

```
步骤 1：   修饰符电流 + 刺激处理（I_clamp、ramp、离子/配体注入）→ 分段的 I_buffer
步骤 2：   轴向电流（IntraConnection）→ 原子累加至 I_buffer
步骤 2.5：分段间扩散（离子、ATP、温度、转录），按菲克定律
步骤 3：   突触电流（释放 → 调节 → 突触后响应 → 清除）
步骤 4：   更新 V_m = V_m + I_buffer / C_m × dt，重置 I_buffer，电压钳（覆写 V_m）
```

每一步均同步（`q.wait()`），从而保证确定性的计算顺序。

## 巩固/再巩固（两阶段记忆）— v0.10.0

### 基因系统（Nucleus，412 字节）

每个神经元都有一个带有 46 个可塑性基因的细胞核（`Nucleus`）。基因转录为 mRNA（`gene_activity[]`），再翻译为蛋白质（`protein_level[]`）。该过程由后台线程 `ConsolidationThread` 管理。

**46 个基因：** `CREB`、`c-Fos`、`Zif268`、`Arc`、`BDNF`、`Cofilin`、`MMP-9`、`TIMP`、`GluR1`、`GluR2`、`PSD-95`、`Neurexin`、`Neuroligin`、`Dynamin`、`Clathrin`、`IntegrinB3`、`TrkB`、`mTOR`、`S6K1`、`4E-BP`

### 信号级联（修饰符 87-92）

| 类型 | 用途 | 动力学 |
|-----|-----------|----------|
| `MOD_MAPK_ERK` (87) | Ca²⁺→Ras→Raf→MEK→ERK→pCREB（巩固） | Hill n=3, Kd=0.5μM, τ_ERK=5s, τ_nuc=30s |
| `MOD_IKK_NFKB` (88) | Ca²⁺→IKK→IkB→NF-κB→Zif268（再巩固） | Michaelis-Menten, Kd=0.3μM, τ=10s |
| `MOD_MTORC1` (89) | 双钥匙：AA→Rheb + PI3K/AKT→mTORC1 | τ_Rheb=5s, τ_AKT=8s, τ_mTOR=15s |
| `MOD_PROTEIN_SYNTH` (90) | mTORC1→S6K1/4E-BP→蛋白质翻译 | τ_S6K=10s, τ_4EBP=8s, τ_translate=60s |
| `MOD_COFILIN` (91) | ERK→LIMK→pCofilin/F-actin（肌动蛋白细胞骨架） | τ_LIMK=8s, τ_Cof=5s, τ_actin=30s |
| `MOD_MMP9` (92) | ERK+NF-κB→MMP-9/TIMP/整合素（ECM） | τ_MMP9=20s, τ_TIMP=15s, τ_ECM=100s |

### 基因调控（consolidation.cpp）

```
巩固：       pCREB ──→ c-Fos, BDNF, Arc
             ERK   ──→ Cofilin, MMP-9
             ERK+NF-κB ──→ MMP-9

再巩固：     NF-κB ──→ Zif268（而非 c-Fos！）

晚期基因：   c-Fos ──→ GluR1, GluR2
             Arc   ──→ PSD-95
             BDNF  ──→ Neurexin, Neuroligin, TrkB
             MMP-9 ──→ IntegrinB3
             mTORC1──→ S6K1, 4E-BP
```

### 结构可塑性

| 事件 | 判据 | 效果 |
|---------|---------|--------|
| **新突触** | Arc>0.6 + PSD-95>0.5 + MMP-9>0.3 + BDNF>0.5 | 向最近的分段创建 CHEMICAL 突触（r<50μm） |
| **LTD 移除** | Calcineurin>0.3 + weight<0.05 + MMP-9<0.3 | Tombstone (id=0) |
| **直径变化** | F_actin 偏离 0.5 | Δr = (F_actin-0.5)×0.02×r（最大 ±20%） |
| **AMPAR 密度** | GluR1+GluR2 自 0.5 | Δd = (GluR-0.5)×0.01 |

### 模拟步骤

步骤 2.5b（每个 tick）：胞体→Nucleus — 缓存 pCREB、ERK、NF-κB、mTORC1、amino_acid_level。

步骤 5（每个 tick）：应用线程队列中的 `ConsolidationEvent`。

### NB_ 受体（Neuroblox 兼容，ID 63–79）

实现 Neuroblox.jl 动力学模型的 17 个修饰符：

| 组 | 类型 | 说明 |
|--------|------|----------|
| 受体 (8) | NB_NMDA_MoradiFull, NB_AMPA_STA, NB_D1/D2_Modulator, NB_Alpha7ER_nAChR, NB_Beta2_nAChR, NB_CaTRPM4R, NB_MuscarinicR | Q10 校正，Markov 门控，Ca²⁺ 纳米域，Hill 占位 |
| Destexhe (6) | NB_AMPA/NMDA/GABAA/GABAB_Destexhe, NB_MsnNMDAR, NB_MsnAMPAR | 阻尼振荡动力学，交叉调节耦合 D1↔NMDAR、D2↔AMPAR |
| 胞体 (3) | NB_SomaATP, NB_SomaThermal, NB_CREB_Transcription | 线粒体 ATP 合成，37°C 热稳态，CREB→基因→可塑性 |
| **巩固 (6)** | **MAPK_ERK, IKK_NFKB, mTORC1, ProteinSynth, Cofilin, MMP9** | **两阶段记忆：基因→蛋白质→结构** |

## 分段间扩散（步骤 2.5）

- **离子扩散**（Na⁺、K⁺、Ca²⁺、Cl⁻、Mg²⁺），按菲克定律并做体积归一化
- **ATP 扩散** 从胞体到树突/轴突（DIFF_ATP = 0.14）
- **热传导**（胞体作为神经元的热核，DIFF_HEAT = 0.0014）
- **转录扩散**（CREB 信号从细胞核到树突，缓慢 0.01×）

## 二进制格式 .bion

`.bion` 是网络的**唯一**配置文件。它包含**全部**数据：拓扑、分段参数、修饰符（包括突触前和突触后）、模拟模式。无需任何 JSON/YAML/XML。

### 结构（v9）
```
[NetworkHeader]  84 字节 — magic (0x42494F4E)、version (9)、mode、计数器、num_nuclei、nucleus_version
[Neuron[]]       num_neurons × 24 字节
[Segment[]]      num_segments × 1692 字节
[IntraConnection[]] num_intra_connections × 16 字节
[Synapse[]]      num_synapses × 836 字节
[Nucleus[]]      num_nuclei × 412 字节 — 基因、蛋白质、信号缓存
```

### 特性
- 所有数据顺序排列、无间隙 — 可实现零拷贝反序列化
- 所有结构均为 POD，`#pragma pack(1)` — FPGA 兼容
- 文件中仅含数组索引，无指针
- 加载时校验：magic、版本、索引边界、修饰符限制
- 支持保存/重载（检查点）且数据一致
- `build_id` 与 `timestamp` 用于追踪文件来源

## GPU 执行（SYCL）

- 所有内核均通过 Intel DPC++/icpx 以 `-fsycl` 编译
- 内存：USM（`sycl::malloc_shared`）— 统一的宿主 + 设备地址空间
- 以 switch 分派代替函数指针（与 SYCL 设备代码不兼容）
- 原子操作（`sycl::atomic_ref`）用于线程安全的电流累加
- ARBITRARY 模式下用户覆盖时的自动 CPU 回退
- 支持 GPU（Intel UHD/Iris/Arc、经原生 CUDA (nvcc) 的 NVIDIA）与 CPU

## FPGA 兼容性（设计）

- 所有结构均为带 `#pragma pack(1)` 的 POD — 固定偏移
- 固定数组（内核中无动态分配）
- 计算路径中分支最少
- .bion 的可流式读取（固定偏移 = 内存映射）
- 代码已就绪，可综合至 Xilinx Vitis HLS

## 测试

| 测试 | 覆盖范围 | 断言 |
|------|----------|------------|
| `test_hh` | HH 通道、静息电位、尖峰、泵、电缆传导、髓鞘、arbitrary 覆盖 | 10 |
| `test_synapses` | AMPA、GABA-A、NMDA、5-HT1A、D1、mGluR1、泵（长期稳定性） | 12 |
| `test_plugins` | 加载 .so 插件、注册、分派 | 8 |
| `test_nb_receptors` | NB_ 受体（24 项测试：Mg²⁺ 阻断、Q10、D1/D2、α7、β2、ATP、CREB、Destexhe……） | 43 |
| `test_consolidation` | 巩固：Nucleus、修饰符 87-92、基因→蛋白质→结构、LTD 移除 | 44 |
| `test_bion` | 加载/保存 .bion v6、校验、v4→v6 迁移 | 23 |
| `test_gpu` | GPU 上的完整模拟循环：V_m、轴向电流、髓鞘、CPU 回退、突触 | 集成 |
| `test_plugin_manager` | PluginManager：目录加载、info JSON、生命周期、边界情况 | 18 |
| `test_consolidation_plasticity` | 可塑性：LTP/LTD、钙触发、可塑性修饰符 | 12 |
| `test_gpu_backend` | GPU 后端：dlopen、初始化、CPU↔GPU 切换、刺激 | 10 |
| `test_delay_buffer` | GPU 延迟缓冲：环形缓冲、上下文切换 | 6 |
| `test_v3v4_migration` | BION v3→v4 迁移：向后兼容、direction、新字段 | 8 |
| `test_all_modifiers` | 全部 198 种修饰符类型：注册与参数校验 | 8 |
| `test_diffusion` | 分段间扩散：离子、ATP、温度（按菲克定律） | 6 |
| `test_stimuli` | 刺激：I_clamp、电压钳、ramp、离子/配体注入 | 10 |
| `test_parametric_overrides` | 参数覆盖：覆盖修饰符参数 | 8 |
| `test_simulation_host` | 完整 CPU 模拟循环：加载、ticks、快照 | 10 |
| `test_bion_loader_host` | 宿主加载器：头部校验、边界、数据完整性 | 12 |

**合计：156 项测试，100% PASS。** Valgrind：无泄漏。

## 物理常量

- 法拉第常数：96485 C/mol
- 气体常数：8.314 J/(mol·K)
- 温度：37°C（310.15 K）— 能斯特方程、HH 动力学
- 膜比电容：C_SPECIFIC_PF_PER_UM2 = 0.01 pF/μm²（= 1 μF/cm²，被髓鞘降低 100 倍）

## 关键生物学机制

- **静息电位** 由 Na⁺/K⁺ 通道与 Na⁺/K⁺-ATP 酶的平衡形成
- **动作电位**（尖峰）：去极化 → Na⁺ 通道激活 → 失活 → K⁺ 复极化
- **Na⁺/K⁺ 泵** 通过消耗 ATP 抵抗离子漂移；没有它神经元会去极化
- **髓鞘** 阻断有髓区域的离子型电流，使郎飞结保持活跃
- **跳跃式传导**：尖峰在郎飞结之间跳跃，加速传递
- **概率性突触释放**：受突触前自身受体、易化与抑制调节
- **神经递质清除**：突触间隙中浓度的指数衰减

## 突触接触的生物学：活性区与 PSD

在真实的中枢神经系统突触中，受体与神经递质释放机制并非集中于神经元的整个表面，而是集中在微小的特化接触区。使用圆柱形分段的全部面积（2πrl ≈ 300–3000 μm²）计算突触电导在生物学上是不正确的。

### 活性区（突触前，`pre_area`）

活性区是发生突触囊泡对接与胞吐的突触前膜区域：

- **中枢神经系统中的尺寸：** 约 0.05–0.2 μm²（单个突触）。大型突触（Held 花萼、海马苔藓纤维膨体）可达 1–2 μm²，具有多个活性区。
- **结构：** SNARE 蛋白（syntaxin、SNAP-25、synaptobrevin）、Ca²⁺ 通道（Cav2.1/Cav2.2）、活性区蛋白（Bassoon、Piccolo、RIM、Munc13）组成的致密网格。
- **功能：** 决定可立即释放的囊泡数量（RRP — 可释放池，每个活性区约 5–10 个囊泡）。
- **自身受体：** 位于突触旁（距活性区约 0.1–0.5 μm）— 其密度按 `pre_area` 缩放。

### PSD — 突触后致密区（`post_area`）

PSD（postsynaptic density）是突触后侧的电子致密结构：

- **尺寸：** 约 0.05–0.5 μm²。与活性区尺寸成比例（突触的前、后侧通过跨突触信号在尺寸上匹配）。
- **结构：** 由支架蛋白（PSD-95、SAP97、Shank、Homer、GKAP）锚定的受体簇（AMPA、NMDA、GABA-A）、肌动蛋白细胞骨架。
- **功能：** 突触电导与 PSD 面积 × 受体密度成比例（约 100–1000 受体/μm²）。受体在 PSD 上分布不均：AMPA — 分布整个面积，NMDA — 更靠中心，mGluR — 位于突触旁。

### 突触旁组织

- **突触旁受体：** mGluR2/3、GABA-B、CB1 位于距 PSD 边缘约 0.1–1 μm 处，处于谷氨酸量子释放的直接可达范围之外。在溢出（神经递质扩散到间隙之外）时被激活。
- **星形胶质细胞鞘：** 星形胶质细胞突起覆盖约 60–80% 的突触接触，限制溢出并表达转运体（谷氨酸的 EAAT1/2，GABA 的 GAT-1/3）。
- **突触外受体：** 多达 30–50% 的受体可位于分段表面的突触之外。它们使用分段的全部面积（当 `pre_area`/`post_area` = 0 时回退到几何面积）。

### 在 BioNeuronCore 中的体现

- `Synapse::pre_area` 与 `Synapse::post_area`（默认值 0.1 μm²）通过 `Modifier::state[7]`（scratchpad）缩放受体的电导。
- `state[7]` 在每次分派前于 `simulation_host.cpp`、`simulation.cpp` 和 SYCL 内核中设置。
- 若 `pre_area`/`post_area` = 0 或未设置，则使用分段的几何面积（向后兼容）。
- 受体修饰符（离子型与代谢型）与突触前调节剂通过 `effective_area(m, s)` 读取 `state[7]`。

---

## 权利与知识产权

© 2024–2026 **费申·维塔利·亚历山德罗维奇** / Vitaly Alexandrovich Feshin。保留所有权利。
电子邮箱：feshin2004@mail.ru · vfeshin@mindcopy.ru · 电话：+7(918)345-75-18。俄罗斯克拉斯诺达尔 / Krasnodar, Russia。

计算机程序国家注册证书（俄罗斯专利局）**编号 727894_683451**。
未经作者书面许可，禁止商业使用、传播和再许可。
全文请参见平台根目录中的 `LICENSE` 与 `DISCLAIMER.md`。
