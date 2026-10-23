# Release Notes — BioNeuronCore

## Changes (2026-10-09 — 2026-10-12) — CPG alternation (d→d fix) + CUDA 13

### Tadpole CPG — alternation 0→4 switches
- **d→d density fixed** (`D2D_AMPA_R` 76→2000, = d→c): Ferrario 2018 — din→din ampa = din→cin ampa = 0.593 nS; was 26× weaker → d→d did not conduct, NMDA Mg²⁺-block was not relieved on spike.
- **HdIN ascending branch** (Roberts 2010): d→d `seg0↔seg0` in rostral dIN — critical for swimming self-sustain (without it rostral dIN fail).
- **d→d convergence normalization**: per-synapse density by actual convergence (removed caudal dIN plateau +18 mV).
- **Glycine deepened** (`GLY_REC` 80→160) — dR rebound more robust (Ih_m 0.42, Na_h 0.76).

### CUDA 13 (GPU backend / ISO image)
- Driver 580.178.04 = CUDA 13.0; runtime `cuda-cudart-13-0` (libcudart.so.13) instead of libcudart12 — fixed toolkit↔driver mismatch (`cudaMallocManaged` err=46).

## Changes after v0.20.0 (2026-09-25 — 2026-10-02) — NAK→isoforms, surrogate removal, new modifiers, BION v12

### New modifiers (MOD_TYPE_COUNT 187→198)
- **Mrgpr (188)** — metabotropic itch receptor (Gαq/Gβγ → Ca²⁺ → TRPA1).
- **PIEZO2 (189)**, **TMC1 (190)**, **Cav3.3 (191)** — mechanosensors and T-type Ca²⁺.
- **Rhodopsin phototransduction (192)** — light→R*→Gt→PDE→cGMP↓→CNG (rod hyperpolarization).
- **Nav1.2 developmental (193)** — tadpole axonal Na⁺ (Ribera & Spitzer).
- **NaP-tadpole (195)** — persistent Na⁺ of dIN (Svensson 2017).
- **Kv2.1 (196)** — slow delayed rectifier K⁺ (single-spike dIN).
- **Na/K-ATPase α3 (197)** — dynamic pump, usAHP during bursts (Zahler 1997).

### Surrogate removal (Single Source / Max Biology)
- Ohm surrogates 80/81 removed (`MOD_GENERIC_VOLTAGE_GATED`/`MOD_GENERIC_LIGAND_GATED`).
- NAK surrogates removed: `MOD_IONOTROPIC_NAK` (0), `DIN_NAK` (125), `CIN_NAK` (138), `MN_NAK` (139), `RB_NAK` (143), `AXON_NAK` (194) → replaced by Nav1.2/Nav1.6/NaP isoforms.
- `MOD_POST_INHIBITORY_REBOUND` (93) removed → rebound via HCN + T-Ca²⁺. Slot 124 reserved.
- Neurons migrated to the isoform canon (Nav1.2/Kv1.1-KDR/NaP, Ca_leak 0.001).

### Biophysics / format
- **BION v9 → v12**: Segment 1692→1800 bytes, Synapse 856 bytes. Backward compatible v3-v11.
- **ION_COUNT 5→7**: +A⁻ (macro-anion), +H⁺ (proton, OTOP1). **NT_COUNT 14→15**: +capsaicin.
- **NeuroML import/export**: Ohm↔GHK conversion (γ ↔ condDensity, forwardRate/reverseRate).
- **Diffusion**: bath relaxation of extra_ions to the reservoir, φ_e capped ±10 mV, sg_bernoulli small-x threshold.

### Tests
- 156 CTest tests (was 55).

## v0.20.0 (2026-09-04) — 18 new modifiers, AQP4, transporters, modulation, PNP φ_e

### New modifiers (MOD_TYPE_COUNT 169→187)
- Nociceptor Nav (Nav1.7/1.8/1.9) + TRP thermosensors (TRPV1/TRPM8/TRPA1/TRPV3) + KCNQ4.
- Nav1.3, Kir7.1, TRPC6/7, CALHM1/3, OTOP1, ENaC (sensory/metabolic).
- Neuronal transporters EAAT3/GlyT2/GAT1 (SLC1/SLC6, shared helper).

### Biophysics
- **AQP4** (aquaporin-4): water channel, osmotic transport (self-sufficient, no BION bump).
- **Modulation**: TEA→Kv3.1 (IC50 0.2 mM), PKC→Kv3.1 (Ser503 phospho-state, PP1/PP2A), O₂→TASK (metabolic).
- **Full PNP**: extracellular Poisson φ_e (solve_poisson_phi_e, Jacobi) + electromigration + V_m=φ_i−φ_e.

### Tests
- test_nociceptor_channels, test_other_channels, test_gating_dynamics, test_modulation, test_transporters_neuronal.

## v0.19.9 — GOLDEN: stub cleanup, daemon build_id/mode, gene pipeline
**Date:** 10 July 2026
- **Daemon**: build_id (DJB2 hash) in MSG_GET_VERSION, mode from the BION header in MSG_GET_NETWORK_INFO.
- **Nucleus**: offset comments updated (GENE_COUNT 36→46), core_nucleus_size() in the opaque API.
- **myelin_modulator**: documented as by-design (effect via segment_has_myelin() dispatch).
- **GPU consolidation**: enabled for GPU backends (USM is available from the CPU).
- **docs**: README/CLAUDE/relnotes updated (BION v9, MOD_TYPE_COUNT=124, tests 55/55).

## v0.19.8 — BION v9 (IntraConnection 16B) + MOD_KV_DELAYED_RECTIFIER
**Date:** 9 July 2026
- **BION v9**: IntraConnection 12→16 bytes (XMM overflow fix). Added `_pad` for 16-byte alignment. BION_VERSION=9, backward compatibility v3-v8 via `connection_file_size`.
- **MOD_KV_DELAYED_RECTIFIER (123)**: Kv1.1/Kv2.1 delayed rectifier K⁺ channel. GHK K⁺ flux, m-gate, V_half=−30mV.
- **MOD_TYPE_COUNT**: 123→124 (+KvDR)
- **sizeof(BionIntraConnection)**: 12→16 bytes. bion_loader: automatic size detection by version.
- Tests: 55/55.

## v0.19.7 — MOD_NKCC1 (Cl⁻ switch) + BION v8 audit
**Date:** 8 July 2026
- **MOD_NKCC1 (122)**: Na-K-2Cl cotransporter — electroneutral Cl⁻ import. Antagonist of KCC2. Key to the ontogenetic Cl⁻ switch in Xenopus.
- **MOD_TYPE_COUNT**: 122→123 (+NKCC1)
- **BION v8**: bion_version_string() fixed (was hardcoded "7").
- **sizeof(Segment)**: comment 912→1692 bytes.
- Tests: 55/55.

## v0.19.6 — IPC state frames fix + daemon safe defaults
**Date:** 2 July 2026
- **IPC EAGAIN fix**: `sendBuffer()` — EAGAIN/EWOULDBLOCK handling via `poll(POLLOUT)` when writing large frames (2.6MB) to a non-blocking fd. Without the fix, frames were silently lost.
- **daemon safe defaults**: `gpu_enabled=false` (CPU-only by default), `tcp_port=0` (no TCP). Production sets these explicitly in the config.
- **GPU-to-CPU fallback**: on GPU failure `loadNetwork()` no longer fails, but switches to the CPU path.
- **sim_ipc_equiv**: 6/6 PASS (was 0/6), sim_ipc_bridge: All PASS (were All FAIL)

## v0.19.5 — CUDA two-pass diffusion + CPG tuning + Agent Skills
**Date:** 1 July 2026
- **CUDA two-pass diffusion fix**: `kernel_diffusion` split into `kernel_diffusion_compute` + `kernel_diffusion_apply` — eliminated the atomicAdd race condition on extra_ions. CPU-GPU parity: 221 assertions, delta=0.29mV
- **CPG config**: K2P=0.06, PUMP=0.40, NAK_HH=0.8, SK_CIN=1.8, GLY_TAU=80ms — calibrated for V_rest ~-67mV
- **Agent Skills**: DDD methodology + debugging triage (5 phases) + planning workflow in CLAUDE.md
- **--gpu-device flag**: the daemon accepts the GPU device index in batch mode
- **GPU device flag**: `BIOCORE_GPU_DEVICE` env var + `--gpu-device N` for GPU selection

## v0.19.4 — Logging unification + daemon fix
**Date:** 25 June 2026
- **Unified log format**: unixtime [LEVEL] msg across all subprojects (Core/Studio/Orch)
- **64 fprintf/printf → DLOG_INFO/WARN/ERROR**: 7 production files (daemon_main, daemon_server, daemon_config, bion_loader, bion_loader_host, plugin_manager, gpu_backend)
- **Daemon fix**: stopSim() before freeNetwork() — prevents a race on disconnect without stopping the simulation
- **Removed dead nernst()** from receptors_biological.cpp (GHK migration)
- **17 warnings silenced** (65→2): unused variables, fread/write returns
- **33/33 tests**, sim_ipc_bridge 203/203

## v0.19.3 — Educational experiments (trilingual READMEs)
**Date:** 23 June 2026
- 16 educational .bexp experiments: 4 undergraduate + 8 graduate + 4 postgraduate
- README in Russian, English and Chinese for each
- Topics: action potential, passive cable, synaptic integration, lateral inhibition, CPG, working memory, receptive field, STDP (LTP/LTD/full window), epilepsy, pharmacology, homeostatic plasticity, bifurcations

## v0.19.2 — Reorganization of experiments
- 4 built-in experiments (motion, direction, stdp_ltp/ltd) moved to graduate/
- stdp_ltp and stdp_ltd kept as separate + a combined stdp_learning
- Index README with a table of all 16 experiments

## v0.19.1 — 12 new educational experiments
- Generation of .bexp recipes for all learning levels
- README with biological explanations, verification and reproduction methods

## v0.19.0 — MSG_LOAD_BEXP + --load-bexp batch mode
**Date:** 23 June 2026
- `MSG_LOAD_BEXP` (0x0108): loading .bexp as an experiment recipe
- `--load-bexp` in batch mode: auto-extraction of bion_file_path from the JSON header
- Parsing of events (stimuli) from .bexp, detection configuration setup

## v0.18.0 — P0 fixes: task progress, graceful shutdown, auto-checkpoint, tiered shadow validation
- `MSG_GET_TASK_PROGRESS`: real current/total/percent via task_start_tick/task_total_ticks
- Graceful shutdown: SIGTERM in batch mode → bion_save of an emergency checkpoint → exit(75)
- Auto-checkpoint in the server simulationLoop (checkpoint_interval in SessionContext)
- Biological shadow validation: COLD segments are checked against the passive model (I_leak)
- On deviation > 0.1mV → automatic promotion to WARM/HOT

## v0.17.3 — Batch mode integration tests
- `test_batch_mode.cpp`: 7 tests, 14 assertions on the live test_single_neuron.bion
- Smoke tests (binary exists, help), validation (rejects missing args), integration (BEXP output), params JSON, checkpoint

## v0.17.2 — MSG_RUN_TASK_PARAMS returns metrics
- core_compute_metrics() in the MSG_RUN_TASK_PARAMS handler
- Metrics: mean_vm, spike_count, synchrony, V_m by neuron type

## v0.17.1 — Checkpoint interval + resume support in batch mode
- `--checkpoint-interval N`: auto-save BION every N ticks
- `--resume-from PATH`: restore from a checkpoint
- volunteer_name in the agent

## v0.17.0 — Distributed computing Stages 1-5
**Date:** 23 June 2026
- **Stage 1:** daemon batch mode (`--batch --load-bion --ticks --bexp-output --params-json --seed`), MSG_RUN_TASK_PARAMS (0x090A) with parameter override
- **Stage 2:** Tiered execution (HOT/WARM/COLD classification) in simulation_host.cpp behind the tiered_execution flag
- **Stage 3:** core_compute_metrics() — deterministic V_m/spike/synchrony metrics
- **Stage 5:** MSG_CHECKPOINT_AND_CONTINUE (0x0804), MSG_GET_TASK_PROGRESS (0x090C)
- **Tests: 32/32**

## v0.38.3 — Plugin tests + SDK examples

**Date:** 22 June 2026

- **test_plugin_compat:** 5 tests validating all .so plugins (dlopen → init, RTLD_NOW+RTLD_LAZY, stale-detection of removed symbols).
- **SDK examples:** `example_ca_channel.so` and `example_d2_receptor.so` rebuilt against the current API (`register_modifier_current` instead of the removed `register_user_modifier`).
- **dispatch_generated.cpp:** regenerated — user types 121-122 included in the GPU dispatcher.
- **Total tests: 20** (was 19).

## v0.38.2 — Per-session isolation + test coverage 28/28

**Date:** 22 June 2026

- **Per-session isolation (#1-#3):** global variables of consolidation, the delay buffer and active_library moved into SessionContext. Added `_ctx` variants of functions: `core_init_consolidation_ctx`, `core_run_tick_ctx` and others. `TickSessionPtrs` — a single implementation without globals.
- **Daemon fixes:** streamer flush for all sessions, removeClient clears fd from all streamers.
- **bion_save fix:** `BION_MAGIC` + filling all header fields from `BionData` (previously the magic was zero when saving a checkpoint).
- **Tests: 28/28** (was 21)
  - `test_daemon_protocol_full.cpp` — 32 tests, 27 MSG handlers (all previously uncovered)
  - `test_plugin_lifecycle.cpp` — 15 tests (.so lifecycle, names, .info, bounds)
  - `test_core_security.cpp` — 15 tests (broken BION, invalid headers)
  - `test_core_chaos.cpp` — 10 tests (nullptr, double init, rapid cycles)
  - `test_core_regression.cpp` — 17 tests (sizeof, constants, bion_validate)
  - `test_core_smoke.cpp` — 4 tests (daemon start, PING, VERSION, PLUGINS)
  - `test_core_performance.cpp` — 6 tests (run_tick, BION I/O, plugin timing)
- **Testing platform:** Catch2 v2 (single-header), 28 executables via ctest.

## v0.38.1 — Parser unification + daemon_server fix

**Date:** 19 June 2026

- **daemon_config.cpp:** switched to the shared `json_parser.h` from BioNeuronOrchestrator
- **daemon_server.cpp:** ~40 compilation errors fixed
  - Added `default_session_` (session 0)
  - Field names: `segments()` → `_segments()` (and similar)
  - `SessionContext::freeNetwork()` — inline definition of memory release
  - `MAX_STIM` → `SessionContext::MAX_STIM`
  - `sim_thread_` → `current_ctx_->sim_thread`
  - `simulationLoop()` — non-static method (uses `current_ctx_`)
  - Per-session wrappers: `loadNetworkForSession()`, `startSimForSession()` and others
  - `isNetworkLoaded()`, `segs()`, `syns()` and others — inline definitions
- **daemon_config.cpp:** field validation: tcp_port, max_sessions, log_max_size_mb, log_max_files
- Include-path: `-I../BioNeuronOrchestrator` for daemon_config in CMakeLists.txt

## v0.38.0 — Splitting Core/Studio into two processes (IPC)

**Date:** 18 June 2026

- **Core Daemon:** the `biocore-daemon` daemon — a standalone process listening on a Unix socket + TCP (port 6789).
- **IPC protocol:** a binary wire protocol (30+ message types: sim, stimuli, state, plugins, consolidation, checkpoint).
- **State streaming:** STATE_FRAME (raw Segment/Synapse arrays) for efficient transfer of V_m/ions/NT to the GUI.
- **Config:** `/etc/biocore/biocore.conf` (JSON), no personal data.
- **Systemd unit:** `biocore-daemon.service`, user `biocore`, sandbox security.
- **Logging system:** `daemon_log.h` — size-based rotation (as in Studio), `[timestamp] [LEVEL]`.
- **Plugins:** 111/111 plugins load (`-rdynamic` + `RTLD_LAZY`).
- **GPU backend:** dlopen `libbiocore_gpu_backend.so` in the daemon, GPU path in simulationLoop.
- **Arguments:** `--config`, `--plugins-path`, `--ready-fd N`.
- **Packaging:** the `mindcopy-daemon` .deb package (preinst/postinst/prerm/postrm).
- **Tests:** `test_daemon_protocol` (9 protocol tests).

## v0.16.7 — BION loader fix and test updates

**Date:** 16 June 2026

- **bion_loader_host.cpp:** added error code BION_ERR_FILE_READ (-11), all 8 fread calls wrapped with return checks.
- **bion_format.h:** `bion_version_string()` fixed — returns "6" (not "5").
- **Documentation:** CLAUDE.md/README.md — BION v4/v5→v6, MAX=4→6.
- **Tests:** updated constants (sizeof(Nucleus)=332, BION_VERSION=6, MOD_TYPE_COUNT=121). Core: 18/18 tests.

## v0.16.6 — Electroreception + Magnetoreception (TODO 3: parts 3+4)

**Date:** 15 June 2026

- **MOD_ELECTRORECEPTOR_AMPULLARY (118)** — ampullae of Lorenzini: gel channel (τ=100ms, low-pass), Ca²⁺ adaptation (τ=5s), directional tuning, GHK (Na⁺50%/K⁺35%/Ca²⁺15%)
- **MOD_MAGNETITE_MECHANORECEPTOR (119)** — Fe₃O₄ chains: torque τ=m×B → MET channels, sin²(θ) dependence, Ca²⁺ adaptation (τ=8s), GHK (Na⁺45%/K⁺30%/Ca²⁺25%)
- **MOD_CRYPTOCHROME_MAGNETIC (120)** — Cry4 radical pairs [FAD•⁻+Trp•⁺]: light-dependent (λ=400-500nm), sin²(θ) angular dependence, MODULATOR (modulates MOD_LIGHT_SENSOR)
- MOD_TYPE_COUNT=121

## v0.16.5 — Vestibular apparatus (TODO 3: part 2)

**Date:** 15 June 2026

- **MOD_OTOLITH_HAIR_CELL (116)** — otolith hair cell: MET channels, cosine tuning, Ca²⁺ adaptation of myosin motors (τ=30ms), tonic P_open~0.15 (gravity), GHK (Na⁺60%/K⁺30%/Ca²⁺10%)
- **MOD_SEMICIRCULAR_CUPULA (117)** — semicircular canal cupula: torsional pendulum (ω₀², 2ζω₀), push-pull bilaterality, phasic Ca²⁺ adaptation (τ=10ms), GHK (Na⁺55%/K⁺35%/Ca²⁺10%)
- MOD_TYPE_COUNT=118

## v0.16.4 — Audiotaxis (TODO 3: part 1)

**Date:** 15 June 2026

- **MOD_VIBRATION_SENSOR (115)** — vibration/acoustic sensor: mechanotransduction, sigmoidal activation, habituation+sensitization, GHK
- MOD_TYPE_COUNT=116

## v0.16.3 — Sensory adaptation (H2)

**Date:** 14 June 2026

- **Two-process adaptation model** for all 4 sensory modifiers:
  - Counter-directed (habituation): GRK/β-arrestin/Ca²⁺-CaM inactivation
  - Co-directed (sensitization): cAMP/PKA/Ca²⁺-recoverin priming
- **MOD_PHYSICAL_SENSOR (6)**: +habituation (state[2]), +sensitization (state[3]), configurable k_hab/k_sens/tau_hab
- **MOD_LIGHT_SENSOR (95)**: +sensitization (state[3], tau=60s, k_sens=0.5 — Ca²⁺/recoverin/GCAP)
- **MOD_OLFACTORY_RECEPTOR (108)**: +sensitization (state[3], tau=10s, k_sens=0.3 — cAMP/PKA)
- **MOD_HUNGER_SENSOR (94)**: +habituation (state[1], tau=300s — AMPK/mTOR), +sensitization (state[2], tau=600s)
- **Shared helpers** in `plugin_helpers.h`: `sensor_habituation_step()`, `sensor_sensitization_step()`, `sensor_adaptation_gain()`
- CPU/GPU delta = 0.00 mV for the physical and light sensors
- GPU fallback (`hh_biological.cpp`) synchronized with CPU plugins

## v0.16.2 — MOD_CONNEXIN_CX36 + gap junction delay (G2)

**Date:** 12 June 2026

- **MOD_CONNEXIN_CX36 (114)**: molecular biology of gap junctions
  - Synthesis of Cx36 in the ER → connexon assembly (6×Cx36) → membrane
  - Phosphorylation: CaMKII→↓P_open, PKA(cAMP)→↑P_open
  - pH dependence: closing at pH<6.8
  - Integrated into the gap junction: g_eff *= Cx36_pre * Cx36_post
- **MOD_TYPE_COUNT** 114→115
- 18/18 tests

## v0.16.1 — F2+F3+G1: Density events + Ca²⁺→CREB + gap junction delay

**Date:** 11 June 2026

- **F2 — Gene→modifier density events**:
  - CONSOL_EVENT_MODIFIER_DENSITY (4) in types.h
  - Gene→modifier mapping (SCN1A→NaK, KCND2→Kv4.2, KCNQ2/3→Kv7, etc.)
  - applyPendingEvents: density events → segment.modifiers[].density
- **F3 — Ca²⁺→CREB→TF intrinsic homeostasis**:
  - cacheSignalsFromSoma: Ca²⁺_soma → pCREB (Hill n=2, Kd=0.5μM)
  - pCREB → TF expression → ion channel genes → protein → density
- **G1 — Gap junction delay buffer**:
  - First-order RC filter (tau=delay_ms) on I_gap
  - CPU (simulation_host.cpp) + GPU (kernels_part2.sycl)
- 18/18 tests

## v0.16.0 — BION v6 + 36 genes + GRN (F1)

**Date:** 11 June 2026

- **GENE_COUNT** 20→36: 6 TFs + 10 ion channels
- **Nucleus** 204→332 bytes
- **BION_VERSION** 5→6, v5→v6 migration
- New genes: TF_LHX3, TF_ISLET1, TF_PAX2, TF_BRN3A, TF_MNX1, TF_LMX1B,
  SCN1A(Nav1.1), KCND2(Kv4.2), KCNQ2, KCNQ3, CACNA1G(Cav3.1), HCN1, HCN2,
  KCNJ2(Kir2.1), KCNK2(K2P), KCNMA1(BK)
- TF→ion channel gene regulation in ConsolidationThread
- 18/18 tests

## v0.15.0 — Biological STDP + BCM θ_m + PP1/DARPP-32 (C1+C3)

**Date:** 11 June 2026

- **Surrogate timing traces REMOVED**: syn->facilitation/depression are no longer manual timers
- **BCM θ_m**: sliding average of Ca²⁺² (τ≈5min) in syn->depression
- CaMKII (Kd=1μM) vs Calcineurin (Kd=0.05μM) naturally separate LTP/LTD
- **MOD_PP1_INHIBITOR_DARPP32 (113)**: D1→cAMP→PKA→p-DARPP32→inhibit PP1→LTD↓
- PP1_active integrated into AMPAR trafficking
- Pre-spike facilitation=1.0 removed from CPU and GPU
- **MOD_TYPE_COUNT** 112→114
- 18/18 tests

## v0.14.5 — MOD_KV7_MCURRENT (KCNQ2/3 M-current)

**Date:** 11 June 2026

- **MOD_KV7_MCURRENT (112)**: slow K⁺ channel (tau~100ms), PIP2 dependence
- GHK K⁺ flux, V_half=-38mV, no inactivation
- CPU+GPU, .so plugin, 6 biophysical tests
- CPU=GPU: delta=0.00mV
- **MOD_TYPE_COUNT** 112→113
- 18/18 tests

## v0.14.4 — K+ conservation + GPU-CPU parity + MOD_THERMAL_NOISE

**Date:** 11 June 2026

- **K+ ionic conservation** (hh_biological.cpp):
  - extra_ions[K] added to 5 channels: K2P_LEAK, KIR_CHANNEL, SK_CHANNEL, BK_CHANNEL, PIR_HCN
  - Signs fixed in Kv4.2 and Generic V-gated (intra/extra were inverted)
- **GPU-CPU parity** (kernels_part1.sycl, simulation.cpp):
  - Thermal noise ±0.05mV added to GPU update_membrane (identical to CPU)
  - Tick passed to the GPU kernel for reproducible noise
  - NT race condition fix: `=` → atomic fetch_add (GPU) / `+=` (CPU)
  - extra_only guard in GPU diffusion (skip intra for glial connections)
  - V_m guard CPU/GPU alignment [-200, +100]
  - STDP/plasticity (CaMKII/Calcineurin/eCB) in the GPU path (step 3.5)
- **MOD_THERMAL_NOISE (111)**: stochastic noise on the xorshift32 PRNG
  - .so plugin + GPU fallback + dispatch
  - state[0]=σ(mV), state[1]=seed_offset
- **MOD_TYPE_COUNT** 111 → 112
- 18/18 tests

## v0.14.3 — Full biological_library + GPU-safe GHK

**Date:** 10 June 2026

- `receptors_biological.cpp`: types 98-107 (PMCA, NCX, KCC2, Ca_leak, Ca_buffer,
  SK, BK, NaP, KIR, K2P) added to `init_biological_library()`. Fixes the
  plugin-free CPU path (previously used NULL pointers).
- `hh_biological.cpp`: `safe_ghk_denom()` with epsilon 1e-7f in `k2p_leak_current`
  (protection against fp32 underflow on the GPU).
- 18/18 tests PASS.

## v0.14.2 — GPU NaN fix + biological parameters

**Date:** 10 June 2026

- **Critical GPU fixes:**
  - `kernels/kernels_part1.sycl`: skip axial current when `coupling_resistance==0`
    (glial `connect_extra` caused division by 0 → NaN)
  - `kernels/kernels_part1.sycl`: NaN guard in `update_membrane` (`V_m!=V_m` → -65mV)
  - `kernels/kernels_part2.sycl`: release formula `(1+facil)/(1+depress)`
    instead of `facil/depress` (NaN at 0/0 at rest)
  - `plugin_helpers.h`, `receptors_biological.cpp`: `safe_ghk_denom()` epsilon 1e-7f
  - `hh_biological.cpp`: NaK_HH — conductance now with `*0.01f` (as the CPU plugin),
    fixed the 100x ion concentration drift
  - `simulation.cpp`: NaN protection of concentrations after diffusion
- **Biological parameters (CPU plugins → GPU values):**
  - NaP: g_max 1.0→0.08 (BIOREF 0.01-0.1 mS/cm²)
  - SK: g_max 1.5→0.5 (BIOREF 0.5-5 mS/cm²)
  - BK: g_max 2.5→2.0 (BIOREF 1-20 mS/cm²)
  - KCC2: v_max 0.002→0.0008 (BIOREF default)
  - KIR: g_max 1.5→1.0 (BIOREF 0.1-1 mS/cm²)
- Tests: `test_hh.cpp`, `test_synapses.cpp` — thresholds updated
  for correct biological scaling

## v0.14.1 — Expansion of test coverage

**Date:** 10 June 2026

- +5 test files: consolidation_plasticity, delay_buffer, gpu_backend, plugin_manager, v3v4_migration
- Total: 18/18 tests, 100% PASS
- Valgrind: no leaks

## v0.14.0 — Coordinated Switch to biophysical units

**Date:** June 2026

### Biophysical migration (coordinated switch)
- **C_SPECIFIC_PF_PER_UM2 = 0.01** (1 µF/cm² = 0.01 pF/µm²) — biophysical specific capacitance
- **All currents scaled to biophysical units:**
  - Conductance-based: `g × density × area × dV × 0.01`
  - GHK-based: `P × density × area × 1e-11f` (instead of 1e-9f)
  - Pumps/NCX: biological ion transport + scaled return (×0.01)
- **Global compensation removed:** `I_buffer *= C_SPECIFIC_PF_PER_UM2` removed from the GPU and CPU paths
- **38+ files converted:** 34 .so plugins + 2 fallback files + plugin_helpers.h + 2 simulation kernels
- **C_m is now biophysical:** `C_m = area × 0.01` (pF) with clamp 0.1 pF

### Test fixes
- `test_consolidation`: MOD_TYPE_COUNT 109→111
- `test_bion_loader_host`: coupling_resistance recomputed from geometry (1e6×L/πr²)
- `test_synapses`: C_m formula updated to C_SPECIFIC

### General
- `MOD_TYPE_COUNT`: 111 (added MOD_GLIAL_KIR41=109, MOD_GLIAL_NAK_PUMP=110)
- Core: **13/13 PASS** (0 failures)
- Studio: **162/162 PASS** (0 failures)

## v0.13.0 — Ca²⁺/K⁺/Cl⁻ homeostasis + Rhythmogenesis channels + STDP

**Date:** June 2026

### Ca²⁺/K⁺/Cl⁻ homeostasis (5 new modifiers: 98-102)
- **MOD_PMCA** (98) — plasmalemmal Ca²⁺-ATPase: ATP-dependent extrusion of 1Ca²⁺, Hill n=2, K_m=300nM, electrogenic
- **MOD_NCX** (99) — Na⁺/Ca²⁺ exchanger: 3Na⁺ in : 1Ca²⁺ out, V-dependent, allosteric Ca²⁺ activation
- **MOD_KCC2** (100) — K⁺-Cl⁻ cotransporter: electroneutral 1K⁺:1Cl⁻ out, critical for glycine/GABA-A inhibition
- **MOD_CA_LEAK** (101) — passive Ca²⁺ leak: GHK Ca²⁺ (z=2), no gating, maintains basal [Ca²⁺]i
- **MOD_CA_BUFFER** (102) — fast Ca²⁺ buffer: parvalbumin-like, on/off kinetics, B_total=150μM, K_d=500nM

### Rhythmogenesis ion channels (4 new modifiers: 103-106)
- **MOD_SK_CHANNEL** (103) — SK (KCa2.x): purely Ca²⁺-activated K⁺, AHP, Hill n=4, K_d=400nM, GHK K⁺
- **MOD_BK_CHANNEL** (104) — BK (KCa1.1): Ca²⁺+V-activated K⁺, fast repolarization, Ca²⁺-shift of V_half, GHK K⁺
- **MOD_NAP_CHANNEL** (105) — NaP (Nav1.6): persistent Na⁺, no inactivation, subthreshold activation (-52mV), GHK Na⁺
- **MOD_KIR_CHANNEL** (106) — Kir (Kir2.x): inward rectifier K⁺, Boltzmann+rectification, RMP stabilization, GHK K⁺

### STDP refactoring
- Continuous CaMKII/Calcineurin mechanism + timing-trace modulation (facilitation/depression)
- Pre-before-post → boost LTP rate. Post-before-pre → boost LTD rate
- Reset of timing traces after consumption (prevents false triggers)
- Inhibitory synapses (GABA-A, GABA-B, Glycine): inverted dw sign

### Na/K pump tuning
- Default p_rate: 0.1 → 0.15 (plugin + GPU fallback)
- Biologically justified: different α-subunit isoforms in CPG neurons

### Architectural principle
**All 9 new modifiers work via GHK + direct ion movement.**
Not a single `I = g*(V-E_rev)`. Each modifier explicitly updates `s->intra_ions[]`.

### General
- `MOD_TYPE_COUNT`: 98 → 107
- All plugins: `.cpp` + `.info` (full neurobiological descriptions)
- GPU fallback: all 9 types in `hh_biological.cpp` + switch cases in `kernel_dispatch.h`
- CPU: individual `.so` plugins (auto-build via CMake glob)

### Tests
- Core: 13/13 — 100%
- Studio: 158/159 (sim_regression — pre-existing)

## v0.12.0 — Gap Junction Gating + D1/Gi/o Fix

**Date:** June 2026

### Electrical synapses (gap junctions)
- **Fast Vj gate**: Boltzmann curve (V_half=40mV, k=5mV, tau=5ms), residual conductance 10%
- **Slow Ca²⁺/CaM gate**: Hill activation of calmodulin (n=2, EC50=300nM, tau=100ms), full channel closure
- Gate states stored in `Synapse::facilitation` (m_fast) and `Synapse::depression` (m_slow) — the `.bion` format is untouched
- CPU (`simulation_host.cpp`) + GPU (`kernels_part2.sycl`)

### Current sign fix
- **D1 (Gs) and Gi/o**: `return -(-I_K)` → `return -I_K` in `gs_ghk_effect` and `gi_o_ghk_effect` (`plugin_helpers.h`, `receptors_biological.cpp`)
- K⁺ efflux is now correctly treated as a hyperpolarizing current

### Incidental fixes
- `mod_09_nmda.cpp`: fixed the lost return of `ionotropic_ghk_current()`
- `mod_63_nb_nmda_moradi_full.cpp`: removed stray GHK code from `temp_mod()`
- `mod_67_nb_alpha7_er_nachr.cpp`, `mod_75_nb_msn_nmdar.cpp`: removed incomplete GHK blocks

### Tests
- Core: 13/13 (5 new tests: 3× gap junction gating + 2× Gi/o hyperpolarization)
- All `test_synapses` tests pass

## v0.11.1 — GPU/CPU synchronization fixes

**Date:** June 2026

### Simulation kernel fixes
- **GPU compute_modifier_currents**: `I_buffer = I_total` → `I_buffer += I_total` — stimuli are no longer overwritten every tick
- **GPU update_membrane**: added `C_m < 1.0f` clamp (as in the CPU path)
- **core_run_tick**: `duration_ms` correctly converted to ticks via `dt` (was a direct ms-to-ticks comparison)
- **CurrentStimulus**: fields in the packed structure correctly read by the GPU path

### Tests
- 159 automated tests (Core + Studio), 0 failures
- New regression test `sim_regression_bugs` (28 checks)
- `sim_gpu_bridge` tests the GPU path via SimulationBridge
- `sim_gpu_stim` checks the packed structures of the GPU path

## v0.11.0 — GHK migration + PIR + sensors

**Date:** June 2026

### PIR: post-inhibitory rebound (3 new modifiers)
- `MOD_POST_INHIBITORY_REBOUND` (93) — umbrella delegate to 96+97
- `MOD_PIR_TTYPE_CALCIUM` (96) — T-type Ca²⁺ (Cav3.x): GHK Ca²⁺ flux (z=2), m³h gating, deinactivation window (-70..-30mV), 5-HT/DA suppression
- `MOD_PIR_HCN_CHANNEL` (97) — HCN1/2/4 (Ih): separate GHK Na⁺(40%)/K⁺(60%) fluxes, sag effect, DA suppression, E_rev arises from the permeability ratio

### Sensors (2 modifiers)
- `MOD_HUNGER_SENSOR` (94) — interoceptive hunger sensor: ATP monitor → nonselective cation channel (Na⁺ 60%/K⁺ 30%/Ca²⁺ 10%) via GHK
- `MOD_LIGHT_SENSOR` (95) — photoreceptor (CNG channel): Na⁺ 50%/K⁺ 30%/Ca²⁺ 20% via GHK, adaptation (tau=2s)

### GHK migration (59 plugins)
- **Complete abandonment of I=g*(V-E)**: all electrical currents via the Goldman-Hodgkin-Katz equation
- **3 template functions rewritten** in `plugin_helpers.h`: `ionotropic_ghk_current()`, `gi_o_ghk_effect()`, `gs_ghk_effect()`
- 9 ionotropic receptors (AMPA..TRPV1) → GHK cation/anion fluxes
- 24 Gi/o receptors → GHK K⁺ via GIRK
- 11 Gs receptors → GHK K⁺ (channel closure)
- 12 NB_ receptors → GHK with Mg²⁺ block preserved
- physical_sensor (06) → GHK nonselective cation
- HH NaK (00) → E_leak = nernst(K) instead of hardcode
- **0 hardcoded E_rev in all plugins**

### General
- `MOD_TYPE_COUNT` = 98 (was 93)
- `ghk_monovalent()`, `ghk_divalent()`, `ghk_anion()` — shared helpers in `plugin_helpers.h`
- All plugins directly move ions (`s->intra_ions[X] += ...`), V_m is updated from charge balance

### Tests
- `test_consolidation.cpp`: MOD_TYPE_COUNT=98
- `test_simulation_host.cpp`: myelin test fix (noise + 100-tick accumulation)
- 13/13 tests passed

---

## v0.10.0 — Consolidation/reconsolidation (two-stage memory)

**Date:** June 2026

### Gene system (Nucleus)
- The `Nucleus` structure (204 bytes): 20 plasticity genes, signaling caches, settings
- 20 genes: CREB, c-Fos, Zif268, Arc, BDNF, Cofilin, MMP-9, TIMP, GluR1, GluR2, PSD-95, Neurexin, Neuroligin, Dynamin, Clathrin, IntegrinB3, TrkB, mTOR, S6K1, 4E-BP
- `gene_activity[20]` — mRNA levels (0..1), `protein_level[20]` — protein levels (0..1)

### 6 new modifiers (87-92)
- `MOD_MAPK_ERK` (87): Ca²⁺→Ras→Raf→MEK→ERK→pCREB, cross-talk with CREB
- `MOD_IKK_NFKB` (88): Ca²⁺→IKK→IkB→NF-κB→Zif268 (reconsolidation)
- `MOD_MTORC1` (89): two-key (AA + PI3K/AKT), ATP inhibition
- `MOD_PROTEIN_SYNTH` (90): mTORC1→S6K1/4E-BP→translation, ATP consumption
- `MOD_COFILIN` (91): ERK→LIMK→pCofilin/F-actin (actin remodeling)
- `MOD_MMP9` (92): ERK+NF-κB→MMP-9/TIMP/integrins (ECM degradation)

### ConsolidationThread
- Background thread with a configurable interval (0=OFF), per-neuron override
- Gene→protein→structure: 3 phases per cycle
- Consolidation: pCREB→c-Fos/BDNF/Arc, ERK→Cofilin/MMP9
- Reconsolidation: NF-κB→Zif268 (a path distinct from CREB)
- Structural plasticity: synaptogenesis, LTD removal (Calcineurin>0.3+weight<0.05), Δdiameter (Cofilin/F-actin), ΔAMPAR (GluR1/GluR2)
- Reactivation buffer (1000 ticks of V_m) for memory detection
- Step 2.5b: soma→Nucleus link (cache of pCREB/ERK/NF-kB/mTORC1/AA)
- Step 5: applying ConsolidationEvent from the queue

### BION v5
- NetworkHeader: 68→84 bytes (+num_nuclei, +nucleus_version, +reserved)
- Nucleus[] in .bion: load/save of 204 bytes/neuron
- v4→v5 migration: automatic creation of default nuclei when loading old files
- `bion_calc_size()` updated to account for nuclei

### C API
- `core_init_consolidation`, `core_start_consolidation`, `core_stop_consolidation`
- `core_set/get_consolidation_interval`, `core_get_pending_consolidation_events`
- `core_get_nucleus_snapshot`

### Testing
- `test_consolidation`: 15 cases, 44 assertions (structures, modifiers 87-92, integration)
- MOD_TYPE_COUNT: 87→93

## v0.9.0 — Diffusion extension: intra_nt, extra_ions, neurotransmitter

### Generic voltage-gated channel (MOD_GENERIC_VOLTAGE_GATED = 80)
- Parametric Hodgkin-Huxley kinetics without hardcode
- state[]: V½_fwd, k_fwd, rate_fwd, V½_rev from NeuroML gateHHrates
- Separate forward/reverse parameters: alpha(V) from forward, beta(V) from reverse
- Three kinetics forms: HHExpLinearRate, HHExpRate, HHSigmoidRate
- Inactivation (h-gate) with its own V½_h

### Generic ligand-gated receptor (MOD_GENERIC_LIGAND_GATED = 81)
- Hill kinetics (n=2) with configurable EC₅₀, tau_rise, tau_decay, g_max
- E_rev: Nernst (state[5]=0) or fixed from NeuroML

### Molecular STDP cascade (types 82-86)
- MOD_CAMKII (82): Ca²⁺/calmodulin-dependent kinase II, autophosphorylation
- MOD_CALCINEURIN (83): Ca²⁺/calmodulin-dependent phosphatase PP2B
- MOD_CA_L_TYPE (84): L-type Ca²⁺ channel (Cav1.x, HVA, CDI)
- MOD_KV42 (85): Kv4.2 A-type K⁺ channel with CaMKII phosphorylation
- MOD_ECB_SYNTHESIS (86): endocannabinoid synthesis (2-AG) from Ca²⁺
- AMPAR trafficking in simulation_host.cpp step 3: CaMKII/Calcineurin → Δsyn.weight
- eCB retrograde signaling: 2-AG → cleft → presynaptic CB1

### Cm fix
- The CPU path now divides I_buffer by C_m (membrane capacitance) like the GPU path
- V_m += I_buffer / (2π·r·l) · dt — physiological dynamics

### Fixes
- MOD_TYPE_COUNT: 80 → 87
- INFO_CACHE_SIZE: 80 → 87
- Parametric alpha/beta with separate forward/reverse parameters
- NMDAR Mg²⁺ block: coincidence detection (glutamate + depolarization)

## v0.9.0 — Diffusion extension: intra_nt, extra_ions, neurotransmitter
- intra_nt[NT_COUNT], extra_ions/neurotransmitter diffusion, Stokes-Einstein
- Segment: 932 → 988 bytes

## v0.8.0 — Stimulation in Core
- I_clamp, VoltageClamp, RampStimulus, IonInjection, LigandInjection

## v0.7.0 — CPU/GPU switch
- gpu_backend.cpp, libbiocore_gpu_backend.so, dlopen

## v0.6.2 — state[7] scratchpad
- pre_area/post_area for synaptic contact areas

## v0.6.0 — BioNeuronSDK
- plugin-devkit.h, biocore-gen-dispatch, cmake module

## v0.5.0 — 63 .so plugins
- extern "C" registration, modifier_current_table[128]

## v0.4.0 — BION v4
- Host loader, plugin infrastructure (dlopen)

## v0.3.0 — Binary .bion v3
- Full validation, v3→v4 migration

## v0.2.0 — Synapses
- 63 receptors, 12 presynaptic modulators

## v0.1.0 — HH channels
- Na/K, Ca, Cl, axial currents, SYCL kernels

## v0.38.0 (2026-06-19) — Multi-Client + Multi-GPU Daemon

### New
- **Multi-Client isolation:** SessionContext for independent simulation of several Studios via one daemon
- **Multi-GPU:** enumeration of SYCL devices, creation of simulations on a specific GPU
- **MSG_FLAG_SESSION:** backward-compatible protocol with sessions (session_id)
- **MSG_CREATE/DESTROY_SESSION:** session management via IPC
- **MSG_LIST_DEVICES:** obtaining the list of GPU devices
- **MSG_RUN_TASK:** running background tasks via IPC (without Studio)
- **GpuDeviceInfo:** structure with device information in types.h
- **DaemonConfig:** max_sessions, gpu_device_allowlist
- **Validation:** checking gpu_device_index, session_id, bion_path, ticks

### Changes
- daemon_server: refactoring (session registry, compatibility aliases)
- gpu_backend.cpp: device_count, get_device_info, create_on_device
- daemon_session.h/cpp: new file — SessionContext
- Error types: ERR_MAX_SESSIONS, ERR_INVALID_SESSION, ERR_GPU_DEVICE_UNAVAILABLE


## Rights and intellectual property

© 2024–2026 **Feshin Vitaly Alexandrovich** / Vitaly Alexandrovich Feshin. All rights reserved.
E-mail: feshin2004@mail.ru · vfeshin@mindcopy.ru · Tel.: +7(918)345-75-18. Krasnodar, Russia / Краснодар, Россия.

State registration certificate of a computer program (Rospatent) **No. 727894_683451**.
Commercial use, distribution and sublicensing without the author's written permission are prohibited.
For the full text, see `LICENSE` and `DISCLAIMER.md` in the platform root.
