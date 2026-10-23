# 发布说明 — BioNeuronCore

## 变更（2026-10-09 — 2026-10-12）— CPG 交替（d→d 修复）+ CUDA 13

### 蝌蚪 CPG — 交替 0→4 次切换
- **d→d 密度修复**（`D2D_AMPA_R` 76→2000，= d→c）：Ferrario 2018 — din→din ampa = din→cin ampa = 0.593 nS；此前弱 26 倍 → d→d 不传导，NMDA Mg²⁺ 阻断未在峰电位解除。
- **HdIN 上行分支**（Roberts 2010）：吻侧 dIN 的 d→d `seg0↔seg0` — 对游泳自持至关重要（缺失则吻侧 dIN 失效）。
- **d→d 收敛归一化**：每突触密度按实际收敛（消除尾侧 dIN +18 mV 平台）。
- **甘氨酸加深**（`GLY_REC` 80→160）— dR 反弹更稳健（Ih_m 0.42，Na_h 0.76）。

### CUDA 13（GPU 后端 / ISO 镜像）
- 驱动 580.178.04 = CUDA 13.0；运行时 `cuda-cudart-13-0`（libcudart.so.13）替代 libcudart12 — 修复 toolkit↔driver 不匹配（`cudaMallocManaged` err=46）。

## v0.20.0 之后的变更（2026-09-25 — 2026-10-02）— NAK→亚型、删除代理、新修饰符、BION v12

### 新修饰符（MOD_TYPE_COUNT 187→198）
- **Mrgpr (188)** — 代谢型痒觉受体（Gαq/Gβγ → Ca²⁺ → TRPA1）。
- **PIEZO2 (189)**、**TMC1 (190)**、**Cav3.3 (191)** — 机械感受器与 T 型 Ca²⁺。
- **视紫红质光转导 (192)** — 光→R*→Gt→PDE→cGMP↓→CNG（视杆超极化）。
- **Nav1.2 developmental (193)** — 蝌蚪轴突 Na⁺（Ribera & Spitzer）。
- **NaP-tadpole (195)** — dIN 的持续 Na⁺（Svensson 2017）。
- **Kv2.1 (196)** — 慢速延迟整流 K⁺（单峰 dIN）。
- **Na/K-ATP酶 α3 (197)** — 动态泵，爆发时产生 usAHP（Zahler 1997）。

### 删除代理（单一来源 / 最大生物性）
- 删除 Ohm 代理 80/81（`MOD_GENERIC_VOLTAGE_GATED`/`MOD_GENERIC_LIGAND_GATED`）。
- 删除 NAK 代理：`MOD_IONOTROPIC_NAK` (0)、`DIN_NAK` (125)、`CIN_NAK` (138)、`MN_NAK` (139)、`RB_NAK` (143)、`AXON_NAK` (194) → 由 Nav1.2/Nav1.6/NaP 亚型替代。
- 删除 `MOD_POST_INHIBITORY_REBOUND` (93) → 通过 HCN + T-Ca²⁺ 实现回弹。槽位 124 已保留。
- 神经元迁移至亚型规范（Nav1.2/Kv1.1-KDR/NaP，Ca_leak 0.001）。

### 生物物理 / 格式
- **BION v9 → v12**：Segment 1692→1800 字节，Synapse 856 字节。向后兼容 v3-v11。
- **ION_COUNT 5→7**：+A⁻（大阴离子）、+H⁺（质子，OTOP1）。**NT_COUNT 14→15**：+辣椒素。
- **NeuroML 导入/导出**：Ohm↔GHK 转换（γ ↔ condDensity、forwardRate/reverseRate）。
- **扩散**：extra_ions 向储库的 bath 弛豫、φ_e 限幅 ±10 mV、sg_bernoulli small-x 阈值。

### 测试
- 156 个 CTest 测试（原为 55）。

## v0.20.0 (2026-09-04) — 18 个新修饰符、AQP4、转运体、调制、PNP φ_e

### 新修饰符（MOD_TYPE_COUNT 169→187）
- 伤害感受性 Nav（Nav1.7/1.8/1.9）+ TRP 温度传感器（TRPV1/TRPM8/TRPA1/TRPV3）+ KCNQ4。
- Nav1.3、Kir7.1、TRPC6/7、CALHM1/3、OTOP1、ENaC（感觉/代谢）。
- 神经元转运体 EAAT3/GlyT2/GAT1（SLC1/SLC6，共享辅助）。

### 生物物理学
- **AQP4**（水通道蛋白-4）：水通道、渗透转运（自足，无 BION bump）。
- **调制**：TEA→Kv3.1（IC50 0.2 mM）、PKC→Kv3.1（Ser503 磷酸化状态、PP1/PP2A）、O₂→TASK（代谢）。
- **完整 PNP**：胞外泊松方程 φ_e（solve_poisson_phi_e，雅可比）+ 电迁移 + V_m=φ_i−φ_e。

### 测试
- test_nociceptor_channels、test_other_channels、test_gating_dynamics、test_modulation、test_transporters_neuronal。

## v0.19.9 — GOLDEN：stub 清理、daemon build_id/mode、基因流水线
**日期：** 2026 年 7 月 10 日
- **Daemon**：MSG_GET_VERSION 中的 build_id（DJB2 哈希），MSG_GET_NETWORK_INFO 中来自 BION 头部的 mode。
- **Nucleus**：偏移注释已更新（GENE_COUNT 36→46），opaque API 中的 core_nucleus_size()。
- **myelin_modulator**：记录为 by-design（通过 segment_has_myelin() 分派产生效果）。
- **GPU consolidation**：为 GPU 后端启用（USM 可从 CPU 访问）。
- **docs**：README/CLAUDE/relnotes 已更新（BION v9、MOD_TYPE_COUNT=124、测试 55/55）。

## v0.19.8 — BION v9（IntraConnection 16B）+ MOD_KV_DELAYED_RECTIFIER
**日期：** 2026 年 7 月 9 日
- **BION v9**：IntraConnection 12→16 字节（XMM 溢出修复）。添加 `_pad` 以实现 16 字节对齐。BION_VERSION=9，通过 `connection_file_size` 向后兼容 v3-v8。
- **MOD_KV_DELAYED_RECTIFIER (123)**：Kv1.1/Kv2.1 延迟整流 K⁺ 通道。GHK K⁺ 通量，m-门，V_half=−30mV。
- **MOD_TYPE_COUNT**：123→124（+KvDR）
- **sizeof(BionIntraConnection)**：12→16 字节。bion_loader：按版本自动检测大小。
- 测试：55/55。

## v0.19.7 — MOD_NKCC1（Cl⁻ 开关）+ BION v8 审计
**日期：** 2026 年 7 月 8 日
- **MOD_NKCC1 (122)**：Na-K-2Cl 共转运体 — 电中性 Cl⁻ 内流。KCC2 的拮抗剂。Xenopus 个体发育 Cl⁻ 开关的关键。
- **MOD_TYPE_COUNT**：122→123（+NKCC1）
- **BION v8**：bion_version_string() 已修复（原硬编码为 "7"）。
- **sizeof(Segment)**：注释 912→1692 字节。
- 测试：55/55。

## v0.19.6 — IPC 状态帧修复 + daemon 安全默认值
**日期：** 2026 年 7 月 2 日
- **IPC EAGAIN 修复**：`sendBuffer()` — 向非阻塞 fd 写入大帧（2.6MB）时通过 `poll(POLLOUT)` 处理 EAGAIN/EWOULDBLOCK。若无此修复，帧会被静默丢弃。
- **daemon 安全默认值**：`gpu_enabled=false`（默认仅 CPU）、`tcp_port=0`（无 TCP）。生产环境在配置中显式设置。
- **GPU 到 CPU 回退**：GPU 失败时 `loadNetwork()` 不再失败，而是切换到 CPU 路径。
- **sim_ipc_equiv**：6/6 PASS（原为 0/6），sim_ipc_bridge：All PASS（原为 All FAIL）

## v0.19.5 — CUDA 双遍扩散 + CPG 调优 + Agent Skills
**日期：** 2026 年 7 月 1 日
- **CUDA 双遍扩散修复**：`kernel_diffusion` 拆分为 `kernel_diffusion_compute` + `kernel_diffusion_apply` — 消除 extra_ions 上的 atomicAdd 竞争条件。CPU-GPU 一致性：221 个断言，delta=0.29mV
- **CPG 配置**：K2P=0.06、PUMP=0.40、NAK_HH=0.8、SK_CIN=1.8、GLY_TAU=80ms — 针对 V_rest ~-67mV 校准
- **Agent Skills**：CLAUDE.md 中的 DDD 方法论 + 调试分诊（5 个阶段）+ 规划工作流
- **--gpu-device 标志**：daemon 在 batch 模式下接受 GPU 设备索引
- **GPU 设备标志**：`BIOCORE_GPU_DEVICE` 环境变量 + `--gpu-device N` 用于选择 GPU

## v0.19.4 — 日志统一 + daemon 修复
**日期：** 2026 年 6 月 25 日
- **统一日志格式**：所有子项目（Core/Studio/Orch）中 unixtime [LEVEL] msg
- **64 处 fprintf/printf → DLOG_INFO/WARN/ERROR**：7 个生产文件（daemon_main、daemon_server、daemon_config、bion_loader、bion_loader_host、plugin_manager、gpu_backend）
- **daemon 修复**：freeNetwork() 之前的 stopSim() — 防止在未停止模拟的情况下断开连接时的竞争
- **从 receptors_biological.cpp 删除死代码 nernst()**（GHK 迁移）
- **静默 17 个警告**（65→2）：未使用的变量、fread/write 返回值
- **33/33 测试**，sim_ipc_bridge 203/203

## v0.19.3 — 教育实验（三语 README）
**日期：** 2026 年 6 月 23 日
- 16 个教育性 .bexp 实验：4 个本科 + 8 个硕士 + 4 个博士
- 每个均配有俄语、英语和中文 README
- 主题：动作电位、无源电缆、突触整合、侧向抑制、CPG、工作记忆、感受野、STDP（LTP/LTD/完整窗口）、癫痫、药理学、稳态可塑性、分岔

## v0.19.2 — 实验重组
- 4 个内置实验（motion、direction、stdp_ltp/ltd）移至 graduate/
- stdp_ltp 和 stdp_ltd 保留为独立项 + 合并的 stdp_learning
- 带全部 16 个实验表格的索引 README

## v0.19.1 — 12 个新教育实验
- 为所有学习层次生成 .bexp 配方
- 带生物学解释、验证与复现方法的 README

## v0.19.0 — MSG_LOAD_BEXP + --load-bexp batch 模式
**日期：** 2026 年 6 月 23 日
- `MSG_LOAD_BEXP` (0x0108)：将 .bexp 作为实验配方加载
- batch 模式中的 `--load-bexp`：从 JSON 头部自动提取 bion_file_path
- 解析 .bexp 中的 events（刺激），设置 detection 配置

## v0.18.0 — P0 修复：任务进度、优雅关闭、自动检查点、分层影子验证
- `MSG_GET_TASK_PROGRESS`：通过 task_start_tick/task_total_ticks 提供真实的 current/total/percent
- 优雅关闭：batch 模式中 SIGTERM → 紧急检查点 bion_save → exit(75)
- 服务器 simulationLoop 中的自动检查点（SessionContext 中的 checkpoint_interval）
- 生物影子验证：COLD 分段对照无源模型（I_leak）进行校验
- 偏差 > 0.1mV 时 → 自动提升至 WARM/HOT

## v0.17.3 — Batch 模式集成测试
- `test_batch_mode.cpp`：在活动 test_single_neuron.bion 上的 7 个测试、14 个断言
- 冒烟测试（二进制存在、help）、验证（拒绝缺失参数）、集成（BEXP 输出）、参数 JSON、检查点

## v0.17.2 — MSG_RUN_TASK_PARAMS 返回指标
- MSG_RUN_TASK_PARAMS 处理器中的 core_compute_metrics()
- 指标：mean_vm、spike_count、synchrony、按神经元类型划分的 V_m

## v0.17.1 — batch 模式中的检查点间隔 + 恢复支持
- `--checkpoint-interval N`：每 N 个 tick 自动保存 BION
- `--resume-from PATH`：从检查点恢复
- agent 中的 volunteer_name

## v0.17.0 — 分布式计算阶段 1-5
**日期：** 2026 年 6 月 23 日
- **阶段 1：** daemon batch 模式（`--batch --load-bion --ticks --bexp-output --params-json --seed`）、带参数覆盖的 MSG_RUN_TASK_PARAMS (0x090A)
- **阶段 2：** simulation_host.cpp 中 tiered_execution 标志后的分层执行（HOT/WARM/COLD 分类）
- **阶段 3：** core_compute_metrics() — 确定性的 V_m/spike/synchrony 指标
- **阶段 5：** MSG_CHECKPOINT_AND_CONTINUE (0x0804)、MSG_GET_TASK_PROGRESS (0x090C)
- **测试：32/32**

## v0.38.3 — 插件测试 + SDK 示例

**日期：** 2026 年 6 月 22 日

- **test_plugin_compat：** 5 个测试验证所有 .so 插件（dlopen → init、RTLD_NOW+RTLD_LAZY、已删除符号的 stale 检测）。
- **SDK 示例：** `example_ca_channel.so` 与 `example_d2_receptor.so` 已按当前 API 重新构建（`register_modifier_current` 取代已删除的 `register_user_modifier`）。
- **dispatch_generated.cpp：** 已重新生成 — 用户类型 121-122 纳入 GPU 分派器。
- **测试总数：20**（原为 19）。

## v0.38.2 — 每会话隔离 + 测试覆盖 28/28

**日期：** 2026 年 6 月 22 日

- **每会话隔离（#1-#3）：** 巩固、延迟缓冲与 active_library 的全局变量移入 SessionContext。新增函数的 `_ctx` 变体：`core_init_consolidation_ctx`、`core_run_tick_ctx` 等。`TickSessionPtrs` — 无全局变量的单一实现。
- **Daemon 修复：** 所有会话的 streamer flush，removeClient 从所有 streamer 清除 fd。
- **bion_save 修复：** `BION_MAGIC` + 从 `BionData` 填充所有头部字段（此前保存检查点时 magic 为零）。
- **测试：28/28**（原为 21）
  - `test_daemon_protocol_full.cpp` — 32 个测试、27 个 MSG 处理器（此前均未覆盖）
  - `test_plugin_lifecycle.cpp` — 15 个测试（.so 生命周期、名称、.info、边界）
  - `test_core_security.cpp` — 15 个测试（损坏的 BION、无效头部）
  - `test_core_chaos.cpp` — 10 个测试（nullptr、双重 init、快速循环）
  - `test_core_regression.cpp` — 17 个测试（sizeof、常量、bion_validate）
  - `test_core_smoke.cpp` — 4 个测试（daemon 启动、PING、VERSION、PLUGINS）
  - `test_core_performance.cpp` — 6 个测试（run_tick、BION I/O、插件计时）
- **测试平台：** Catch2 v2（单头文件），通过 ctest 运行 28 个可执行文件。

## v0.38.1 — 解析器统一 + daemon_server 修复

**日期：** 2026 年 6 月 19 日

- **daemon_config.cpp：** 切换到 BioNeuronOrchestrator 的共享 `json_parser.h`
- **daemon_server.cpp：** 修复约 40 个编译错误
  - 添加 `default_session_`（会话 0）
  - 字段名：`segments()` → `_segments()`（及类似项）
  - `SessionContext::freeNetwork()` — 内存释放的内联定义
  - `MAX_STIM` → `SessionContext::MAX_STIM`
  - `sim_thread_` → `current_ctx_->sim_thread`
  - `simulationLoop()` — 非静态方法（使用 `current_ctx_`）
  - 每会话包装：`loadNetworkForSession()`、`startSimForSession()` 等
  - `isNetworkLoaded()`、`segs()`、`syns()` 等 — 内联定义
- **daemon_config.cpp：** 字段验证：tcp_port、max_sessions、log_max_size_mb、log_max_files
- 包含路径：CMakeLists.txt 中为 daemon_config 添加 `-I../BioNeuronOrchestrator`

## v0.38.0 — 将 Core/Studio 拆分为两个进程（IPC）

**日期：** 2026 年 6 月 18 日

- **Core Daemon：** 守护进程 `biocore-daemon` — 独立进程，监听 Unix 套接字 + TCP（端口 6789）。
- **IPC 协议：** 二进制线路协议（30+ 种消息类型：sim、stimuli、state、plugins、consolidation、checkpoint）。
- **状态流：** STATE_FRAME（原始 Segment/Synapse 数组），用于向 GUI 高效传输 V_m/离子/NT。
- **配置：** `/etc/biocore/biocore.conf`（JSON），不含个人数据。
- **Systemd 单元：** `biocore-daemon.service`，用户 `biocore`，沙箱安全。
- **日志系统：** `daemon_log.h` — 按大小轮转（与 Studio 相同），`[timestamp] [LEVEL]`。
- **插件：** 111/111 个插件加载成功（`-rdynamic` + `RTLD_LAZY`）。
- **GPU 后端：** 在 daemon 中 dlopen `libbiocore_gpu_backend.so`，simulationLoop 中的 GPU 路径。
- **参数：** `--config`、`--plugins-path`、`--ready-fd N`。
- **打包：** `mindcopy-daemon` .deb 包（preinst/postinst/prerm/postrm）。
- **测试：** `test_daemon_protocol`（9 个协议测试）。

## v0.16.7 — BION 加载器修复与测试更新

**日期：** 2026 年 6 月 16 日

- **bion_loader_host.cpp：** 添加错误码 BION_ERR_FILE_READ (-11)，全部 8 处 fread 调用均包裹返回检查。
- **bion_format.h：** `bion_version_string()` 已修复 — 返回 "6"（而非 "5"）。
- **文档：** CLAUDE.md/README.md — BION v4/v5→v6、MAX=4→6。
- **测试：** 更新常量（sizeof(Nucleus)=332、BION_VERSION=6、MOD_TYPE_COUNT=121）。Core：18/18 测试。

## v0.16.6 — 电感受 + 磁感受（TODO 3：第 3+4 部分）

**日期：** 2026 年 6 月 15 日

- **MOD_ELECTRORECEPTOR_AMPULLARY (118)** — 洛伦齐尼壶腹：凝胶通道（τ=100ms，低通）、Ca²⁺ 适应（τ=5s）、方向性调谐、GHK（Na⁺50%/K⁺35%/Ca²⁺15%）
- **MOD_MAGNETITE_MECHANORECEPTOR (119)** — Fe₃O₄ 链：扭矩 τ=m×B → MET 通道、sin²(θ) 依赖、Ca²⁺ 适应（τ=8s）、GHK（Na⁺45%/K⁺30%/Ca²⁺25%）
- **MOD_CRYPTOCHROME_MAGNETIC (120)** — Cry4 自由基对 [FAD•⁻+Trp•⁺]：光依赖（λ=400-500nm）、sin²(θ) 角度依赖、调制器（调制 MOD_LIGHT_SENSOR）
- MOD_TYPE_COUNT=121

## v0.16.5 — 前庭器官（TODO 3：第 2 部分）

**日期：** 2026 年 6 月 15 日

- **MOD_OTOLITH_HAIR_CELL (116)** — 耳石毛细胞：MET 通道、余弦调谐、肌球蛋白马达的 Ca²⁺ 适应（τ=30ms）、紧张性 P_open~0.15（重力）、GHK（Na⁺60%/K⁺30%/Ca²⁺10%）
- **MOD_SEMICIRCULAR_CUPULA (117)** — 半规管壶腹帽：扭转摆（ω₀²、2ζω₀）、推拉双侧性、时相性 Ca²⁺ 适应（τ=10ms）、GHK（Na⁺55%/K⁺35%/Ca²⁺10%）
- MOD_TYPE_COUNT=118

## v0.16.4 — 听觉趋向性（TODO 3：第 1 部分）

**日期：** 2026 年 6 月 15 日

- **MOD_VIBRATION_SENSOR (115)** — 振动/声学传感器：机械转导、S 型激活、习惯化+敏化、GHK
- MOD_TYPE_COUNT=116

## v0.16.3 — 感觉适应（H2）

**日期：** 2026 年 6 月 14 日

- **双过程适应模型**，适用于全部 4 个感觉修饰符：
  - 反向（习惯化）：GRK/β-arrestin/Ca²⁺-CaM 失活
  - 同向（敏化）：cAMP/PKA/Ca²⁺-恢复蛋白启动
- **MOD_PHYSICAL_SENSOR (6)**：+习惯化（state[2]）、+敏化（state[3]）、可配置 k_hab/k_sens/tau_hab
- **MOD_LIGHT_SENSOR (95)**：+敏化（state[3]、tau=60s、k_sens=0.5 — Ca²⁺/恢复蛋白/GCAP）
- **MOD_OLFACTORY_RECEPTOR (108)**：+敏化（state[3]、tau=10s、k_sens=0.3 — cAMP/PKA）
- **MOD_HUNGER_SENSOR (94)**：+习惯化（state[1]、tau=300s — AMPK/mTOR）、+敏化（state[2]、tau=600s）
- **共享辅助** 在 `plugin_helpers.h` 中：`sensor_habituation_step()`、`sensor_sensitization_step()`、`sensor_adaptation_gain()`
- 物理与光传感器的 CPU/GPU delta = 0.00 mV
- GPU 回退（`hh_biological.cpp`）与 CPU 插件同步

## v0.16.2 — MOD_CONNEXIN_CX36 + 缝隙连接延迟（G2）

**日期：** 2026 年 6 月 12 日

- **MOD_CONNEXIN_CX36 (114)**：缝隙连接的分子生物学
  - 在 ER 中合成 Cx36 → 连接子组装（6×Cx36）→ 膜
  - 磷酸化：CaMKII→↓P_open、PKA(cAMP)→↑P_open
  - pH 依赖：pH<6.8 时关闭
  - 集成到缝隙连接：g_eff *= Cx36_pre * Cx36_post
- **MOD_TYPE_COUNT** 114→115
- 18/18 测试

## v0.16.1 — F2+F3+G1：密度事件 + Ca²⁺→CREB + 缝隙连接延迟

**日期：** 2026 年 6 月 11 日

- **F2 — 基因→修饰符密度事件**：
  - types.h 中的 CONSOL_EVENT_MODIFIER_DENSITY (4)
  - 基因→修饰符映射（SCN1A→NaK、KCND2→Kv4.2、KCNQ2/3→Kv7 等）
  - applyPendingEvents：密度事件 → segment.modifiers[].density
- **F3 — Ca²⁺→CREB→TF 内在稳态**：
  - cacheSignalsFromSoma：Ca²⁺_soma → pCREB（Hill n=2、Kd=0.5μM）
  - pCREB → TF 表达 → 离子通道基因 → 蛋白质 → 密度
- **G1 — 缝隙连接延迟缓冲**：
  - I_gap 上的一阶 RC 滤波器（tau=delay_ms）
  - CPU（simulation_host.cpp）+ GPU（kernels_part2.sycl）
- 18/18 测试

## v0.16.0 — BION v6 + 36 个基因 + GRN（F1）

**日期：** 2026 年 6 月 11 日

- **GENE_COUNT** 20→36：6 个 TF + 10 个离子通道
- **Nucleus** 204→332 字节
- **BION_VERSION** 5→6，v5→v6 迁移
- 新基因：TF_LHX3、TF_ISLET1、TF_PAX2、TF_BRN3A、TF_MNX1、TF_LMX1B、
  SCN1A(Nav1.1)、KCND2(Kv4.2)、KCNQ2、KCNQ3、CACNA1G(Cav3.1)、HCN1、HCN2、
  KCNJ2(Kir2.1)、KCNK2(K2P)、KCNMA1(BK)
- ConsolidationThread 中的 TF→离子通道基因调控
- 18/18 测试

## v0.15.0 — 生物 STDP + BCM θ_m + PP1/DARPP-32（C1+C3）

**日期：** 2026 年 6 月 11 日

- **已删除代理时序追踪**：syn->facilitation/depression 不再是手动计时器
- **BCM θ_m**：syn->depression 中 Ca²⁺² 的滑动平均（τ≈5min）
- CaMKII（Kd=1μM）与 Calcineurin（Kd=0.05μM）自然区分 LTP/LTD
- **MOD_PP1_INHIBITOR_DARPP32 (113)**：D1→cAMP→PKA→p-DARPP32→抑制 PP1→LTD↓
- PP1_active 集成到 AMPAR 运输
- 从 CPU 和 GPU 删除 spike 前 facilitation=1.0
- **MOD_TYPE_COUNT** 112→114
- 18/18 测试

## v0.14.5 — MOD_KV7_MCURRENT（KCNQ2/3 M 电流）

**日期：** 2026 年 6 月 11 日

- **MOD_KV7_MCURRENT (112)**：慢 K⁺ 通道（tau~100ms）、PIP2 依赖
- GHK K⁺ 通量、V_half=-38mV、无失活
- CPU+GPU、.so 插件、6 个生物物理测试
- CPU=GPU：delta=0.00mV
- **MOD_TYPE_COUNT** 112→113
- 18/18 测试

## v0.14.4 — K+ 守恒 + GPU-CPU 一致性 + MOD_THERMAL_NOISE

**日期：** 2026 年 6 月 11 日

- **K+ 离子守恒**（hh_biological.cpp）：
  - 向 5 个通道添加 extra_ions[K]：K2P_LEAK、KIR_CHANNEL、SK_CHANNEL、BK_CHANNEL、PIR_HCN
  - 修复 Kv4.2 与 Generic V-gated 中的符号（intra/extra 曾反转）
- **GPU-CPU 一致性**（kernels_part1.sycl、simulation.cpp）：
  - 向 GPU update_membrane 添加热噪声 ±0.05mV（与 CPU 相同）
  - 将 Tick 传入 GPU 内核以实现可复现噪声
  - NT 竞争条件修复：`=` → atomic fetch_add（GPU）/ `+=`（CPU）
  - GPU 扩散中的 extra_only 保护（胶质连接跳过 intra）
  - V_m 保护 CPU/GPU 对齐 [-200, +100]
  - GPU 路径中的 STDP/可塑性（CaMKII/Calcineurin/eCB）（步骤 3.5）
- **MOD_THERMAL_NOISE (111)**：xorshift32 PRNG 上的随机噪声
  - .so 插件 + GPU 回退 + 分派
  - state[0]=σ(mV)、state[1]=seed_offset
- **MOD_TYPE_COUNT** 111 → 112
- 18/18 测试

## v0.14.3 — 完整 biological_library + GPU 安全的 GHK

**日期：** 2026 年 6 月 10 日

- `receptors_biological.cpp`：将类型 98-107（PMCA、NCX、KCC2、Ca_leak、Ca_buffer、
  SK、BK、NaP、KIR、K2P）加入 `init_biological_library()`。修复
  无插件的 CPU 路径（此前使用 NULL 指针）。
- `hh_biological.cpp`：`k2p_leak_current` 中的 `safe_ghk_denom()`，epsilon 1e-7f
  （防止 GPU 上的 fp32 下溢）。
- 18/18 测试 PASS。

## v0.14.2 — GPU NaN 修复 + 生物参数

**日期：** 2026 年 6 月 10 日

- **关键 GPU 修复：**
  - `kernels/kernels_part1.sycl`：当 `coupling_resistance==0` 时跳过轴向电流
    （胶质 `connect_extra` 导致除以 0 → NaN）
  - `kernels/kernels_part1.sycl`：`update_membrane` 中的 NaN 保护（`V_m!=V_m` → -65mV）
  - `kernels/kernels_part2.sycl`：释放公式 `(1+facil)/(1+depress)`
    取代 `facil/depress`（静息时 0/0 → NaN）
  - `plugin_helpers.h`、`receptors_biological.cpp`：`safe_ghk_denom()` epsilon 1e-7f
  - `hh_biological.cpp`：NaK_HH — 电导现带 `*0.01f`（如 CPU 插件），
    修复离子浓度 100 倍漂移
  - `simulation.cpp`：扩散后的浓度 NaN 保护
- **生物参数（CPU 插件 → GPU 值）：**
  - NaP：g_max 1.0→0.08（BIOREF 0.01-0.1 mS/cm²）
  - SK：g_max 1.5→0.5（BIOREF 0.5-5 mS/cm²）
  - BK：g_max 2.5→2.0（BIOREF 1-20 mS/cm²）
  - KCC2：v_max 0.002→0.0008（BIOREF 默认值）
  - KIR：g_max 1.5→1.0（BIOREF 0.1-1 mS/cm²）
- 测试：`test_hh.cpp`、`test_synapses.cpp` — 阈值已更新
  以适应正确的生物缩放

## v0.14.1 — 扩展测试覆盖

**日期：** 2026 年 6 月 10 日

- +5 个测试文件：consolidation_plasticity、delay_buffer、gpu_backend、plugin_manager、v3v4_migration
- 合计：18/18 测试，100% PASS
- Valgrind：无泄漏

## v0.14.0 — 协调切换到生物物理单位

**日期：** 2026 年 6 月

### 生物物理迁移（协调切换）
- **C_SPECIFIC_PF_PER_UM2 = 0.01**（1 µF/cm² = 0.01 pF/µm²）— 生物物理比电容
- **所有电流缩放到生物物理单位：**
  - 基于电导：`g × density × area × dV × 0.01`
  - 基于 GHK：`P × density × area × 1e-11f`（取代 1e-9f）
  - 泵/NCX：生物离子转运 + 缩放回流（×0.01）
- **移除全局补偿：** 从 GPU 和 CPU 路径删除 `I_buffer *= C_SPECIFIC_PF_PER_UM2`
- **转换 38+ 个文件：** 34 个 .so 插件 + 2 个回退文件 + plugin_helpers.h + 2 个模拟内核
- **C_m 现为生物物理量：** `C_m = area × 0.01`（pF），clamp 0.1 pF

### 测试修复
- `test_consolidation`：MOD_TYPE_COUNT 109→111
- `test_bion_loader_host`：coupling_resistance 从几何重新计算（1e6×L/πr²）
- `test_synapses`：C_m 公式更新为 C_SPECIFIC

### 总体
- `MOD_TYPE_COUNT`：111（添加 MOD_GLIAL_KIR41=109、MOD_GLIAL_NAK_PUMP=110）
- Core：**13/13 PASS**（0 失败）
- Studio：**162/162 PASS**（0 失败）

## v0.13.0 — Ca²⁺/K⁺/Cl⁻ 稳态 + 节律发生通道 + STDP

**日期：** 2026 年 6 月

### Ca²⁺/K⁺/Cl⁻ 稳态（5 个新修饰符：98-102）
- **MOD_PMCA** (98) — 质膜 Ca²⁺-ATP 酶：ATP 依赖挤出 1Ca²⁺，Hill n=2，K_m=300nM，产电
- **MOD_NCX** (99) — Na⁺/Ca²⁺ 交换体：3Na⁺ 入 : 1Ca²⁺ 出，V 依赖，变构 Ca²⁺ 激活
- **MOD_KCC2** (100) — K⁺-Cl⁻ 共转运体：电中性 1K⁺:1Cl⁻ 出，对甘氨酸/GABA-A 抑制至关重要
- **MOD_CA_LEAK** (101) — 被动 Ca²⁺ 泄漏：GHK Ca²⁺（z=2），无门控，维持基础 [Ca²⁺]i
- **MOD_CA_BUFFER** (102) — 快速 Ca²⁺ 缓冲：类小清蛋白，on/off 动力学，B_total=150μM，K_d=500nM

### 节律发生离子通道（4 个新修饰符：103-106）
- **MOD_SK_CHANNEL** (103) — SK（KCa2.x）：纯 Ca²⁺ 激活 K⁺，AHP，Hill n=4，K_d=400nM，GHK K⁺
- **MOD_BK_CHANNEL** (104) — BK（KCa1.1）：Ca²⁺+V 激活 K⁺，快速复极化，V_half 的 Ca²⁺ 偏移，GHK K⁺
- **MOD_NAP_CHANNEL** (105) — NaP（Nav1.6）：持续 Na⁺，无失活，阈下激活（-52mV），GHK Na⁺
- **MOD_KIR_CHANNEL** (106) — Kir（Kir2.x）：内向整流 K⁺，玻尔兹曼+整流，RMP 稳定，GHK K⁺

### STDP 重构
- 连续 CaMKII/Calcineurin 机制 + 时序追踪调制（facilitation/depression）
- 前-后 → 提升 LTP 速率。后-前 → 提升 LTD 速率
- 消耗后重置时序追踪（防止误触发）
- 抑制性突触（GABA-A、GABA-B、Glycine）：反转 dw 符号

### Na/K 泵调优
- 默认 p_rate：0.1 → 0.15（插件 + GPU 回退）
- 生物学依据：CPG 神经元中不同的 α 亚基异构体

### 架构原则
**全部 9 个新修饰符均通过 GHK + 直接离子运动工作。**
没有任何一个 `I = g*(V-E_rev)`。每个修饰符显式更新 `s->intra_ions[]`。

### 总体
- `MOD_TYPE_COUNT`：98 → 107
- 所有插件：`.cpp` + `.info`（完整神经生物学描述）
- GPU 回退：`hh_biological.cpp` 中的全部 9 种类型 + `kernel_dispatch.h` 中的 switch 分支
- CPU：单独的 `.so` 插件（通过 CMake glob 自动构建）

### 测试
- Core：13/13 — 100%
- Studio：158/159（sim_regression — 预先存在）

## v0.12.0 — 缝隙连接门控 + D1/Gi/o 修复

**日期：** 2026 年 6 月

### 电突触（缝隙连接）
- **快速 Vj 门**：玻尔兹曼曲线（V_half=40mV、k=5mV、tau=5ms），残余电导 10%
- **慢速 Ca²⁺/CaM 门**：钙调蛋白的 Hill 激活（n=2、EC50=300nM、tau=100ms），通道完全关闭
- 门控状态存储于 `Synapse::facilitation`（m_fast）与 `Synapse::depression`（m_slow）— `.bion` 格式不受影响
- CPU（`simulation_host.cpp`）+ GPU（`kernels_part2.sycl`）

### 电流符号修复
- **D1（Gs）与 Gi/o**：`gs_ghk_effect` 与 `gi_o_ghk_effect` 中 `return -(-I_K)` → `return -I_K`（`plugin_helpers.h`、`receptors_biological.cpp`）
- K⁺ 外流现被正确视为超极化电流

### 附带修复
- `mod_09_nmda.cpp`：修复 `ionotropic_ghk_current()` 的返回丢失
- `mod_63_nb_nmda_moradi_full.cpp`：从 `temp_mod()` 删除游离 GHK 代码
- `mod_67_nb_alpha7_er_nachr.cpp`、`mod_75_nb_msn_nmdar.cpp`：删除不完整的 GHK 块

### 测试
- Core：13/13（5 个新测试：3× 缝隙连接门控 + 2× Gi/o 超极化）
- 所有 `test_synapses` 测试均通过

## v0.11.1 — GPU/CPU 同步修复

**日期：** 2026 年 6 月

### 模拟内核修复
- **GPU compute_modifier_currents**：`I_buffer = I_total` → `I_buffer += I_total` — 刺激不再每个 tick 被覆盖
- **GPU update_membrane**：添加 `C_m < 1.0f` clamp（与 CPU 路径相同）
- **core_run_tick**：`duration_ms` 通过 `dt` 正确转换为 tick（原为 ms 与 tick 直接比较）
- **CurrentStimulus**：打包结构中的字段被 GPU 路径正确读取

### 测试
- 159 个自动化测试（Core + Studio），0 失败
- 新回归测试 `sim_regression_bugs`（28 项检查）
- `sim_gpu_bridge` 通过 SimulationBridge 测试 GPU 路径
- `sim_gpu_stim` 检查 GPU 路径的打包结构

## v0.11.0 — GHK 迁移 + PIR + 传感器

**日期：** 2026 年 6 月

### PIR：抑制后回弹（3 个新修饰符）
- `MOD_POST_INHIBITORY_REBOUND` (93) — 指向 96+97 的总括委托
- `MOD_PIR_TTYPE_CALCIUM` (96) — T 型 Ca²⁺（Cav3.x）：GHK Ca²⁺ 通量（z=2）、m³h 门控、去失活窗口（-70..-30mV）、5-HT/DA 抑制
- `MOD_PIR_HCN_CHANNEL` (97) — HCN1/2/4（Ih）：分离的 GHK Na⁺(40%)/K⁺(60%) 通量、sag 效应、DA 抑制、E_rev 由通透性比率产生

### 传感器（2 个修饰符）
- `MOD_HUNGER_SENSOR` (94) — 内感受饥饿传感器：ATP 监测器 → 非选择性阳离子通道（Na⁺ 60%/K⁺ 30%/Ca²⁺ 10%），经 GHK
- `MOD_LIGHT_SENSOR` (95) — 光感受器（CNG 通道）：Na⁺ 50%/K⁺ 30%/Ca²⁺ 20%，经 GHK，适应（tau=2s）

### GHK 迁移（59 个插件）
- **完全放弃 I=g*(V-E)**：所有电流均通过 Goldman-Hodgkin-Katz 方程
- **在 `plugin_helpers.h` 中重写 3 个模板函数**：`ionotropic_ghk_current()`、`gi_o_ghk_effect()`、`gs_ghk_effect()`
- 9 个离子型受体（AMPA..TRPV1）→ GHK 阳离子/阴离子通量
- 24 个 Gi/o 受体 → 经 GIRK 的 GHK K⁺
- 11 个 Gs 受体 → GHK K⁺（通道关闭）
- 12 个 NB_ 受体 → 保留 Mg²⁺ 阻断的 GHK
- physical_sensor (06) → GHK 非选择性阳离子
- HH NaK (00) → E_leak = nernst(K)，取代硬编码
- **所有插件中 0 个硬编码 E_rev**

### 总体
- `MOD_TYPE_COUNT` = 98（原为 93）
- `ghk_monovalent()`、`ghk_divalent()`、`ghk_anion()` — `plugin_helpers.h` 中的共享辅助
- 所有插件直接移动离子（`s->intra_ions[X] += ...`），V_m 由电荷平衡更新

### 测试
- `test_consolidation.cpp`：MOD_TYPE_COUNT=98
- `test_simulation_host.cpp`：myelin 测试修复（噪声 + 100 tick 累积）
- 13/13 测试通过

---

## v0.10.0 — 巩固/再巩固（两阶段记忆）

**日期：** 2026 年 6 月

### 基因系统（Nucleus）
- `Nucleus` 结构（204 字节）：20 个可塑性基因、信号缓存、设置
- 20 个基因：CREB、c-Fos、Zif268、Arc、BDNF、Cofilin、MMP-9、TIMP、GluR1、GluR2、PSD-95、Neurexin、Neuroligin、Dynamin、Clathrin、IntegrinB3、TrkB、mTOR、S6K1、4E-BP
- `gene_activity[20]` — mRNA 水平（0..1），`protein_level[20]` — 蛋白质水平（0..1）

### 6 个新修饰符（87-92）
- `MOD_MAPK_ERK` (87)：Ca²⁺→Ras→Raf→MEK→ERK→pCREB，与 CREB 的串扰
- `MOD_IKK_NFKB` (88)：Ca²⁺→IKK→IkB→NF-κB→Zif268（再巩固）
- `MOD_MTORC1` (89)：双钥匙（AA + PI3K/AKT），ATP 抑制
- `MOD_PROTEIN_SYNTH` (90)：mTORC1→S6K1/4E-BP→翻译，消耗 ATP
- `MOD_COFILIN` (91)：ERK→LIMK→pCofilin/F-actin（肌动蛋白重塑）
- `MOD_MMP9` (92)：ERK+NF-κB→MMP-9/TIMP/整合素（ECM 降解）

### ConsolidationThread
- 后台线程，间隔可配置（0=关闭），每神经元覆盖
- 基因→蛋白质→结构：每周期 3 个阶段
- 巩固：pCREB→c-Fos/BDNF/Arc、ERK→Cofilin/MMP9
- 再巩固：NF-κB→Zif268（不同于 CREB 的路径）
- 结构可塑性：突触发生、LTD 移除（Calcineurin>0.3+weight<0.05）、Δ直径（Cofilin/F-actin）、ΔAMPAR（GluR1/GluR2）
- 再激活缓冲（1000 个 tick 的 V_m）用于记忆检测
- 步骤 2.5b：胞体→Nucleus 连接（pCREB/ERK/NF-kB/mTORC1/AA 缓存）
- 步骤 5：从队列应用 ConsolidationEvent

### BION v5
- NetworkHeader：68→84 字节（+num_nuclei、+nucleus_version、+reserved）
- .bion 中的 Nucleus[]：每神经元加载/保存 204 字节
- v4→v5 迁移：加载旧文件时自动创建默认 nuclei
- `bion_calc_size()` 已更新以计入 nuclei

### C API
- `core_init_consolidation`、`core_start_consolidation`、`core_stop_consolidation`
- `core_set/get_consolidation_interval`、`core_get_pending_consolidation_events`
- `core_get_nucleus_snapshot`

### 测试
- `test_consolidation`：15 个用例、44 个断言（结构、修饰符 87-92、集成）
- MOD_TYPE_COUNT：87→93

## v0.9.0 — 扩散扩展：intra_nt、extra_ions、neurotransmitter

### 通用电压门控通道（MOD_GENERIC_VOLTAGE_GATED = 80）
- 无硬编码的参数化 Hodgkin-Huxley 动力学
- state[]：来自 NeuroML gateHHrates 的 V½_fwd、k_fwd、rate_fwd、V½_rev
- 分离的前向/反向参数：alpha(V) 来自 forward，beta(V) 来自 reverse
- 三种动力学形式：HHExpLinearRate、HHExpRate、HHSigmoidRate
- 失活（h-门）具有独立的 V½_h

### 通用配体门控受体（MOD_GENERIC_LIGAND_GATED = 81）
- Hill 动力学（n=2），可配置 EC₅₀、tau_rise、tau_decay、g_max
- E_rev：能斯特（state[5]=0）或来自 NeuroML 的固定值

### 分子 STDP 级联（类型 82-86）
- MOD_CAMKII (82)：Ca²⁺/钙调蛋白依赖激酶 II，自磷酸化
- MOD_CALCINEURIN (83)：Ca²⁺/钙调蛋白依赖磷酸酶 PP2B
- MOD_CA_L_TYPE (84)：L 型 Ca²⁺ 通道（Cav1.x、HVA、CDI）
- MOD_KV42 (85)：带 CaMKII 磷酸化的 Kv4.2 A 型 K⁺ 通道
- MOD_ECB_SYNTHESIS (86)：由 Ca²⁺ 合成内源性大麻素（2-AG）
- simulation_host.cpp 步骤 3 中的 AMPAR 运输：CaMKII/Calcineurin → Δsyn.weight
- eCB 逆行信号：2-AG → 间隙 → 突触前 CB1

### Cm 修复
- CPU 路径现如 GPU 路径一样将 I_buffer 除以 C_m（膜电容）
- V_m += I_buffer / (2π·r·l) · dt — 生理动力学

### 修复
- MOD_TYPE_COUNT：80 → 87
- INFO_CACHE_SIZE：80 → 87
- 带分离前向/反向参数的参数化 alpha/beta
- NMDAR Mg²⁺ 阻断：符合性检测（谷氨酸 + 去极化）

## v0.9.0 — 扩散扩展：intra_nt、extra_ions、neurotransmitter
- intra_nt[NT_COUNT]、extra_ions/neurotransmitter 扩散、斯托克斯-爱因斯坦
- Segment：932 → 988 字节

## v0.8.0 — Core 中的刺激
- I_clamp、VoltageClamp、RampStimulus、IonInjection、LigandInjection

## v0.7.0 — CPU/GPU 切换
- gpu_backend.cpp、libbiocore_gpu_backend.so、dlopen

## v0.6.2 — state[7] scratchpad
- 用于突触接触面积的 pre_area/post_area

## v0.6.0 — BioNeuronSDK
- plugin-devkit.h、biocore-gen-dispatch、cmake 模块

## v0.5.0 — 63 个 .so 插件
- extern "C" 注册、modifier_current_table[128]

## v0.4.0 — BION v4
- 宿主加载器、插件基础设施（dlopen）

## v0.3.0 — 二进制 .bion v3
- 完整验证、v3→v4 迁移

## v0.2.0 — 突触
- 63 个受体、12 个突触前调节剂

## v0.1.0 — HH 通道
- Na/K、Ca、Cl、轴向电流、SYCL 内核

## v0.38.0 (2026-06-19) — 多客户端 + 多 GPU 守护进程

### 新增
- **多客户端隔离：** 通过一个守护进程独立模拟多个 Studio 的 SessionContext
- **多 GPU：** 枚举 SYCL 设备，在特定 GPU 上创建模拟
- **MSG_FLAG_SESSION：** 带会话（session_id）的向后兼容协议
- **MSG_CREATE/DESTROY_SESSION：** 通过 IPC 管理会话
- **MSG_LIST_DEVICES：** 获取 GPU 设备列表
- **MSG_RUN_TASK：** 通过 IPC 运行后台任务（无需 Studio）
- **GpuDeviceInfo：** types.h 中带设备信息的结构
- **DaemonConfig：** max_sessions、gpu_device_allowlist
- **验证：** 检查 gpu_device_index、session_id、bion_path、ticks

### 变更
- daemon_server：重构（会话注册表、兼容性别名）
- gpu_backend.cpp：device_count、get_device_info、create_on_device
- daemon_session.h/cpp：新文件 — SessionContext
- 错误类型：ERR_MAX_SESSIONS、ERR_INVALID_SESSION、ERR_GPU_DEVICE_UNAVAILABLE


## 权利与知识产权

© 2024–2026 **费申·维塔利·亚历山德罗维奇** / Vitaly Alexandrovich Feshin。保留所有权利。
电子邮箱：feshin2004@mail.ru · vfeshin@mindcopy.ru · 电话：+7(918)345-75-18。俄罗斯克拉斯诺达尔 / Krasnodar, Russia。

计算机程序国家注册证书（俄罗斯专利局）**编号 727894_683451**。
未经作者书面许可，禁止商业使用、传播和再许可。
全文请参见平台根目录中的 `LICENSE` 与 `DISCLAIMER.md`。
