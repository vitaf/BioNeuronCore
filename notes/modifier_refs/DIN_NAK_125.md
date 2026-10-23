# DIN_NAK (125) — dIN Na/K single-spike (головастик Xenopus)

## Источник
Sautois B, Soffe SR, Li WC, Roberts A (2007). *Role of type-specific neuron properties in a spinal
cord motor network*. J Comput Neurosci 23:59–77. DOI 10.1007/s10827-006-0019-1.
PDF: `articles/Sautois2007_JComputNeurosci_type-specific-neuron-properties.pdf`.

## Модель Sautois (Ом-форма, C = 4 пФ)

```
I = g_leak(V−V_leak) + gNa·m³h(V−V_Na) + gKfast·n⁴(V−V_K) + gKslow·n⁴(V−V_K)
```

### Проводимости (Table 2, dIN)
| Параметр | Значение (nS) | Плотность (мС/см², area=400µm²) |
|---|---|---|
| C | 4.00 пФ | — |
| gLeak | 3.68 | 0.92 |
| VLeak | −51 мВ | — |
| gNa | 210 | 52.5 |
| VNa | +50 мВ | — |
| gKfast | 0.5 | 0.125 |
| gKslow | 3.0 | 0.75 |
| VK | −80 мВ | — |

### Гейтинг (HH α/β, форма α = (A + B·V)/(C + exp((D+V)/E)))
| Гейт | A | B | C | D | E |
|---|---|---|---|---|---|
| α m_Na | 13.01 | 0 | 4 | −1.01 | −12.56 |
| β m_Na | 5.73 | 0 | 1 | 9.01 | 9.69 |
| α h_Na | 0.06 | 0 | 0 | 30.88 | 26 |
| β h_Na | 3.06 | 0 | 1 | −7.09 | −10.21 |
| α n_Kfast | 3.1 | 0 | 1 | −31.5 | −9.3 |
| β n_Kfast | 0.44 | 0 | 1 | 4.98 | 16.19 |
| α n_Kslow | 0.2 | 0 | 1 | −6.96 | −7.74 |
| β n_Kslow | 0.05 | 0 | 2 | −18.07 | 6.1 |

### Firing (Table 1)
V_rest = −51 мВ, R_in = 272 MΩ, AP threshold −28 мВ, AP peak +29 мВ, width 1.9 мс,
rise 85 / fall 76 мВ/мс, AHP trough −43 мВ, **single AP**.

---

## Наша реализация (din_nak_current, hh_biological.cpp + modifier_helpers.h)

| Параметр | Sautois | Наш | Расхождение |
|---|---|---|---|
| gNa | 52.5 мС/см² | 10 мС/см² (0.01 С/см²) | **×5.25 меньше** |
| K-каналы | fast (0.125) + slow (0.75) | один gK=0.05 | **структурное** (1 vs 2) |
| m-гейт | HH α/β | Boltzmann V½=−30,k=7.2 | форма |
| h-гейт | HH α/β (h_inf(−28)=0.36) | Boltzmann V½=−50,k=4 (h_inf(−28)=0.004) | **глубокая инактивация** |
| n-гейт | HH α/β ×2 | Boltzmann V½=−10,k=10 (один) | форма |
| tau | τ=1/(α+β) (V-зависимые) | фикс. tau_m=0.1,h=1.0,n=2.0 | **V-независимые** |
| V_leak | −51 | −70 (разбито на K2P/NALCN/NaP) | утечка разбита |

## Статус: ✅ ДОСТОВЕРНО (исправлено 2026-08-14; одномолекулярный рефакторинг 2026-09-21, согласовано)

Применено: (1) gNa 10→52.5 мС/см²; (2) один K → gKfast+gKslow; (3) гейтинг Boltzmann →
HH α/β с τ(V)=1/(α+β). Тест test_din_nak_sautois (параметрический + динамический) зелёный.

**Одномолекулярный рефакторинг (2026-09-21):** γ_Na 20→20.8 пС (Nav1.2 Ben-Shalom 2017).
Суррогат `GHK_GAMMA_PS_TO_P_CM_S` + `DENSITY=G·10/γ` заменены на `P_single=γ·RT/(z²F²·c̄)`
и chord-conductance плотность `density=G·10·c̄_ion/(γ·c̄_eff)`: Na 26.25→37.64, Kf 0.25→0.692,
Ks 0.625→1.730 (c̄_eff Na=37.5, K=14.64 мМ). Ток Na ×1.43 (=c̄_ion/c̄_eff против Ohm ×10).

Утечка (gLeak 0.92 мС/см², V_leak −51) — НЕ входит в DIN_NAK; это K2P/NALCN/NaP
(отдельные модификаторы, аудируются своими задачами).
