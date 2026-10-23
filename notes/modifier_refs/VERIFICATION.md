# Верификация нейронов (динамика) — находки и правила

**Создано:** 2026-09-09. Дополняет `feedback_neuron_verification.md` (память) и скил `mindcopy` Шаг 3.5.
Цель: калибровка (подставить G) ≠ верификация (нейрон работает биологически).

## Метод (каркас `test_neuron_dynamics.cpp`)

Сборка blueprint → `SaveNetwork`(.bion) → `core_load_bion` → `core_run_tick` (mode=0 BIO, dt=0.025)
→ измерить `core_seg_vm` (V_rest, спайк-счёт) → сверить с литературным target (RMP/порог/паттерн).
Для пейсмекера V_rest = **среднее** V_m за период (не мгновенное — ловит середину спайка).

## Найденные баги и решения (2026-09-09)

| # | Баг | Причина | Решение |
|---|---|---|---|
| 1 | Покой −110…−120 вместо −55…−80 | `assembleNeuron` не задавал `extra_ions` → E_K=−∞ | задать внеклеточные Na140/K5/Ca1.2/Cl130/Mg2 |
| 2 | Runaway (V_m→+150) | ModelDB тотальные µS применены как density без ÷площади | `G[мС/см²]=g[µS]×1000/area[мкм²]`, потом `density=G·10/γ` |
| 3 | Покой деполяризован (−12) | Nav1.6 persistent (`MOD_AXON_NAV16`) на соме | сома → `IONOTROPIC_NAK` (чистый Na/K); Nav1.6 только AIS/узел |
| 4 | Runaway MSN | Kir (inward-rectifier) в тонком дендрите (объём ~28 мкм³) | дендритный Kir 0.15→0.03 (локализация соматодендритная сохранена) |
| 5 | Runaway быстрых Na (STN 538 Гц) | dt=0.1 слишком велик (числ. нестабильность Na) | dt=0.025 (параметр `dt_ms_`, НЕ хардкод) |
| 6 | Heap corruption (`malloc invalid size`) | задача #46 (MechanoStimulus буфер) | ✅ РЕШЕНО 2026-09-09: хардкод `sizeof_Synapse_constant()=836` ≠ `sizeof(Synapse)=856` (cleft 14→15 + bouton_Ca/spine×3) → мусорный pre/post_receptor_count → запись за пределы synapseBuffer. Фикс: accessor `core_synapse_size()`. |

## dt — численный шаг, НЕ биология

- dt меняет ТОЛЬКО точность интегрирования, не физику ионов/каналов.
- dt=0.1 → runaway (числ. ошибка); dt=0.025 → физиологично (STN 10 Гц).
- Вынесен в параметр `SimulationBridge::dt_ms_` (не хардкод).
- ⚠️ CPG-параметры (tau_clearance, веса) откалиброваны под dt=0.1 → после перехода на 0.025
  нужна перепроверка частоты/числа switches (не биология, а подгонка под шаг).

## V_rest 6/6 (первый проход юстировки)

| Нейрон | V_rest | target |
|---|---|---|
| Raphe (5-HT) | −49 | −55 |
| α-MN | −65 | −68 |
| STN | −62 (10 Гц) | −55 |
| PreBötC | −55 | −55 |
| GPe | −53 | −55 |
| MSN D1 | −40 (up-state) | −50 |

## Правила юстировки (кратко)

1. Только литературные рамки γ/Po/G; не выдумывать коэффициенты (no-tricks).
2. Сохранять локализацию (Na→AIS, Cav1.3/HCN→соматодендрит, SK→сома, Cav2.1→бутон).
3. Сохранять градиенты (HCN↑, Kv4.2↑, Cav1.3↓ −0.5).
4. Сохранять миелин/AIS (g-ratio, Na в узле, Nav1.2/Nav1.6).
5. Общие модификаторы не трогать (shared-modifiers-sensitive) — юстировать плотности, не молекулу.
6. Победа → document-neuron-wins + зелёный тест per-нейрон.

## Связанные
[[LOCALIZATION_GRADIENTS]], [[feedback-neuron-verification]], [[neuron-density-po]], [[adapt-extracted-gbar]]
