# NEURON aIN — ascending interneuron (головастик Xenopus)

Справочник модификаторного состава + плотностей. Синхронизировано с `cpg_config.h` (2026-08-31).

## Источник (литература)
- **Sautois B et al. (2007)** J Comput Neurosci 23:59-77 — Table 2 (проводимости, группа aIN/MN), Table 3 (α/β кинетика).

## Биология
- Восходящий тормозной (GABAergic) интернейрон.
- Repetitive firing (MN_NAK — та же группа aIN/MN, что и мотонейрон).
- Низкая K2P-утечка (K2P_AIN=0.2) — высокая чувствительность к d→a AMPA.

## Модификаторный состав сомы (density)
| Модификатор | type | density | G (мС/см²) | γ (пС) | Po | Формула |
|---|---|---|---|---|---|---|
| MN_NAK | 139 | 1.0 | — | — | — | Sautois Table 2 (aIN/MN) |
| K2P (TREK) | 107 | 0.2 | — | 100 | 0.012 | минимальная утечка (K2P_AIN) |
| NALCN | 133 | 0.11 | 0.3 | 27 | — | G·10/γ |
| Na/K-насос | 7 | 0.6 | 3 µA/см² | — | — | |
| KCC2 | 100 | 0.8 | — | — | — | |
| Cl-leak | 2 | 0.1 | 0.2 | — | — | |
| Ca-buffer | 102 | 0.5 | — | — | — | |
| PMCA | 98 | 0.3 | — | — | — | |

## Аксон
MN_NAK 1.0 + K2P 0.2 + NALCN 0.11 + **Cav2.1 (140) 100** + Ca-buffer 0.5 + PMCA 0.3.

## Сверка (было → стало)
| Было | Стало |
|---|---|
| IONOTROPIC_NAK (HH squid) | MN_NAK (Sautois aIN/MN) |
| PUMP 0.2 | PUMP 0.6 |
| KV42 0.5, SK 0.5, KIR 0.4, KV_DR 0.3 | убрано |
| KCC2 0.5 | KCC2 0.8 |
| NCX 0.2, CA_LEAK 0.08 | убрано |
| без CL_LEAK | CL-leak 0.1 (в `aIN_soma_mods`) |

## Статус
✅ синхронизировано с `cpg_config.h` `aIN_soma_mods()` / `aIN_axon_mods()` (2026-08-31).
