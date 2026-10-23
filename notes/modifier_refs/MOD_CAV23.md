# MOD_CAV23 — Cav2.3 (CACNA1E) R-type Ca²⁺-канал

**Тип:** MOD_CAV23 — новая изоформа (кортикальные дендритные Ca-спайки, dCaAP).

## Литература
- **γ ≈ 14 пС** (Randall & Tsien 1995, J Neurosci 15:2995-3012).
- HVA R-type (резистентный к органическим блокаторам), промежуточный между L- и P/Q-типом.
- Локализация: **дистальный дендрит** (Ca²⁺-спайки; dCaAP L2/3 — Gidon 2020 Science, [PMID 31896716](https://pubmed.ncbi.nlm.nih.gov/31896716/)).
- Вклад в дендритное плато с NMDAR (R-type + NMDAR).

## Молекулярные константы
- γ ≈ 14 пС; V½ ≈ −20…−30 мВ (HVA, нужна точная лит-ра); Po ~0.2 (peak).
- GHK Ca²⁺ (z=2).

## Двухкомпонентная инактивация (исправлено 2026-09-22, Фаза 0)
Было: единый h-гейт τ_h=100 мс (slow). Стало: полная двухкомпонентная fast + slow:
- **h_fast** (state[1]) — τ=25 мс (fast const, Bernatchez 2001), fractional ~0.65 (WT).
- **h_slow** (state[2]) — τ=100 мс (WT τ_slow ~90–105 мс), fractional ~0.35.
- h_eff = 0.65·h_fast + 0.35·h_slow; open = m²·h_eff (CDI слабая поверх).
V½_инакт=−60 мВ (k=6) — оставлена (в диапазоне Metz 2005 −60.9 / oocytes −71; литература противоречива −17.9…−71).
Константы `CAV23_TAU_H_FAST_MS`/`CAV23_TAU_H_SLOW_MS`/`CAV23_FAST_WEIGHT` в modifier_helpers.h (single-source).

## Применение
Пирамидные (L2/3 dCaAP, L5, CA1) — дистальные дендриты.

## Связанные
[[modifier-audit-method]], [[neuron-density-po]], [[NEURON_HUMAN_cortex_hippocampus]]
