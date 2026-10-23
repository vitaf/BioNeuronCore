# NEURON cIN — commissural interneuron (головастик Xenopus)

Справочник модификаторного состава + плотностей. Синхронизировано с `cpg_config.h` (2026-08-31).

## Источник (литература)
- **Sautois B et al. (2007)** J Comput Neurosci 23:59-77 — Table 2 (проводимости), Table 3 (α/β кинетика).
- **Ribera & Spitzer 1990** — delayed-firing A-механизм (Xenopus).
- **Zhang & Sillar 2012, Heitler 2025** — сильный Na/K-насос → usAHP, обрыв эпизода.

## Биология
- Тормозной (глицинергический) перекрёстный CPG-интернейрон.
- Delayed burst: single spike → gap → burst (A-type де-инактивация после гиперполяризации).
- SK (Ca²⁺-AHP) терминирует burst (иначе репетитивная стрельба → глицин не отпускает dIN).

## Модификаторный состав сомы (density)
| Модификатор | type | density | G (мС/см²) | γ (пС) | Po | Формула |
|---|---|---|---|---|---|---|
| CIN_NAK | 138 | 1.0 | — | — | — | Sautois Table 2 (Na+Kfast+Kslow+A) |
| SK (KCa2.x) | 103 | 6.0 | 0.5-5 | — | — | Ca²⁺-AHP (терминирует burst) |
| K2P (TREK) | 107 | 8.125 | 0.975 | 100 | 0.012 | G·10/(γ·Po) |
| NALCN | 133 | 0.088 | 0.237 | 27 | — | G·10/γ |
| Na/K-насос | 7 | 1.0 | 5 µA/см² | — | — | сильный usAHP |
| KCC2 | 100 | 0.8 | — | — | — | |
| Cl-leak | 2 | 0.1 | 0.2 | — | — | |
| Ca-buffer | 102 | 0.5 | — | — | — | |
| PMCA | 98 | 0.3 | — | — | — | |

## Аксон
CIN_NAK 1.0 + K2P 8.125 + NALCN 0.088 + **Cav2.1 (140) 100** + Ca-buffer 0.5 + PMCA 0.3.

## Сверка (было → стало)
| Было | Стало |
|---|---|
| IONOTROPIC_NAK 0.05 («только d→c drive») | CIN_NAK 1.0 (Sautois) |
| K2P 2.0 | K2P 8.125 |
| PUMP 0.2 | PUMP 1.0 |
| SK 1.8 | SK 6.0 |
| KIR 0.4 | NALCN 0.088 |
| KV42 0.5, KNA 0.15, KV_DR 0.3, PIR_TCA 0.2 | убрано |
| 1 дендрит | без дендритов |

## Статус
✅ синхронизировано с `cpg_config.h` `cIN_soma_mods()` / `cIN_axon_mods()` (2026-08-31).
