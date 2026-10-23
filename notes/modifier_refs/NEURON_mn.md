# NEURON mn — motoneuron (головастик Xenopus)

Справочник модификаторного состава + плотностей. Синхронизировано с `cpg_config.h` (2026-08-31).

## Источник (литература)
- **Sautois B et al. (2007)** J Comput Neurosci 23:59-77 — Table 2 (проводимости, группа aIN/MN), Table 3 (α/β кинетика).
- **Dale 1995** — repetitive firing MN на sustained деполяризацию.

## Биология
- Холинергический мотонейрон. Repetitive firing (MN_NAK: Na+Kfast+Kslow = delayed rectifier).
- **usAHP = Na⁺/K⁺-насос** (НЕ SK/BK). У Xenopus головастика НЕТ Ca²⁺-зависимого mAHP.
  SK/BK — млекопитающая черта (SK 1–18 мС/см² в млекопитающих MN, не Xenopus).
  Sun & Dale 1998: IKCa (BK) появляется st.41/42 — терминирование burst, НЕ классический mAHP.
- Утечка по Rekling 2000 (K 72% / Na 9% / Cl 18%): K2P+Cl; Na-утечка через INaP (window MN_NAK).

## Модификаторный состав сомы (density)
| Модификатор | type | density | G (мС/см²) | γ (пС) | Po | Формула |
|---|---|---|---|---|---|---|
| MN_NAK | 139 | 1.0 | — | — | — | Sautois Table 2 (Na+Kfast+Kslow) |
| K2P (TREK) | 107 | 3.75 | 0.45 | 100 | 0.012 | G·10/(γ·Po), Rekling 72% |
| Na/K-насос | 7 | 0.6 | 3 µA/см² | — | — | usAHP |
| KCC2 | 100 | 0.8 | — | — | — | |
| Cl-leak | 2 | 0.055 | 0.11 | — | — | Rekling 18% |
| Cx36 | 114 | 2 | — | 14 | — | gap junction MN↔MN |

Примечание: NALCN убрана (в MN Xenopus не обоснована, Na через INaP). BK/SK/KvDR/L-type Ca
убраны — млекопитающая черта, не Xenopus (2026-09-11, литература-агент).

## AIS
Nav1.2 (142) 500 + K2P (AIS_K2P) + NALCN (AIS_NALCN=0.04) — инициация ПД.

## Аксон
MN_NAK 1.0 + K2P 3.75 + **Cav2.1 (140) 100** + Ca-buffer 0.5 + PMCA 0.3.

## Сверка (было → стало)
| Было | Стало |
|---|---|
| IONOTROPIC_NAK (HH squid) | MN_NAK (Sautois repetitive) |
| K2P 2.0 | K2P 4.17 |
| PUMP 0.5 | PUMP 0.6 |
| KIR 0.4 | NALCN 0.11 |
| KV42 0.5 | убрано (A-type не в MN Sautois) |
| KCC2 0.5, Cl 0.3 | KCC2 0.8, Cl 0.1 |
| NCX 0.2, CA_LEAK 0.08 | убрано |

## Статус
✅ синхронизировано с `cpg_config.h` `MN_soma_mods()` / `MN_axon_mods()` + тип-специфичные каналы мотонейрона (BK/SK/KvDR/L-type) (2026-08-31).
