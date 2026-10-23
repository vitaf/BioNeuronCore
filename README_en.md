# BioNeuronCore — neural network computational engine

## Purpose

BioNeuronCore is an engine for biologically realistic simulation of neural networks in C++17 with SYCL 2020 (Intel oneAPI DPC++). Computation runs on the GPU (Intel, NVIDIA, AMD), with the possibility of future translation to FPGA (Xilinx Vitis HLS). The code is built around the principle of a **segmented neuron model**: each neuron consists of independent electrical compartments (segments) connected by intracellular links.

**File format:** BION v12 (MAX_MODIFIERS=32, backward compatible with v3-v11). **Version:** v0.20.0. **Tests:** 156. **Modifiers:** 198.

## Key capabilities

| Mode | Constant | Description |
|-------|-----------|----------|
| **BIOLOGICAL** | `MODE_BIOLOGICAL` (0) | All 198 modifier types use biologically calibrated models (Hodgkin-Huxley, neurotransmitter receptors, pumps, signaling cascades, Ca²⁺/K⁺/Cl⁻ homeostasis, rhythmogenesis channels, gene expression of 46 genes, Cx36, TRP/nociceptors, transporters, AQP4). |
| **ARBITRARY** | `MODE_ARBITRARY` (1) | The user can override any modifier with their own function. User modifiers (ID 0–127, the entire range is plugin-based) take priority over biological ones. |

### CPU/GPU backends

| Backend | Build | Description |
|--------|--------|----------|
| **CPU** | `libbiocore_gcc.a` (GCC) | Always available. Used by Studio by default. |
| **GPU** | `libbiocore_gpu_backend.so` (DPC++) | Loaded via dlopen. Requires Intel oneAPI runtime. Selected in the Studio toolbar. USM pointers are accessible from the CPU. |

### Modifier plugins

User modifiers (ID 0–127) are loaded from `.so` files via `dlopen`. Each plugin exports `biocore_plugin_init()` and `biocore_plugin_desc()` (JSON descriptor). API: `plugin_load()`, `plugin_load_directory()`, `register_user_modifier()`. Example: `plugins/example_ca_plugin.cpp`.

### Host loader (without SYCL)

`bion_loader_host.cpp` — loading/saving .bion into host memory (new[]/delete[]). Does not require Intel oneAPI. Used in BioNeuronStudio for loading models.

### Architecture: segments, neurons, network

```
.bion file → NetworkHeader → Neuron[] → Segment[] → IntraConnection[] → Synapse[]
```

**Segment** — the basic electrical compartment. It has:
- Geometry: position (x, y, z), length, radius → membrane area → capacitance
- Membrane potential `V_m` (mV)
- 7 intracellular and 7 extracellular ion concentrations (Na⁺, K⁺, Ca²⁺, Cl⁻, Mg²⁺, A⁻, H⁺)
- 15 types of neurotransmitters in the extracellular environment
- Up to 32 modifiers (ion channels, receptors, pumps, myelin, sensors)
- ATP level and temperature
- Transcription signal `transcription_signal` (CREB-dependent plasticity)
- Working current buffer `I_buffer` (zeroed on every tick)

**Neuron** — a logical grouping of segments. Contains:
- Segment range `[segment_start, segment_start + segment_count)`
- Soma index (`soma_index`)
- Spike threshold and type

**Intracellular link (IntraConnection)** — a resistive bridge between two segments:
- Axial current: `I = (V_a − V_b) / R_coupling`
- Segments accumulate up to 16 incoming links (fast access via `intra_connection_ids[]`)

**Synapse** — a chemical/electrical link between neurons:
- Presynaptic autoreceptors (up to 8) — modulate release probability
- Postsynaptic receptors (up to 8) — produce ionic currents upon neurotransmitter binding
- Probabilistic release (xorshift32 RNG, seed = id ⊕ tick ⊕ V_m)
- Neurotransmitter clearance from the cleft (exponential decay)
- Refractory period
- Facilitation and depression

### Modifiers: 198 types

#### Basic (8 types)
| Type | Function |
|-----|---------|
| `MOD_RESERVED_0` (0) | Former `MOD_IONOTROPIC_NAK` (HH squid) — removed (Phase 4, replaced by Nav1.x isoforms) |
| `MOD_CAV13` (1) | Cav1.3 L-type Ca²⁺ channel (pacemaker) |
| `MOD_IONOTROPIC_CL` (2) | Cl⁻ channel |
| `MOD_METABOTROPIC` (3) | General-purpose metabotropic receptor |
| `MOD_MYELIN` (4) | Myelin sheath: blocks ionotropic currents, reduces capacitance 100× |
| `MOD_SYNAPTIC_RELEASE` (5) | Neurotransmitter release mechanism |
| `MOD_PHYSICAL_SENSOR` (6) | Physical sensor (temperature, pressure) |
| `MOD_PUMP_NAK` (7) | Na⁺/K⁺-ATPase: 3 Na⁺ out, 2 K⁺ in per cycle, ATP-dependent |

#### Ionotropic receptors (9 types)
`AMPA`, `NMDA`, `KAINATE`, `GABA-A`, `GLYCINE`, `NICOTINIC`, `5-HT3`, `P2X`, `TRPV1`

They open ion channels directly upon neurotransmitter binding. Myelin blocks ionotropic receptors on myelinated segments (nodes of Ranvier — segments without myelin — remain active).

#### Metabotropic receptors (46 types, 3 families)

**Gi/o (24 types, inhibitory):** `GABA-B`, `5-HT1A/1B/1D/1F`, `D2/D3/D4`, `M2/M4`, `A1/A3`, `mGluR2/3/4/6/7/8`, `α2`, `CB1/CB2`, `H3`, `MT1/MT2`

**Gs (11 types, excitatory via cAMP):** `5-HT4/6/7`, `D1/D5`, `A2A/A2B`, `β1/β2`, `H2`, `TAAR1`

**Gq/11 (11 types, excitatory via IP3/DAG):** `5-HT2A/2B/2C`, `α1A/α1B`, `M1/M3/M5`, `mGluR1/5`, `H1`

### Presynaptic modulators (12 types)

Autoreceptors on the presynaptic membrane that modulate neurotransmitter release probability:
`5-HT1A`, `5-HT1B`, `D2`, `A1`, `A2A`, `D1`, `mGluR2`, `mGluR3`, `CB1`, `H3`, `NICOTINIC`, `α2`

### Neurotransmitters (15 types)
Glutamate, GABA, dopamine, serotonin, acetylcholine, adenosine, noradrenaline, glycine, histamine, endocannabinoids, ATP, melatonin, Mg²⁺, nicotine, capsaicin

### Ions (7 types)
Na⁺, K⁺, Ca²⁺, Cl⁻, Mg²⁺, A⁻ (macro-anion), H⁺ (proton) — intracellular and extracellular concentrations are tracked

## Simulation loop (5 steps per tick)

```
Step 1:   Modifier currents + stimulus processing (I_clamp, ramp, ionic/ligand injections) → segment I_buffer
Step 2:   Axial currents (IntraConnection) → atomic accumulation into I_buffer
Step 2.5: Intersegmental diffusion (ions, ATP, temperature, transcription) per Fick's law
Step 3:   Synaptic currents (release → modulation → post-response → clearance)
Step 4:   Update V_m = V_m + I_buffer / C_m × dt, reset I_buffer, voltage clamp (V_m overwrite)
```

Each step is synchronized (`q.wait()`), which guarantees a deterministic computation order.

## Consolidation/reconsolidation (two-stage memory) — v0.10.0

### Gene system (Nucleus, 412 bytes)

Each neuron has a nucleus (`Nucleus`) with 46 plasticity genes. Genes are transcribed into mRNA (`gene_activity[]`), then translated into proteins (`protein_level[]`). The process is managed by the background thread `ConsolidationThread`.

**46 genes:** `CREB`, `c-Fos`, `Zif268`, `Arc`, `BDNF`, `Cofilin`, `MMP-9`, `TIMP`, `GluR1`, `GluR2`, `PSD-95`, `Neurexin`, `Neuroligin`, `Dynamin`, `Clathrin`, `IntegrinB3`, `TrkB`, `mTOR`, `S6K1`, `4E-BP`

### Signaling cascades (modifiers 87-92)

| Type | Purpose | Kinetics |
|-----|-----------|----------|
| `MOD_MAPK_ERK` (87) | Ca²⁺→Ras→Raf→MEK→ERK→pCREB (consolidation) | Hill n=3, Kd=0.5μM, τ_ERK=5s, τ_nuc=30s |
| `MOD_IKK_NFKB` (88) | Ca²⁺→IKK→IkB→NF-κB→Zif268 (reconsolidation) | Michaelis-Menten, Kd=0.3μM, τ=10s |
| `MOD_MTORC1` (89) | Two-key: AA→Rheb + PI3K/AKT→mTORC1 | τ_Rheb=5s, τ_AKT=8s, τ_mTOR=15s |
| `MOD_PROTEIN_SYNTH` (90) | mTORC1→S6K1/4E-BP→protein translation | τ_S6K=10s, τ_4EBP=8s, τ_translate=60s |
| `MOD_COFILIN` (91) | ERK→LIMK→pCofilin/F-actin (actin cytoskeleton) | τ_LIMK=8s, τ_Cof=5s, τ_actin=30s |
| `MOD_MMP9` (92) | ERK+NF-κB→MMP-9/TIMP/integrins (ECM) | τ_MMP9=20s, τ_TIMP=15s, τ_ECM=100s |

### Gene regulation (consolidation.cpp)

```
Consolidation:     pCREB ──→ c-Fos, BDNF, Arc
                   ERK   ──→ Cofilin, MMP-9
                   ERK+NF-κB ──→ MMP-9

Reconsolidation:   NF-κB ──→ Zif268 (NOT c-Fos!)

Late genes:        c-Fos ──→ GluR1, GluR2
                   Arc   ──→ PSD-95
                   BDNF  ──→ Neurexin, Neuroligin, TrkB
                   MMP-9 ──→ IntegrinB3
                   mTORC1──→ S6K1, 4E-BP
```

### Structural plasticity

| Event | Criterion | Effect |
|---------|---------|--------|
| **New synapse** | Arc>0.6 + PSD-95>0.5 + MMP-9>0.3 + BDNF>0.5 | Creation of a CHEMICAL synapse to the nearest segment (r<50μm) |
| **LTD removal** | Calcineurin>0.3 + weight<0.05 + MMP-9<0.3 | Tombstone (id=0) |
| **Diameter change** | F_actin deviation from 0.5 | Δr = (F_actin-0.5)×0.02×r (±20% max) |
| **AMPAR density** | GluR1+GluR2 from 0.5 | Δd = (GluR-0.5)×0.01 |

### Simulation steps

Step 2.5b (every tick): soma→Nucleus — caching of pCREB, ERK, NF-κB, mTORC1, amino_acid_level.

Step 5 (every tick): applying `ConsolidationEvent` from the thread queue.

### NB_ receptors (Neuroblox-compatible, ID 63–79)

17 modifiers implementing kinetic models from Neuroblox.jl:

| Group | Types | Description |
|--------|------|----------|
| Receptors (8) | NB_NMDA_MoradiFull, NB_AMPA_STA, NB_D1/D2_Modulator, NB_Alpha7ER_nAChR, NB_Beta2_nAChR, NB_CaTRPM4R, NB_MuscarinicR | Q10 correction, Markov gating, Ca²⁺ nanodomain, Hill occupancy |
| Destexhe (6) | NB_AMPA/NMDA/GABAA/GABAB_Destexhe, NB_MsnNMDAR, NB_MsnAMPAR | Damped oscillator kinetics, cross-modulator coupling D1↔NMDAR, D2↔AMPAR |
| Soma (3) | NB_SomaATP, NB_SomaThermal, NB_CREB_Transcription | Mitochondrial ATP synthesis, thermal homeostasis at 37°C, CREB→genes→plasticity |
| **Consolidation (6)** | **MAPK_ERK, IKK_NFKB, mTORC1, ProteinSynth, Cofilin, MMP9** | **Two-stage memory: gene→protein→structure** |

## Intersegmental diffusion (step 2.5)

- **Ionic diffusion** (Na⁺, K⁺, Ca²⁺, Cl⁻, Mg²⁺) per Fick's law with volume normalization
- **ATP diffusion** from the soma to dendrites/axon (DIFF_ATP = 0.14)
- **Heat conduction** (soma as the thermal core of the neuron, DIFF_HEAT = 0.0014)
- **Transcription diffusion** (CREB signal from the nucleus to dendrites, slow 0.01×)

## Binary format .bion

`.bion` is the **single** configuration file of the network. It contains **all** the data: topology, segment parameters, modifiers (including pre- and postsynaptic), simulation mode. No JSON/YAML/XML is required.

### Structure (v9)
```
[NetworkHeader]  84 bytes — magic (0x42494F4E), version (9), mode, counters, num_nuclei, nucleus_version
[Neuron[]]       num_neurons × 24 bytes
[Segment[]]      num_segments × 1692 bytes
[IntraConnection[]] num_intra_connections × 16 bytes
[Synapse[]]      num_synapses × 836 bytes
[Nucleus[]]      num_nuclei × 412 bytes — genes, proteins, signaling caches
```

### Properties
- All data is laid out sequentially, without gaps — zero-copy deserialization is possible
- All structures are POD, `#pragma pack(1)` — FPGA compatibility
- Only indices into arrays, no pointers in the file
- Validation on load: magic, version, index bounds, modifier limits
- Save/reload support (checkpoint) with data identity
- `build_id` and `timestamp` for tracking file provenance

## GPU execution (SYCL)

- All kernels are compiled via Intel DPC++/icpx with `-fsycl`
- Memory: USM (`sycl::malloc_shared`) — unified host + device address space
- Switch dispatch instead of function pointers (incompatible with SYCL device code)
- Atomic operations (`sycl::atomic_ref`) for thread-safe current accumulation
- Automatic CPU fallback for user overrides in ARBITRARY mode
- GPU support (Intel UHD/Iris/Arc, NVIDIA via native CUDA (nvcc)) and CPU

## FPGA compatibility (design)

- All structures are POD with `#pragma pack(1)` — fixed offsets
- Fixed arrays (no dynamic allocation in kernels)
- Minimum branching in the computational path
- Streaming readability of .bion (fixed offsets = memory map)
- The code is ready for synthesis in Xilinx Vitis HLS

## Testing

| Test | Coverage | Assertions |
|------|----------|------------|
| `test_hh` | HH channels, resting potential, spike, pump, cable conduction, myelin, arbitrary overrides | 10 |
| `test_synapses` | AMPA, GABA-A, NMDA, 5-HT1A, D1, mGluR1, pump (long stability) | 12 |
| `test_plugins` | Loading .so plugins, registration, dispatch | 8 |
| `test_nb_receptors` | NB_ receptors (24 tests: Mg²⁺ block, Q10, D1/D2, α7, β2, ATP, CREB, Destexhe, ...) | 43 |
| `test_consolidation` | Consolidation: Nucleus, modifiers 87-92, gene→protein→structure, LTD removal | 44 |
| `test_bion` | Load/save .bion v6, validation, v4→v6 migration | 23 |
| `test_gpu` | Full simulation cycle on GPU: V_m, axial currents, myelin, CPU fallback, synapses | integration |
| `test_plugin_manager` | PluginManager: directory loading, info JSON, lifecycle, edge cases | 18 |
| `test_consolidation_plasticity` | Plasticity: LTP/LTD, calcium triggers, plasticity modifier | 12 |
| `test_gpu_backend` | GPU backend: dlopen, initialization, CPU↔GPU switching, stimuli | 10 |
| `test_delay_buffer` | GPU delay buffer: ring buffer, context switching | 6 |
| `test_v3v4_migration` | BION v3→v4 migration: backward compatibility, direction, new fields | 8 |
| `test_all_modifiers` | All 198 modifier types: registration and parameter validation | 8 |
| `test_diffusion` | Intersegmental diffusion: ions, ATP, temperature per Fick | 6 |
| `test_stimuli` | Stimuli: I_clamp, voltage clamp, ramp, ionic/ligand injections | 10 |
| `test_parametric_overrides` | Parametric overrides: overriding modifier parameters | 8 |
| `test_simulation_host` | Full CPU simulation cycle: loading, ticks, snapshots | 10 |
| `test_bion_loader_host` | Host loader: header validation, bounds, data integrity | 12 |

**Total: 156 tests, 100% PASS.** Valgrind: no leaks.

## Physical constants

- Faraday constant: 96485 C/mol
- Gas constant: 8.314 J/(mol·K)
- Temperature: 37°C (310.15 K) — Nernst equation, HH kinetics
- Specific membrane capacitance: C_SPECIFIC_PF_PER_UM2 = 0.01 pF/μm² (= 1 μF/cm², reduced 100× by myelin)

## Key biological mechanisms

- **Resting potential** is formed by the balance of Na⁺/K⁺ channels and Na⁺/K⁺-ATPase
- **Action potential** (spike): depolarization → Na⁺ channel activation → inactivation → K⁺ repolarization
- **Na⁺/K⁺ pump** counteracts ionic drift by consuming ATP; without it the neuron depolarizes
- **Myelin** blocks ionotropic currents on myelinated regions, leaving nodes of Ranvier active
- **Saltatory conduction**: the spike jumps between nodes of Ranvier, accelerating transmission
- **Probabilistic synaptic release**: modulated by presynaptic autoreceptors, facilitation and depression
- **Neurotransmitter clearance**: exponential decay of concentration in the synaptic cleft

## Biology of the synaptic contact: active zone and PSD

In real CNS synapses, receptors and neurotransmitter release mechanisms are not concentrated over the entire neuron surface, but in microscopic specialized contact zones. Using the full area of a cylindrical segment (2πrl ≈ 300–3000 μm²) to compute synaptic conductance is biologically incorrect.

### Active zone (presynapse, `pre_area`)

The active zone is the region of the presynaptic membrane where docking and exocytosis of synaptic vesicles occur:

- **Size in the CNS:** ~0.05–0.2 μm² (one synapse). Large synapses (calyx of Held, hippocampal mossy fiber boutons) can reach 1–2 μm² with multiple active zones.
- **Structure:** a dense lattice of SNARE proteins (syntaxin, SNAP-25, synaptobrevin), Ca²⁺ channels (Cav2.1/Cav2.2), active zone proteins (Bassoon, Piccolo, RIM, Munc13).
- **Function:** determines the number of vesicles ready for immediate release (RRP — readily releasable pool, ~5–10 vesicles per active zone).
- **Autoreceptors:** located perisynaptically (at a distance of ~0.1–0.5 μm from the active zone) — their density is scaled to `pre_area`.

### PSD — postsynaptic density (`post_area`)

PSD (postsynaptic density) is an electron-dense structure on the postsynaptic side:

- **Size:** ~0.05–0.5 μm². Proportional to the active zone size (the pre- and postsynaptic sides of the synapse are matched in size via trans-synaptic signals).
- **Structure:** receptor clusters (AMPA, NMDA, GABA-A) anchored by scaffold proteins (PSD-95, SAP97, Shank, Homer, GKAP), actin cytoskeleton.
- **Function:** synapse conductance is proportional to the PSD area × receptor density (~100–1000 receptors/μm²). Receptors are distributed non-uniformly across the PSD: AMPA — across the whole area, NMDA — more centrally, mGluR — perisynaptically.

### Perisynaptic organization

- **Perisynaptic receptors:** mGluR2/3, GABA-B, CB1 are located at a distance of ~0.1–1 μm from the PSD edge, outside direct access to the quantum release of glutamate. They are activated upon spillover (diffusion of neurotransmitter beyond the cleft).
- **Astrocytic sheath:** astrocyte processes cover ~60–80% of the synaptic contact, limiting spillover and expressing transporters (EAAT1/2 for glutamate, GAT-1/3 for GABA).
- **Extrasynaptic receptors:** up to 30–50% of receptors can be located outside the synapse on the segment surface. They use the full segment area (geometry fallback when `pre_area`/`post_area` = 0).

### Reflection in BioNeuronCore

- `Synapse::pre_area` and `Synapse::post_area` (default value 0.1 μm²) scale receptor conductance via `Modifier::state[7]` (scratchpad).
- `state[7]` is set before each dispatch in `simulation_host.cpp`, `simulation.cpp`, and the SYCL kernels.
- If `pre_area`/`post_area` = 0 or unset, the geometric segment area is used (backward compatibility).
- Receptor modifiers (ionotropic and metabotropic) and presynaptic modulators read `state[7]` via `effective_area(m, s)`.

---

## Rights and intellectual property

© 2024–2026 **Feshin Vitaly Alexandrovich** / Vitaly Alexandrovich Feshin. All rights reserved.
E-mail: feshin2004@mail.ru · vfeshin@mindcopy.ru · Tel.: +7(918)345-75-18. Krasnodar, Russia / Краснодар, Россия.

State registration certificate of a computer program (Rospatent) **No. 727894_683451**.
Commercial use, distribution and sublicensing without the author's written permission are prohibited.
For the full text, see `LICENSE` and `DISCLAIMER.md` in the platform root.
