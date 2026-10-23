# MN_NAK (139) — MN/aIN Na/K repetitive firing (головастик Xenopus)

## Источник
Sautois B, Soffe SR, Li WC, Roberts A (2007). J Comput Neurosci 23:59–77. DOI 10.1007/s10827-006-0019-1.
PDF: `articles/Sautois2007_JComputNeurosci_type-specific-neuron-properties.pdf`.
Исходная кинетика: Dale N (1995). J Physiol 489:489–510 + 489:473–488.

## Модель Sautois (Ом-форма, C = 4 пФ)
```
I = g_leak(V−V_leak) + gNa·m³h(V−V_Na) + gKfast·n⁴(V−V_K) + gKslow·n⁴(V−V_K)
```
Repetitive firing (RF) — обычная h-инактивация (НЕ глубокая, в отличие от single-spike dIN).

### Проводимости (Table 2)
| Параметр | MN (nS) | MN (мС/см²) | aIN (nS) | aIN (мС/см²) |
|---|---|---|---|---|
| gLeak | 2.47 | 0.62 | 1.35 | 0.34 |
| gNa | 110 | 27.5 | 150 | 37.5 |
| gKfast | 8 | 2.0 | 15 | 3.75 |
| gKslow | 1 | 0.25 | 2.5 | 0.625 |
| VNa | +50 | — | +50 | — |

### Измеренные свойства (Table 1)
MN: V_rest −61, AP threshold −26, AP peak +26, width 0.9 мс, rise 105 мВ/мс — RF (repetitive firing).

### Гейтинг (Table 3, группа aIN/MN, HH α/β)
| Гейт | A (aIN/MN) | C | D (aIN/MN) | E (aIN/MN) |
|---|---|---|---|---|
| α m_Na | 8.67/**13.26** | 0.5 | −13.01/**−5.01** | −18.56/**−12.56** |
| β m_Na | 5.73 | 1 | −2.99/**5.01** | 9.69 |
| α h_Na | 0.04 | 0 | 15.8/**28.8** | 26 |
| β h_Na | 4.08/**2.04** | 0.001 | −19.09/**−9.09** | −10.21 |
| α n_Kfast | 3.1 | 1 | −35.5/**−27.5** | −9.3 |
| β n_Kfast | 1.1/**0.44** | 1 | 0.98/**8.98** | 16.19 |
| α n_Kslow | 0.2 | 1 | −10.96/**−2.96** | −7.74 |
| β n_Kslow | 0.05 | 1 | −22.07/**−14.07** | 6.1 |

**Жирным — значения MN** (использованы в реализации).

## Наша реализация
`mn_nak_current` (hh_biological.cpp) + `mod_139_mn_nak.cpp` (плагин) + `mn_nak_*` (modifier_helpers.h).
Параметры MN: gNa=27.5, gKfast=2, gKslow=0.25 мС/см². Na m³h + Kfast n⁴ + Kslow n⁴.

**Оговорка про aIN:** aIN и MN в одной группе Table 3, но с разной кинетикой (A_m 8.67 vs 13.26)
и плотностями (gNa 150 vs 110). Реализован один модификатор (параметры MN, примарный выходной
нейрон). aIN в текущем CPG отключён (A2C_N=0, A2D_N=0), отдельный A_IN_NAK — при необходимости.

## Статус: ✅ ДОСТОВЕРНО (добавлено 2026-08-25)
- Тест `test_mn_nak_sautois` (5 тестов, 12 ассертов): m_inf/h_inf/проводимости/τ/релаксация.
- MN: rest −57.8 (цель −61), одиночный спайк peak +36.8 (цель +26), БЕЗ overshoot (был +150 у squid HH).
- parity 61/61, consolidation 44/44.

Задача: заменить кальмарный NaK_HH (тип 0) у MN на тадпольный MN-канал; убрать спонтанный
overshoot +150 (маскировал реальную alternation).
