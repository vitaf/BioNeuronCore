# MOD_KV21 — Kv2.1 (KCNB1) медленный delayed rectifier K⁺ (I_Ks)

**Тип:** MOD_KV21 (196) — новая изоформа (Фаза 2, single-spike dIN).

## Роль
Медленный TEA-чувствительный K⁺-ток (I_Ks) — удерживает single-spike firing в
головастиковых спинальных нейронах (Winlove & Roberts 2011/2012). Высокопороговый
sKDR, медленная активация + медленная U-type инактивация.

## Литература
- **Alexander et al. 2017**, *Br J Pharmacol* (IUPHAR/BPS) — V½_акт=0.0±1.9 мВ (k=9.4±0.5),
  τ_акт=15.7±1.6 мс @+60 мВ, V½_инакт=−26.5±1.6 мВ (k=5.2), деактивация τ₁=15.3/τ₂=73.1 мс @−40.
- **PMC2151531** (Control of single channel conductance) — γ зависит от [K⁺]₀:
  g_out ≈ 5.93 пС (3 мМ K⁺), 6.82 пС (5 мМ), 7.90 пС (10 мМ), g_in ≈ 11.4 пС (60 мМ) → **γ≈8 пС** (физ. градиент).
- **IUPHAR/BPS** — TEA поро-блокатор, **IC50=10 мМ** (pIC50=2, rat Kv2.1).
- **Winlove & Roberts 2011** *J Neurophysiol* 105:2487 — I_Ks = медленный TEA-чувствительный (single-spike RB).
- **Winlove & Roberts 2012** *Eur J Neurosci* 36:2926 — I_Ks предотвращает repetitive; I_Kf (4-AP) = repetitive.

## Молекулярные константы
| Параметр | Значение | Источник |
|---|---|---|
| γ | **8 пС** (физ. K⁺ 6–8) | PMC2151531 |
| V½ активации | **0 мВ** | Alexander 2017 |
| k активации | **9.4** | Alexander 2017 |
| τ_m(V) | **15 + 58/(1+exp(V/10))** мс (≈15 мс @+60, ≈73 мс @−40) | Alexander 2017 |
| V½ инактивации | **−26.5 мВ** (k=5.2) | Alexander 2017 |
| τ_h | **1000 мс** (медленная U-type) | — |
| TEA IC50 | **10 мМ** | IUPHAR/BPS |
| Гейтинг | m⁴·h | delayed rectifier |

## Модель
- **Активация m⁴** (4 гейта), V½=0, k=9.4, τ_m(V) = 15 + 58/(1+exp(V/10)) — сигмоида
  (быстрая ~15 мс при деполяр., медленная ~73 мс при гиперполяр. — деактивация).
- **Инактивация h (U-type, полная)**:
  `h_inf(V) = 1 − (1−h_min)·left·right`, где
  `left = 1/(1+exp(−(V+26.5)/5.2))` (инактивация), `right = 1/(1+exp((V−15)/10))` (восстановление).
  `τ_h(V) = 4000 + (300−4000)/(1+exp((V+80)/15))` мс (развитие ~4 с, recovery ~0.3 с).
  Форма: h(−100)≈1, h(−26.5)≈0.6, h(0)≈0.35 (min), h(+80)≈1 (open-state resistant).
  ⚠️ **V½_right=+15 / k_right=10 — ОЦЕНКА** (Klemic: «max при ~0 мВ, open-state resistant», без числа);
  h_min=0.2 из Gunay 2008 (ModelDB 267669 h0). Помечено в коде.
- **TEA-блок**: `open /= (1 + TEA/10 мМ)`, TEA через state[5].
- **GHK K⁺** (z=1): I = γ·N·Po·flux, Po = m⁴·h.

## Применение
dIN single-spike (Kslow I_Ks в DIN_NAK). Плотность (этап 6): G=0.75 мС/см² (Sautois gKs)
→ density = G·10·(c̄_ion/c̄_eff)/γ.

## Связанные
[[modifier-audit-method]], [[neuron-density-po]], [[NEURON_dIN]], [[DIN_NAK_125]], [[KV_DR_123]]
