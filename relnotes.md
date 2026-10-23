# Release Notes — BioNeuronCore

## Изменения (2026-10-09 — 2026-10-12) — CPG альтернация (d→d фикс) + CUDA 13

### CPG головастика — альтернация 0→4 switches
- **d→d плотность исправлена** (`D2D_AMPA_R` 76→2000, = d→c): Ferrario 2018 — din→din ampa = din→cin ampa = 0.593 нС; было 26× слабее → d→d не проводил, Mg²⁺-блок NMDA не снимался на спайке.
- **Восходящая ветвь HdIN** (Roberts 2010): d→d `seg0↔seg0` у ростральных dIN — критична для самоподдержки плавания (без неё ростральные dIN гаснут).
- **Нормализация конвергенции** d→d: density per-synapse по фактической конвергенции (убрала плато каудального dIN +18 мВ).
- **Глицин углублён** (`GLY_REC` 80→160) — ребаунд dR робастнее (Ih_m 0.42, Na_h 0.76).

### CUDA 13 (GPU-бэкенд / ISO-образ)
- Драйвер 580.178.04 = CUDA 13.0; рантайм `cuda-cudart-13-0` (libcudart.so.13) вместо libcudart12 — устранён рассинхрон toolkit↔driver (`cudaMallocManaged` err=46).

## Изменения после v0.20.0 (2026-09-25 — 2026-10-02) — NAK→изоформы, удаление суррогатов, новые модификаторы, BION v12

### Новые модификаторы (MOD_TYPE_COUNT 187→198)
- **Mrgpr (188)** — метаботропный рецептор зуда (Gαq/Gβγ → Ca²⁺ → TRPA1).
- **PIEZO2 (189)**, **TMC1 (190)**, **Cav3.3 (191)** — механосенсоры и T-type Ca²⁺.
- **Родопсин-фотокаскад (192)** — свет→R*→Gt→PDE→cGMP↓→CNG (гиперполяризация палочки).
- **Nav1.2 developmental (193)** — тадпольный аксональный Na⁺ (Ribera & Spitzer).
- **NaP-tadpole (195)** — персистентный Na⁺ dIN (Svensson 2017).
- **Kv2.1 (196)** — медленный delayed rectifier K⁺ (single-spike dIN).
- **Na/K-АТФаза α3 (197)** — динамический насос, usAHP при залпе (Zahler 1997).

### Удаление суррогатов (Single Source / Max Biology)
- Ohm-суррогаты 80/81 удалены (`MOD_GENERIC_VOLTAGE_GATED`/`MOD_GENERIC_LIGAND_GATED`).
- NAK-суррогаты удалены: `MOD_IONOTROPIC_NAK` (0), `DIN_NAK` (125), `CIN_NAK` (138), `MN_NAK` (139), `RB_NAK` (143), `AXON_NAK` (194) → заменены изоформами Nav1.2/Nav1.6/NaP.
- `MOD_POST_INHIBITORY_REBOUND` (93) удалён → ребаунд через HCN + T-Ca²⁺. Слот 124 зарезервирован.
- Нейроны домигрированы на канон изоформ (Nav1.2/Kv1.1-KDR/NaP, Ca_leak 0.001).

### Биофизика / формат
- **BION v9 → v12**: Segment 1692→1800 байт, Synapse 856 байт. Обратная совместимость v3-v11.
- **ION_COUNT 5→7**: +A⁻ (макроанион), +H⁺ (протон, OTOP1). **NT_COUNT 14→15**: +капсаицин.
- **NeuroML импорт/экспорт**: Ohm↔GHK конвертация (γ ↔ condDensity, forwardRate/reverseRate).
- **Диффузия**: bath-релаксация extra_ions к резервуару, cap φ_e ±10 мВ, sg_bernoulli small-x порог.

### Тесты
- 156 CTest-тестов (было 55).

## v0.20.0 (2026-09-04) — 18 новых модификаторов, AQP4, транспортеры, модуляция, PNP φ_e

### Новые модификаторы (MOD_TYPE_COUNT 169→187)
- Ноцицепторные Nav (Nav1.7/1.8/1.9) + TRP-термосенсоры (TRPV1/TRPM8/TRPA1/TRPV3) + KCNQ4.
- Nav1.3, Kir7.1, TRPC6/7, CALHM1/3, OTOP1, ENaC (сенсорные/метаболические).
- Нейрональные транспортеры EAAT3/GlyT2/GAT1 (SLC1/SLC6, shared-хелпер).

### Биофизика
- **AQP4** (аквапорин-4): водный канал, осмотический транспорт (самодостаточный, без BION bump).
- **Модуляция**: TEA→Kv3.1 (IC50 0.2 мМ), PKC→Kv3.1 (фосфо-состояние Ser503, PP1/PP2A), O₂→TASK (метаболическая).
- **Полный PNP**: внеклеточный Пуассон φ_e (solve_poisson_phi_e, Якоби) + электромиграция + V_m=φ_i−φ_e.

### Тесты
- test_nociceptor_channels, test_other_channels, test_gating_dynamics, test_modulation, test_transporters_neuronal.

## v0.19.9 — GOLDEN: stub cleanup, daemon build_id/mode, gene pipeline
**Дата:** 10 июля 2026
- **Daemon**: build_id (DJB2 hash) в MSG_GET_VERSION, mode из BION-заголовка в MSG_GET_NETWORK_INFO.
- **Nucleus**: комментарии смещений обновлены (GENE_COUNT 36→46), core_nucleus_size() в opaque API.
- **myelin_modulator**: документирован как by-design (эффект через segment_has_myelin() dispatch).
- **GPU consolidation**: включён для GPU-бэкендов (USM доступна с CPU).
- **docs**: README/CLAUDE/relnotes обновлены (BION v9, MOD_TYPE_COUNT=124, тесты 55/55).

## v0.19.8 — BION v9 (IntraConnection 16B) + MOD_KV_DELAYED_RECTIFIER
**Дата:** 9 июля 2026
- **BION v9**: IntraConnection 12→16 байт (XMM overflow fix). Добавлен `_pad` для 16-байтного выравнивания. BION_VERSION=9, обратная совместимость v3-v8 через `connection_file_size`.
- **MOD_KV_DELAYED_RECTIFIER (123)**: Kv1.1/Kv2.1 delayed rectifier K⁺ канал. GHK K⁺ flux, m-gate, V_half=−30mV.
- **MOD_TYPE_COUNT**: 123→124 (+KvDR)
- **sizeof(BionIntraConnection)**: 12→16 байт. bion_loader: автоопределение размера по версии.
- Тесты: 55/55.

## v0.19.7 — MOD_NKCC1 (Cl⁻ switch) + BION v8 audit
**Дата:** 8 июля 2026
- **MOD_NKCC1 (122)**: Na-K-2Cl котранспортёр — электронейтральный импорт Cl⁻. Антагонист KCC2. Ключ к онтогенетическому Cl⁻ переключателю в Xenopus.
- **MOD_TYPE_COUNT**: 122→123 (+NKCC1)
- **BION v8**: bion_version_string() исправлен (был хардкод "7").
- **sizeof(Segment)**: комментарий 912→1692 байт.
- Тесты: 55/55.

## v0.19.6 — IPC state frames fix + daemon safe defaults
**Дата:** 2 июля 2026
- **IPC EAGAIN fix**: `sendBuffer()` — обработка EAGAIN/EWOULDBLOCK через `poll(POLLOUT)` при записи больших кадров (2.6MB) на неблокирующий fd. Без фикса кадры молча терялись.
- **daemon safe defaults**: `gpu_enabled=false` (CPU-only по умолчанию), `tcp_port=0` (без TCP). Продакшен задаёт явно в конфиге.
- **GPU-to-CPU fallback**: при отказе GPU `loadNetwork()` не падает, а переходит на CPU-путь.
- **sim_ipc_equiv**: 6/6 PASS (был 0/6), sim_ipc_bridge: All PASS (были All FAIL)

## v0.19.5 — CUDA two-pass diffusion + CPG tuning + Agent Skills
**Дата:** 1 июля 2026
- **CUDA two-pass diffusion fix**: `kernel_diffusion` разделён на `kernel_diffusion_compute` + `kernel_diffusion_apply` — устранена race condition atomicAdd на extra_ions. CPU-GPU паритет: 221 assertions, delta=0.29mV
- **CPG config**: K2P=0.06, PUMP=0.40, NAK_HH=0.8, SK_CIN=1.8, GLY_TAU=80ms — откалиброваны для V_rest ~-67mV
- **Agent Skills**: методология DDD + debugging triage (5 фаз) + planning workflow в CLAUDE.md
- **--gpu-device flag**: daemon принимает индекс GPU устройства в batch mode
- **GPU device flag**: `BIOCORE_GPU_DEVICE` env var + `--gpu-device N` для выбора GPU

## v0.19.4 — Унификация логирования + фикс демона
**Дата:** 25 июня 2026
- **Единый лог-формат**: unixtime [LEVEL] msg во всех подпроектах (Core/Studio/Orch)
- **64 fprintf/printf → DLOG_INFO/WARN/ERROR**: 7 production-файлов (daemon_main, daemon_server, daemon_config, bion_loader, bion_loader_host, plugin_manager, gpu_backend)
- **Фикс демона**: stopSim() перед freeNetwork() — предотвращает гонку при дисконнекте без остановки симуляции
- **Удалена мёртвая nernst()** из receptors_biological.cpp (GHK-миграция)
- **17 warnings заглушено** (65→2): неиспользуемые переменные, fread/write returns
- **33/33 тестов**, sim_ipc_bridge 203/203

## v0.19.3 — Образовательные эксперименты (трёхъязычные README)
**Дата:** 23 июня 2026
- 16 образовательных .bexp экспериментов: 4 бакалавриат + 8 магистратура + 4 аспирантура
- README на русском, английском и китайском для каждого
- Темы: потенциал действия, пассивный кабель, синаптическая интеграция, латеральное торможение, CPG, рабочая память, рецептивное поле, STDP (LTP/LTD/полное окно), эпилепсия, фармакология, гомеостатическая пластичность, бифуркации

## v0.19.2 — Реорганизация экспериментов
- 4 встроенных эксперимента (motion, direction, stdp_ltp/ltd) перенесены в graduate/
- stdp_ltp и stdp_ltd сохранены как отдельные + объединённый stdp_learning
- Индексный README с таблицей всех 16 экспериментов

## v0.19.1 — 12 новых образовательных экспериментов
- Генерация .bexp рецептов для всех уровней обучения
- README с биологическими пояснениями, верификацией и способами воспроизведения

## v0.19.0 — MSG_LOAD_BEXP + --load-bexp batch mode
**Дата:** 23 июня 2026
- `MSG_LOAD_BEXP` (0x0108): загрузка .bexp как рецепта эксперимента
- `--load-bexp` в batch mode: авто-извлечение bion_file_path из JSON-заголовка
- Парсинг events (стимулов) из .bexp, настройка detection-конфигурации

## v0.18.0 — P0 fixes: task progress, graceful shutdown, auto-checkpoint, tiered shadow validation
- `MSG_GET_TASK_PROGRESS`: реальный current/total/percent через task_start_tick/task_total_ticks
- Graceful shutdown: SIGTERM в batch mode → bion_save аварийного чекпоинта → exit(75)
- Авто-чекпоинт в server simulationLoop (checkpoint_interval в SessionContext)
- Биологическая shadow-валидация: COLD сегменты проверяются против пассивной модели (I_leak)
- При отклонении > 0.1mV → автоматическое повышение до WARM/HOT

## v0.17.3 — Batch mode интеграционные тесты
- `test_batch_mode.cpp`: 7 тестов, 14 assert на живом test_single_neuron.bion
- Smoke-тесты (binary exists, help), валидация (rejects missing args), интеграция (BEXP output), params JSON, checkpoint

## v0.17.2 — MSG_RUN_TASK_PARAMS возвращает метрики
- core_compute_metrics() в MSG_RUN_TASK_PARAMS handler
- Метрики: mean_vm, spike_count, synchrony, V_m по типам нейронов

## v0.17.1 — Checkpoint interval + resume support в batch mode
- `--checkpoint-interval N`: авто-сохранение BION каждые N тиков
- `--resume-from PATH`: восстановление из чекпоинта
- volunteer_name в агенте

## v0.17.0 — Распределённые вычисления Stages 1-5
**Дата:** 23 июня 2026
- **Stage 1:** daemon batch mode (`--batch --load-bion --ticks --bexp-output --params-json --seed`), MSG_RUN_TASK_PARAMS (0x090A) с parameter override
- **Stage 2:** Tiered execution (HOT/WARM/COLD классификация) в simulation_host.cpp за флагом tiered_execution
- **Stage 3:** core_compute_metrics() — детерминированные V_m/spike/synchrony метрики
- **Stage 5:** MSG_CHECKPOINT_AND_CONTINUE (0x0804), MSG_GET_TASK_PROGRESS (0x090C)
- **Тесты: 32/32**

## v0.38.3 — Тесты плагинов + SDK-примеры

**Дата:** 22 июня 2026

- **test_plugin_compat:** 5 тестов валидации всех .so плагинов (dlopen → init, RTLD_NOW+RTLD_LAZY, stale-детекция удалённых символов).
- **SDK-примеры:** `example_ca_channel.so` и `example_d2_receptor.so` пересобраны с текущим API (`register_modifier_current` вместо удалённого `register_user_modifier`).
- **dispatch_generated.cpp:** перегенерирован — пользовательские типы 121-122 включены в GPU-диспетчер.
- **Всего тестов: 20** (было 19).

## v0.38.2 — Per-session изоляция + тестовое покрытие 28/28

**Дата:** 22 июня 2026

- **Per-session изоляция (#1-#3):** глобальные переменные консолидации, delay buffer и active_library перенесены в SessionContext. Добавлены `_ctx`-варианты функций: `core_init_consolidation_ctx`, `core_run_tick_ctx` и др. `TickSessionPtrs` — единая реализация без глобалов.
- **Daemon fixes:** streamer flush для всех сессий, removeClient очищает fd из всех streamer-ов.
- **bion_save fix:** `BION_MAGIC` + заполнение всех полей заголовка из `BionData` (ранее был нулевой magic при сохранении чекпоинта).
- **Тесты: 28/28** (было 21)
  - `test_daemon_protocol_full.cpp` — 32 теста, 27 MSG-обработчиков (все непокрытые)
  - `test_plugin_lifecycle.cpp` — 15 тестов (.so lifecycle, имена, .info, границы)
  - `test_core_security.cpp` — 15 тестов (битые BION, некорректные заголовки)
  - `test_core_chaos.cpp` — 10 тестов (nullptr, двойной init, rapid cycles)
  - `test_core_regression.cpp` — 17 тестов (sizeof, константы, bion_validate)
  - `test_core_smoke.cpp` — 4 теста (демон старт, PING, VERSION, PLUGINS)
  - `test_core_performance.cpp` — 6 тестов (run_tick, BION I/O, plugin timing)
- **Платформа тестирования:** Catch2 v2 (single-header), 28 executables через ctest.

## v0.38.1 — Унификация парсеров + fix daemon_server

**Дата:** 19 июня 2026

- **daemon_config.cpp:** переход на общий `json_parser.h` из BioNeuronOrchestrator
- **daemon_server.cpp:** исправлены ~40 ошибок компиляции
  - Добавлен `default_session_` (сессия 0)
  - Имена полей: `segments()` → `_segments()` (и аналогичные)
  - `SessionContext::freeNetwork()` — inline-определение освобождения памяти
  - `MAX_STIM` → `SessionContext::MAX_STIM`
  - `sim_thread_` → `current_ctx_->sim_thread`
  - `simulationLoop()` — не-static метод (использует `current_ctx_`)
  - Per-session wrappers: `loadNetworkForSession()`, `startSimForSession()` и др.
  - `isNetworkLoaded()`, `segs()`, `syns()` и др. — inline-определения
- **daemon_config.cpp:** валидация полей: tcp_port, max_sessions, log_max_size_mb, log_max_files
- Include-path: `-I../BioNeuronOrchestrator` для daemon_config в CMakeLists.txt

## v0.38.0 — Разделение Core/Studio на два процесса (IPC)

**Дата:** 18 июня 2026

- **Core Daemon:** демон `biocore-daemon` — standalone процесс, слушает Unix-сокет + TCP (порт 6789).
- **IPC-протокол:** бинарный wire-протокол (30+ типов сообщений: sim, stimuli, state, plugins, consolidation, checkpoint).
- **State streaming:** STATE_FRAME (сырые Segment/Synapse массивы) для эффективной передачи V_m/ионов/NT в GUI.
- **Конфиг:** `/etc/biocore/biocore.conf` (JSON), без персональных данных.
- **Systemd-юнит:** `biocore-daemon.service`, пользователь `biocore`, sandbox security.
- **Лог-система:** `daemon_log.h` — ротация по размеру (как в Studio), `[timestamp] [LEVEL]`.
- **Плагины:** 111/111 плагинов загружаются (`-rdynamic` + `RTLD_LAZY`).
- **GPU-бэкенд:** dlopen `libbiocore_gpu_backend.so` в демоне, GPU-путь в simulationLoop.
- **Аргументы:** `--config`, `--plugins-path`, `--ready-fd N`.
- **Пакетирование:** `mindcopy-daemon` .deb пакет (preinst/postinst/prerm/postrm).
- **Тесты:** `test_daemon_protocol` (9 тестов протокола).

## v0.16.7 — Исправление загрузчика BION и обновление тестов

**Дата:** 16 июня 2026

- **bion_loader_host.cpp:** добавлен код ошибки BION_ERR_FILE_READ (-11), все 8 вызовов fread обёрнуты проверкой возврата.
- **bion_format.h:** `bion_version_string()` исправлен — возвращает "6" (не "5").
- **Документация:** CLAUDE.md/README.md — BION v4/v5→v6, MAX=4→6.
- **Тесты:** обновлены константы (sizeof(Nucleus)=332, BION_VERSION=6, MOD_TYPE_COUNT=121). Core: 18/18 тестов.

## v0.16.6 — Электрорецепция + Магниторецепция (TODO 3: части 3+4)

**Дата:** 15 июня 2026

- **MOD_ELECTRORECEPTOR_AMPULLARY (118)** — ампулы Лоренцини: гель-канал (τ=100ms, low-pass), Ca²⁺-адаптация (τ=5s), направленное тюнингование, GHK (Na⁺50%/K⁺35%/Ca²⁺15%)
- **MOD_MAGNETITE_MECHANORECEPTOR (119)** — Fe₃O₄-цепочки: крутящий момент τ=m×B → MET-каналы, sin²(θ) зависимость, Ca²⁺-адаптация (τ=8s), GHK (Na⁺45%/K⁺30%/Ca²⁺25%)
- **MOD_CRYPTOCHROME_MAGNETIC (120)** — Cry4 радикальные пары [FAD•⁻+Trp•⁺]: свето-зависимый (λ=400-500nm), sin²(θ) угловая зависимость, МОДУЛЯТОР (модулирует MOD_LIGHT_SENSOR)
- MOD_TYPE_COUNT=121

## v0.16.5 — Вестибулярный аппарат (TODO 3: часть 2)

**Дата:** 15 июня 2026

- **MOD_OTOLITH_HAIR_CELL (116)** — отолитовая волосковая клетка: MET-каналы, cosine-тюнингование, Ca²⁺-адаптация миозиновых моторов (τ=30ms), тонический P_open~0.15 (гравитация), GHK (Na⁺60%/K⁺30%/Ca²⁺10%)
- **MOD_SEMICIRCULAR_CUPULA (117)** — купула полукружного канала: торсионный маятник (ω₀², 2ζω₀), push-pull билатеральность, фазическая Ca²⁺-адаптация (τ=10ms), GHK (Na⁺55%/K⁺35%/Ca²⁺10%)
- MOD_TYPE_COUNT=118

## v0.16.4 — Аудиотаксис (TODO 3: часть 1)

**Дата:** 15 июня 2026

- **MOD_VIBRATION_SENSOR (115)** — вибрационный/акустический сенсор: механотрансдукция, сигмоидная активация, habituation+sensitization, GHK
- MOD_TYPE_COUNT=116

## v0.16.3 — Сенсорная адаптация (H2)

**Дата:** 14 июня 2026

- **Двух-процессная модель адаптации** для всех 4 сенсорных модификаторов:
  - Контр-направленная (habituation): GRK/β-arrestin/Ca²⁺-CaM-инактивация
  - Со-направленная (sensitization): cAMP/PKA/Ca²⁺-рековерин-прайминг
- **MOD_PHYSICAL_SENSOR (6)**: +habituation (state[2]), +sensitization (state[3]), настраиваемые k_hab/k_sens/tau_hab
- **MOD_LIGHT_SENSOR (95)**: +sensitization (state[3], tau=60s, k_sens=0.5 — Ca²⁺/рековерин/GCAP)
- **MOD_OLFACTORY_RECEPTOR (108)**: +sensitization (state[3], tau=10s, k_sens=0.3 — cAMP/PKA)
- **MOD_HUNGER_SENSOR (94)**: +habituation (state[1], tau=300s — AMPK/mTOR), +sensitization (state[2], tau=600s)
- **Общие хелперы** в `plugin_helpers.h`: `sensor_habituation_step()`, `sensor_sensitization_step()`, `sensor_adaptation_gain()`
- CPU/GPU delta = 0.00 mV для физического и светового сенсоров
- GPU-fallback (`hh_biological.cpp`) синхронизирован с CPU-плагинами

## v0.16.2 — MOD_CONNEXIN_CX36 + gap junction delay (G2)

**Дата:** 12 июня 2026

- **MOD_CONNEXIN_CX36 (114)**: молекулярная биология щелевых контактов
  - Синтез Cx36 в ER → сборка коннексона (6×Cx36) → мембрана
  - Фосфорилирование: CaMKII→↓P_open, PKA(cAMP)→↑P_open
  - pH-зависимость: закрытие при pH<6.8
  - Интегрирован в gap junction: g_eff *= Cx36_pre * Cx36_post
- **MOD_TYPE_COUNT** 114→115
- 18/18 тестов

## v0.16.1 — F2+F3+G1: Density events + Ca²⁺→CREB + gap junction delay

**Дата:** 11 июня 2026

- **F2 — Gene→modifier density events**:
  - CONSOL_EVENT_MODIFIER_DENSITY (4) в types.h
  - Gene→modifier mapping (SCN1A→NaK, KCND2→Kv4.2, KCNQ2/3→Kv7, etc.)
  - applyPendingEvents: density events → segment.modifiers[].density
- **F3 — Ca²⁺→CREB→TF intrinsic homeostasis**:
  - cacheSignalsFromSoma: Ca²⁺_soma → pCREB (Hill n=2, Kd=0.5μM)
  - pCREB → TF expression → ion channel genes → protein → density
- **G1 — Gap junction delay buffer**:
  - RC-фильтр первого порядка (tau=delay_ms) на I_gap
  - CPU (simulation_host.cpp) + GPU (kernels_part2.sycl)
- 18/18 тестов

## v0.16.0 — BION v6 + 36 генов + GRN (F1)

**Дата:** 11 июня 2026

- **GENE_COUNT** 20→36: 6 TFs + 10 ионных каналов
- **Nucleus** 204→332 байта
- **BION_VERSION** 5→6, v5→v6 миграция
- Новые гены: TF_LHX3, TF_ISLET1, TF_PAX2, TF_BRN3A, TF_MNX1, TF_LMX1B,
  SCN1A(Nav1.1), KCND2(Kv4.2), KCNQ2, KCNQ3, CACNA1G(Cav3.1), HCN1, HCN2,
  KCNJ2(Kir2.1), KCNK2(K2P), KCNMA1(BK)
- TF→ion channel gene regulation в ConsolidationThread
- 18/18 тестов

## v0.15.0 — Biological STDP + BCM θ_m + PP1/DARPP-32 (C1+C3)

**Дата:** 11 июня 2026

- **Суррогатные timing traces УДАЛЕНЫ**: syn->facilitation/depression больше не ручные таймеры
- **BCM θ_m**: скользящее среднее Ca²⁺² (τ≈5min) в syn->depression
- CaMKII (Kd=1μM) vs Calcineurin (Kd=0.05μM) естественно разделяют LTP/LTD
- **MOD_PP1_INHIBITOR_DARPP32 (113)**: D1→cAMP→PKA→p-DARPP32→inhibit PP1→LTD↓
- PP1_active интегрирован в AMPAR trafficking
- Pre-spike facilitation=1.0 удалён из CPU и GPU
- **MOD_TYPE_COUNT** 112→114
- 18/18 тестов

## v0.14.5 — MOD_KV7_MCURRENT (KCNQ2/3 M-current)

**Дата:** 11 июня 2026

- **MOD_KV7_MCURRENT (112)**: медленный K⁺ канал (tau~100ms), PIP2-зависимость
- GHK-поток K⁺, V_half=-38mV, без инактивации
- CPU+GPU, .so плагин, 6 биофизических тестов
- CPU=GPU: delta=0.00mV
- **MOD_TYPE_COUNT** 112→113
- 18/18 тестов

## v0.14.4 — K+ conservation + GPU-CPU parity + MOD_THERMAL_NOISE

**Дата:** 11 июня 2026

- **K+ ионная консервация** (hh_biological.cpp):
  - extra_ions[K] добавлен в 5 каналов: K2P_LEAK, KIR_CHANNEL, SK_CHANNEL, BK_CHANNEL, PIR_HCN
  - Исправлены знаки в Kv4.2 и Generic V-gated (intra/extra были инвертированы)
- **GPU-CPU паритет** (kernels_part1.sycl, simulation.cpp):
  - Тепловой шум ±0.05mV добавлен в GPU update_membrane (идентичен CPU)
  - Tick передан в GPU-ядро для воспроизводимого шума
  - NT race condition fix: `=` → atomic fetch_add (GPU) / `+=` (CPU)
  - extra_only guard в GPU-диффузии (скип intra для glial connections)
  - V_m guard CPU/GPU alignment [-200, +100]
  - STDP/plasticity (CaMKII/Calcineurin/eCB) в GPU-путь (step 3.5)
- **MOD_THERMAL_NOISE (111)**: стохастический шум на xorshift32 PRNG
  - .so плагин + GPU fallback + dispatch
  - state[0]=σ(mV), state[1]=seed_offset
- **MOD_TYPE_COUNT** 111 → 112
- 18/18 тестов

## v0.14.3 — Полный biological_library + GPU-safe GHK

**Дата:** 10 июня 2026

- `receptors_biological.cpp`: типы 98-107 (PMCA, NCX, KCC2, Ca_leak, Ca_buffer,
  SK, BK, NaP, KIR, K2P) добавлены в `init_biological_library()`. Исправляет
  CPU-путь без плагинов (ранее использовал NULL-указатели).
- `hh_biological.cpp`: `safe_ghk_denom()` с epsilon 1e-7f в `k2p_leak_current`
  (защита от fp32 underflow на GPU).
- 18/18 тестов PASS.

## v0.14.2 — GPU NaN fix + биологические параметры

**Дата:** 10 июня 2026

- **Критические исправления GPU:**
  - `kernels/kernels_part1.sycl`: пропуск axial current при `coupling_resistance==0`
    (glial `connect_extra` вызывал деление на 0 → NaN)
  - `kernels/kernels_part1.sycl`: NaN-guard в `update_membrane` (`V_m!=V_m` → -65mV)
  - `kernels/kernels_part2.sycl`: формула release `(1+facil)/(1+depress)`
    вместо `facil/depress` (NaN при 0/0 в покое)
  - `plugin_helpers.h`, `receptors_biological.cpp`: `safe_ghk_denom()` epsilon 1e-7f
  - `hh_biological.cpp`: NaK_HH — проводимость теперь с `*0.01f` (как CPU-плагин),
    исправлен 100x drift концентраций ионов
  - `simulation.cpp`: NaN-защита концентраций после диффузии
- **Биологические параметры (CPU плагины → GPU значения):**
  - NaP: g_max 1.0→0.08 (BIOREF 0.01-0.1 mS/cm²)
  - SK: g_max 1.5→0.5 (BIOREF 0.5-5 mS/cm²)
  - BK: g_max 2.5→2.0 (BIOREF 1-20 mS/cm²)
  - KCC2: v_max 0.002→0.0008 (BIOREF default)
  - KIR: g_max 1.5→1.0 (BIOREF 0.1-1 mS/cm²)
- Тесты: `test_hh.cpp`, `test_synapses.cpp` — обновлены пороги
  под корректное биологическое масштабирование

## v0.14.1 — Расширение тестового покрытия

**Дата:** 10 июня 2026

- +5 тестовых файлов: consolidation_plasticity, delay_buffer, gpu_backend, plugin_manager, v3v4_migration
- Итого: 18/18 тестов, 100% PASS
- Valgrind: утечек нет

## v0.14.0 — Coordinated Switch на биофизические единицы

**Дата:** июнь 2026

### Биофизическая миграция (coordinated switch)
- **C_SPECIFIC_PF_PER_UM2 = 0.01** (1 µF/cm² = 0.01 pF/µm²) — биофизическая удельная ёмкость
- **Все токи масштабированы в биофизические единицы:**
  - Conductance-based: `g × density × area × dV × 0.01`
  - GHK-based: `P × density × area × 1e-11f` (вместо 1e-9f)
  - Насосы/NCX: biological транспорт ионов + scaled return (×0.01)
- **Глобальная компенсация убрана:** `I_buffer *= C_SPECIFIC_PF_PER_UM2` удалена из GPU и CPU путей
- **38+ файлов конвертированы:** 34 .so плагина + 2 fallback-файла + plugin_helpers.h + 2 ядра симуляции
- **C_m теперь биофизическая:** `C_m = area × 0.01` (pF) с clamp 0.1 пФ

### Исправления тестов
- `test_consolidation`: MOD_TYPE_COUNT 109→111
- `test_bion_loader_host`: coupling_resistance пересчитывается из геометрии (1e6×L/πr²)
- `test_synapses`: C_m формула обновлена на C_SPECIFIC

### Общее
- `MOD_TYPE_COUNT`: 111 (добавлены MOD_GLIAL_KIR41=109, MOD_GLIAL_NAK_PUMP=110)
- Core: **13/13 PASS** (0 failures)
- Studio: **162/162 PASS** (0 failures)

## v0.13.0 — Ca²⁺/K⁺/Cl⁻ гомеостаз + Каналы ритмогенеза + STDP

**Дата:** июнь 2026

### Ca²⁺/K⁺/Cl⁻ гомеостаз (5 новых модификаторов: 98-102)
- **MOD_PMCA** (98) — Ca²⁺-АТФаза плазмалеммы: ATP-зависимый выброс 1Ca²⁺, Hill n=2, K_m=300nM, электрогенный
- **MOD_NCX** (99) — Na⁺/Ca²⁺ обменник: 3Na⁺ in : 1Ca²⁺ out, V-зависимый, allosteric Ca²⁺-активация
- **MOD_KCC2** (100) — K⁺-Cl⁻ котранспортёр: электронейтральный 1K⁺:1Cl⁻ out, критичен для глицинового/GABA-A торможения
- **MOD_CA_LEAK** (101) — Пассивная Ca²⁺ утечка: GHK Ca²⁺ (z=2), без гейтинга, поддерживает базальный [Ca²⁺]i
- **MOD_CA_BUFFER** (102) — Быстрый Ca²⁺ буфер: парвальбумин-подобный, кинетика on/off, B_total=150μM, K_d=500nM

### Ионные каналы ритмогенеза (4 новых модификатора: 103-106)
- **MOD_SK_CHANNEL** (103) — SK (KCa2.x): чисто Ca²⁺-активируемый K⁺, AHP, Hill n=4, K_d=400nM, GHK K⁺
- **MOD_BK_CHANNEL** (104) — BK (KCa1.1): Ca²⁺+V-активируемый K⁺, быстрая реполяризация, Ca²⁺-сдвиг V_half, GHK K⁺
- **MOD_NAP_CHANNEL** (105) — NaP (Nav1.6): персистентный Na⁺, без инактивации, подпороговая активация (-52mV), GHK Na⁺
- **MOD_KIR_CHANNEL** (106) — Kir (Kir2.x): входящий выпрямитель K⁺, Больцман+rectification, стабилизация RMP, GHK K⁺

### STDP-рефакторинг
- Непрерывный CaMKII/Calcineurin механизм + timing-trace модуляция (facilitation/depression)
- Pre-before-post → boost LTP rate. Post-before-pre → boost LTD rate
- Сброс timing traces после потребления (предотвращает ложные срабатывания)
- Ингибиторные синапсы (GABA-A, GABA-B, Glycine): инвертированный знак dw

### Тюнинг Na/K-насоса
- p_rate по умолчанию: 0.1 → 0.15 (плагин + GPU fallback)
- Биологически обоснованно: разные изоформы α-субъединиц в нейронах CPG

### Архитектурный принцип
**Все 9 новых модификаторов работают через GHK + прямое движение ионов.**
Ни одного `I = g*(V-E_rev)`. Каждый модификатор явно обновляет `s->intra_ions[]`.

### Общее
- `MOD_TYPE_COUNT`: 98 → 107
- Все плагины: `.cpp` + `.info` (полные нейробиологические описания)
- GPU fallback: все 9 типов в `hh_biological.cpp` + switch cases в `kernel_dispatch.h`
- CPU: отдельные `.so` плагины (автосборка через CMake glob)

### Тесты
- Core: 13/13 — 100%
- Studio: 158/159 (sim_regression — пре-существующий)

## v0.12.0 — Gap Junction Gating + D1/Gi/o Fix

**Дата:** июнь 2026

### Электрические синапсы (gap junctions)
- **Быстрый Vj-гейт**: Больцмановская кривая (V_half=40mV, k=5mV, tau=5ms), резидуальная проводимость 10%
- **Медленный Ca²⁺/CaM-гейт**: Hill-активация кальмодулина (n=2, EC50=300nM, tau=100ms), полное закрытие канала
- Состояния гейтов хранятся в `Synapse::facilitation` (m_fast) и `Synapse::depression` (m_slow) — формат `.bion` не затронут
- CPU (`simulation_host.cpp`) + GPU (`kernels_part2.sycl`)

### Исправление знака тока
- **D1 (Gs) и Gi/o**: `return -(-I_K)` → `return -I_K` в `gs_ghk_effect` и `gi_o_ghk_effect` (`plugin_helpers.h`, `receptors_biological.cpp`)
- K⁺ эффлюкс теперь корректно трактуется как гиперполяризующий ток

### Попутные исправления
- `mod_09_nmda.cpp`: исправлена потеря возврата `ionotropic_ghk_current()`
- `mod_63_nb_nmda_moradi_full.cpp`: удалён stray GHK-код из `temp_mod()`
- `mod_67_nb_alpha7_er_nachr.cpp`, `mod_75_nb_msn_nmdar.cpp`: удалены incomplete GHK-блоки

### Тесты
- Core: 13/13 (5 новых тестов: 3× gap junction gating + 2× Gi/o hyperpolarization)
- Все тесты `test_synapses` проходят

## v0.11.1 — Исправления GPU/CPU синхронизации

**Дата:** июнь 2026

### Исправления ядра симуляции
- **GPU compute_modifier_currents**: `I_buffer = I_total` → `I_buffer += I_total` — стимулы больше не затираются на каждом тике
- **GPU update_membrane**: добавлен `C_m < 1.0f` clamp (как в CPU-пути)
- **core_run_tick**: `duration_ms` корректно конвертируется в тики через `dt` (было прямое сравнение мс с тиками)
- **CurrentStimulus**: поля в упакованной структуре корректно читаются GPU-путём

### Тесты
- 159 автоматических тестов (Core + Studio), 0 провалов
- Новый регрессионный тест `sim_regression_bugs` (28 проверок)
- `sim_gpu_bridge` тестирует GPU-путь через SimulationBridge
- `sim_gpu_stim` проверяет packed-структуры GPU-пути

## v0.11.0 — GHK-миграция + PIR + сенсоры

**Дата:** июнь 2026

### PIR: Постингибирующий отскок (3 новых модификатора)
- `MOD_POST_INHIBITORY_REBOUND` (93) — зонтичный делегат на 96+97
- `MOD_PIR_TTYPE_CALCIUM` (96) — T-type Ca²⁺ (Cav3.x): GHK поток Ca²⁺ (z=2), m³h-гейтинг, окно деинактивации (-70..-30mV), 5-HT/DA-подавление
- `MOD_PIR_HCN_CHANNEL` (97) — HCN1/2/4 (Ih): раздельные GHK потоки Na⁺(40%)/K⁺(60%), саг-эффект, DA-подавление, E_rev возникает из соотношения проницаемостей

### Сенсоры (2 модификатора)
- `MOD_HUNGER_SENSOR` (94) — интероцептивный сенсор голода: ATP-монитор → неселективный катионный канал (Na⁺ 60%/K⁺ 30%/Ca²⁺ 10%) через GHK
- `MOD_LIGHT_SENSOR` (95) — фоторецептор (CNG-канал): Na⁺ 50%/K⁺ 30%/Ca²⁺ 20% через GHK, адаптация (tau=2s)

### GHK-миграция (59 плагинов)
- **Полный отказ от I=g*(V-E)**: все электрические токи через уравнение Гольдмана-Ходжкина-Катца
- **3 шаблонные функции переписаны** в `plugin_helpers.h`: `ionotropic_ghk_current()`, `gi_o_ghk_effect()`, `gs_ghk_effect()`
- 9 ионотропных рецепторов (AMPA..TRPV1) → GHK катионные/анионные потоки
- 24 Gi/o рецептора → GHK K⁺ через GIRK
- 11 Gs рецепторов → GHK K⁺ (закрытие каналов)
- 12 NB_-рецепторов → GHK с сохранением Mg²⁺-блока
- physical_sensor (06) → GHK неселективный катионный
- HH NaK (00) → E_leak = nernst(K) вместо хардкода
- **0 хардкодных E_rev во всех плагинах**

### Общее
- `MOD_TYPE_COUNT` = 98 (было 93)
- `ghk_monovalent()`, `ghk_divalent()`, `ghk_anion()` — общие хелперы в `plugin_helpers.h`
- Все плагины напрямую двигают ионы (`s->intra_ions[X] += ...`), V_m обновляется из баланса зарядов

### Тесты
- `test_consolidation.cpp`: MOD_TYPE_COUNT=98
- `test_simulation_host.cpp`: фикс myelin test (шум + накопление 100 тиков)
- 13/13 тестов пройдены

---

## v0.10.0 — Консолидация/реконсолидация (двухстадийная память)

**Дата:** июнь 2026

### Генная система (Nucleus)
- Структура `Nucleus` (204 байта): 20 генов пластичности, сигнальные кэши, настройки
- 20 генов: CREB, c-Fos, Zif268, Arc, BDNF, Cofilin, MMP-9, TIMP, GluR1, GluR2, PSD-95, Neurexin, Neuroligin, Dynamin, Clathrin, IntegrinB3, TrkB, mTOR, S6K1, 4E-BP
- `gene_activity[20]` — уровни мРНК (0..1), `protein_level[20]` — уровни белка (0..1)

### 6 новых модификаторов (87-92)
- `MOD_MAPK_ERK` (87): Ca²⁺→Ras→Raf→MEK→ERK→pCREB, cross-talk с CREB
- `MOD_IKK_NFKB` (88): Ca²⁺→IKK→IkB→NF-κB→Zif268 (реконсолидация)
- `MOD_MTORC1` (89): двухключевая (AA + PI3K/AKT), ATP-ингибирование
- `MOD_PROTEIN_SYNTH` (90): mTORC1→S6K1/4E-BP→трансляция, потребление ATP
- `MOD_COFILIN` (91): ERK→LIMK→pCofilin/F-actin (актиновое ремоделирование)
- `MOD_MMP9` (92): ERK+NF-κB→MMP-9/TIMP/интегрины (ECM деградация)

### ConsolidationThread
- Фоновый поток с настраиваемым интервалом (0=OFF), per-neuron override
- Ген→белок→структура: 3 фазы за цикл
- Консолидация: pCREB→c-Fos/BDNF/Arc, ERK→Cofilin/MMP9
- Реконсолидация: NF-κB→Zif268 (отличный от CREB путь)
- Структурная пластичность: synaptogenesis, LTD-удаление (Calcineurin>0.3+weight<0.05), Δдиаметр (Cofilin/F-actin), ΔAMPAR (GluR1/GluR2)
- Reactivation buffer (1000 тиков V_m) для детекции воспоминания
- Шаг 2.5b: сома→Nucleus связь (кэш pCREB/ERK/NF-kB/mTORC1/AA)
- Шаг 5: применение ConsolidationEvent из очереди

### BION v5
- NetworkHeader: 68→84 байта (+num_nuclei, +nucleus_version, +reserved)
- Nucleus[] в .bion: загрузка/сохранение 204 байта/нейрон
- v4→v5 миграция: автоматическое создание default nuclei при загрузке старых файлов
- `bion_calc_size()` обновлён для учёта nuclei

### C API
- `core_init_consolidation`, `core_start_consolidation`, `core_stop_consolidation`
- `core_set/get_consolidation_interval`, `core_get_pending_consolidation_events`
- `core_get_nucleus_snapshot`

### Тестирование
- `test_consolidation`: 15 кейсов, 44 assertions (структуры, модификаторы 87-92, интеграция)
- MOD_TYPE_COUNT: 87→93

## v0.9.0 — Расширение диффузии: intra_nt, extra_ions, neurotransmitter

### Generic voltage-gated канал (MOD_GENERIC_VOLTAGE_GATED = 80)
- Параметрическая кинетика Ходжкина-Хаксли без хардкода
- state[]: V½_fwd, k_fwd, rate_fwd, V½_rev из NeuroML gateHHrates
- Раздельные forward/reverse параметры: alpha(V) из forward, beta(V) из reverse
- Три формы кинетики: HHExpLinearRate, HHExpRate, HHSigmoidRate
- Инактивация (h-гейт) с собственным V½_h

### Generic ligand-gated рецептор (MOD_GENERIC_LIGAND_GATED = 81)
- Hill-кинетика (n=2) с настраиваемыми EC₅₀, tau_rise, tau_decay, g_max
- E_rev: Нернст (state[5]=0) или фиксированный из NeuroML

### Молекулярный каскад STDP (типы 82-86)
- MOD_CAMKII (82): Ca²⁺/кальмодулин-зависимая киназа II, автофосфорилирование
- MOD_CALCINEURIN (83): Ca²⁺/кальмодулин-зависимая фосфатаза PP2B
- MOD_CA_L_TYPE (84): L-type Ca²⁺ канал (Cav1.x, HVA, CDI)
- MOD_KV42 (85): Kv4.2 A-type K⁺ канал с CaMKII-фосфорилированием
- MOD_ECB_SYNTHESIS (86): эндоканнабиноидный синтез (2-AG) из Ca²⁺
- AMPAR trafficking в simulation_host.cpp шаг 3: CaMKII/Calcineurin → Δsyn.weight
- eCB retrograde signaling: 2-AG → cleft → пресинаптический CB1

### Cm fix
- CPU-путь теперь делит I_buffer на C_m (ёмкость мембраны) как GPU-путь
- V_m += I_buffer / (2π·r·l) · dt — физиологичная динамика

### Исправления
- MOD_TYPE_COUNT: 80 → 87
- INFO_CACHE_SIZE: 80 → 87
- Параметрические alpha/beta с раздельными forward/reverse параметрами
- NMDAR Mg²⁺ блок: coincidence detection (глутамат + деполяризация)

## v0.9.0 — Расширение диффузии: intra_nt, extra_ions, neurotransmitter
- intra_nt[NT_COUNT], extra_ions/neurotransmitter диффузия, Стокс-Эйнштейн
- Segment: 932 → 988 байт

## v0.8.0 — Стимуляция в Core
- I_clamp, VoltageClamp, RampStimulus, IonInjection, LigandInjection

## v0.7.0 — CPU/GPU переключатель
- gpu_backend.cpp, libbiocore_gpu_backend.so, dlopen

## v0.6.2 — state[7] scratchpad
- pre_area/post_area для синаптических контактных площадей

## v0.6.0 — BioNeuronSDK
- plugin-devkit.h, biocore-gen-dispatch, cmake-модуль

## v0.5.0 — 63 .so плагина
- extern "C" регистрация, modifier_current_table[128]

## v0.4.0 — BION v4
- Хост-загрузчик, инфраструктура плагинов (dlopen)

## v0.3.0 — Бинарный .bion v3
- Полная валидация, v3→v4 миграция

## v0.2.0 — Синапсы
- 63 рецептора, 12 пресинаптических модуляторов

## v0.1.0 — HH каналы
- Na/K, Ca, Cl, осевые токи, SYCL-ядра

## v0.38.0 (2026-06-19) — Multi-Client + Multi-GPU Daemon

### Новое
- **Multi-Client изоляция:** SessionContext для независимой симуляции нескольких Studio через одного демона
- **Multi-GPU:** перечисление устройств SYCL, создание симуляций на конкретном GPU
- **MSG_FLAG_SESSION:** обратно-совместимый протокол с сессиями (session_id)
- **MSG_CREATE/DESTROY_SESSION:** управление сессиями через IPC
- **MSG_LIST_DEVICES:** получение списка GPU-устройств
- **MSG_RUN_TASK:** запуск фоновых задач через IPC (без Studio)
- **GpuDeviceInfo:** структура с информацией об устройстве в types.h
- **DaemonConfig:** max_sessions, gpu_device_allowlist
- **Валидация:** проверка gpu_device_index, session_id, bion_path, ticks

### Изменения
- daemon_server: рефакторинг (сессионный реестр, алиасы для совместимости)
- gpu_backend.cpp: device_count, get_device_info, create_on_device
- daemon_session.h/cpp: новый файл — SessionContext
- Типы ошибок: ERR_MAX_SESSIONS, ERR_INVALID_SESSION, ERR_GPU_DEVICE_UNAVAILABLE


## Права и интеллектуальная собственность

© 2024–2026 **Фешин Виталий Александрович** / Vitaly Alexandrovich Feshin. All rights reserved.
E-mail: feshin2004@mail.ru · vfeshin@mindcopy.ru · Тел.: +7(918)345-75-18. Краснодар, Россия / Krasnodar, Russia.

Свидетельство о государственной регистрации программы для ЭВМ (Роспатент) **№ 727894_683451**.
Коммерческое использование, распространение и сублицензирование без письменного разрешения автора запрещены.
Полный текст — см. `LICENSE` и `DISCLAIMER.md` в корне платформы.
