# NALCN (133) — Sodium Leak Channel, Non-Selective

## Источники
1. Lu B, Su Y, Das S, Liu J, Xia J, Ren D (2007). *The neuronal channel NALCN contributes resting
   sodium permeability and is required for normal respiratory rhythm*. Cell 129(2):371–383.
   PMID 17448995. DOI 10.1016/j.cell.2007.04.005.
2. Elementary mechanisms underlying activation of leak sodium channels (2022). Biophys J
   (single-channel ~27 пС, E_rev ~5.7 мВ, FAM155A/UNC79/UNC80).

## Биологические свойства NALCN (Lu 2007)

| Свойство | Значение |
|---|---|
| Тип | Nonselective cation leak channel (Na⁺/K⁺/Ca²⁺) |
| Voltage-dependence | **НЕТ** (voltage-independent, всегда открыт) |
| Инактивация | **НЕТ** (noninactivating) |
| Фармакология | TTX-резистентен (10 µM), Cs⁺-резистентен |
| Относительные P | **P_Na : P_K : P_Cs : P_Ca = 1.3 : 1.2 : 1.0 : 0.5** (WT) |
| EEKA-мутант | P_Na = P_K = 1.3, P_Ca = 0.1 |
| I-V | линейный |
| I при −80 мВ | −573 ± 124 пА (HEK293) |

## Единичный канал (2022, cell-attached)

| Параметр | Значение |
|---|---|
| Single-channel conductance γ | **~27 пС** |
| Reversal potential | **~5.7 мВ** (близко к 0, nonselective) |

## Наша реализация (nalcn_current, hh_biological.cpp + mod_133)

```cpp
P_max = 0.05 (state[0]);  P_base = P_max * density * GHK_P_PER_CM_S;
// P_Na : P_K : P_Ca = 0.42 : 0.42 : 0.16  (нормировано из «1.3:1.3:0.5»)
I_Na = ghk(P_base*0.42, z=1, ...); I_K = ghk(P_base*0.42, ...); I_Ca = ghk(P_base*0.16, z=2, ...);
```

## Сверка

| Параметр | Lu 2007 / 2022 | Наш | Статус |
|---|---|---|---|
| Voltage-independence | да | open=1.0 всегда | ✅ |
| Noninactivating | да | нет h-гейта | ✅ |
| Nonselective (Na/K/Ca) | да | GHK Na+K+Ca | ✅ |
| P_Na : P_K | **1.3 : 1.2** (P_Na > P_K) | 0.42 : 0.42 (равны) | ⚠️ мелкое |
| P_Ca | 0.5 | 0.16 (нормировано 0.5/3.0) | ✅ (нормировка) |
| P_max (абс.) | НЕТ прямого (γ=27 пС — единичный канал) | 0.05 (мембранная) | ⚠️ калибровка плотности |

## Статус: ✅ ДОСТОВЕРНО (исправлено 2026-08-14, согласовано)

1. P_Na:P_K:P_Ca исправлено 0.42:0.42:0.16 → 0.433:0.400:0.167 (Lu WT 1.3:1.2:0.5).
2. Объём плагина — single-source segment_volume (было инлайн /20).
3. **Честная P-шкала**: P_max=0.05 (магия) заменена на **γ=27 пС** (single-channel, Lu/2022).
   **Одномолекулярный рефакторинг (2026-09-21):** γ разложена на парциальные по
   chord-conductance (w_i = f_i·z_i²·c̄_i/Σ, f=1.3:1.2:0.5): γ_Na=16.15, γ_K=10.80,
   γ_Ca=0.057 пС → P_i = ghk_single_channel_P(γ_i, z_i, c̄_i) [м³/с]. Суррогат
   `GHK_GAMMA_PS_TO_P_CM_S` (c_ref=150) УБРАН; ток по-ионно I_i = P_i·flux_i·(density·area).
   Литеральной плотности NALCN нет (b провалился) → density — клеточный уровень (этап 6).
4. Тест test_nalcn (voltage-independent, state[] не меняется, I-V монотонна, noninactivating) — зелёный.
5. `density` — честная плотность каналов (per-нейрон), калибруется в интеграционном тесте
   нейрона (этап 6): сейчас density=1.0 (1 канал) — вклад NALCN занижен, V_rest dIN гиперполяризован.
