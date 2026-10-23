# NaP-tadpole (195) — персистентный Na⁺ dIN (Nav1.2-persistent, головастик)

## Источник
Svensson E, Jeffreys H, Li W-C (2017). *The modulation of two motor behaviors by persistent
sodium currents in Xenopus laevis tadpoles*. J Neurophysiol 118:121–130.
PMID 28331009, PMC5494356.

Ключевой факт (вольт-клэмп dIN/RB/non-dIN):
> «The INaP in all neuron groups **started to be activated at voltage steps to −40 mV**,
> which is below the threshold for the action potential… peaking at around 0 mV.»
> «The activation of INaP will require stepping the membrane potential above −40 mV.»

riluzole (1 µM, INaP-блокатор) уменьшает rebound spikes в dIN — прямое доказательство,
что NaP = мост ребаунд-каскада (не меняя амплитуду спайка/R_in).

## Почему НЕ Nav1.6 (105)
NaP (105) взят из Mes V нейронов (Wu 2005): persistent-активация V½ = −58.7 мВ (акт. −76, пик −48).
dIN (Xenopus) экспрессирует **Nav1.2** (не Nav1.6, Rush 2005 / REST-NRSF), и его INaP
активируется **деполяризованнее** (−40 мВ). Отдельный изоформ — [[shared-modifiers-sensitive]].

## Кинетика (реализация)
| Параметр | Значение | Источник |
|---|---|---|
| Активация m | Boltzmann **V½ = −25 мВ**, k = 6 | Svensson 2017 (акт. −40, пик 0) |
| Инактивация | **медленная (slow) s** | Fleidervish & Gutnick 1996, Crill 1996 |
| slow V½ / k / τ | −50 мВ / 6 / 500 мс (⚠️ ОЦЕНКА) | уточнить по литературе |
| Po (persistent-фракция) | 0.016 | гиппокамп (100 мс) |
| γ | 20.8 пС | Ben-Shalom 2017 (fluct. analysis, Nav1.2 WT 20.8±1.8 пС, n=9) |
| τ_m | 5 мс (фикс) | подпороговый, слабо V-зависим |

V½=−25 выведено из «активируется выше −40, пик ~0» (threshold −40 + I-V пик 0, k=6).
Точный V½ из PMC5494356 — в полном тексте (TODO вытащить).

## Сверка (стационар)
| V (мВ) | m_inf (NaP-tadpole) | m_inf (NaP-105 Nav1.6) |
|---|---|---|
| −51 (покой dIN) | 0.013 (закрыт) | 0.783 (открыт!) |
| −40 | 0.076 | 0.958 |
| −25 | 0.5 | ~0.99 |
| −10 | 0.924 | ~1.0 |
| 0 (пик) | 0.985 | ~1.0 |

**Смысл:** NaP-tadpole ЗАКРЫТ на покое −51 (как в биологии — «активируется выше −40»),
а Nav1.6 NaP-105 был ошибочно открыт на 78% → давал плоский деполяризующий ток на покое.

## Роль в пейсмейкере
NaP-tadpole = регенеративный мост (положительная обратная связь): активируется выше −40,
где снимается Mg²⁺-блок NMDA → NMDA+NaP+Ih вместе деполяризуют dIN до порога → спайк.
Без NaP-tadpole dIN «зависает» на −51 (NaP-105 плоский, NMDA Mg²⁺-блокирован).

## Статус: ✅ ДОСТОВЕРНО (создан 2026-09-18; одномолекулярный рефакторинг 2026-09-20, согласовано)
- Применён к dIN (cpg_config.h) и dlc_MHR (neuron_library.h), плотность **10.7/µm²** — из Svensson 2017
  INaP 0.21 пА/µm² через одномолекулярную GHK: `density = 0.21/(P_single·flux·m·Po)`, P_single=γ·RT/(z²F²·c̄_Na).
- Рефакторинг (2026-09-20): суррогат `GHK_GAMMA_PS_TO_P_CM_S` (c_ref=150 мМ) → `ghk_single_channel_P(γ,1,c̄_Na)`;
  ток Na ×2.68 (= c_ref/c̄_Na = 150/55.96) до литерального 0.21 пА/µm². γ 20.7→20.8 (Ben-Shalom 2017).
- Тест test_nap_tadpole: 9 assertions (активация V½, persistent, динамика m, I-V) — зелёный.
- **Slow inactivation добавлена (2026-09-30)**: прежнее «без h» держало dIN деполяризованным → single-spike ломался
  (NaP=10.6 → 2 спайка). Добавлена slow s (state[6], V½_s=−50, τ_s=500 мс, open=m·Po·s) →
  NaP=10.6 → 1 спайк (single-spike сохранён). ⚠️ V½_s/τ_s — ОЦЕНКА (уточнить Fleidervish & Gutnick 1996).
- Nav1.6 NaP-105 остаётся для млекопитающих (отдельный модификатор, не тронут).

## Связанные
- [[NaP_105]], [[NAV12_142]], [[DIN_NAK_125]], [[dIN_Ih_rebound]]
- Память: [[gating-fix-2026-09-18]], [[bath-nmda-mg-block]]
