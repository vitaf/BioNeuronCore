# CA_L_TYPE (84) — L-type Ca²⁺ (Cav1.2/Cav1.3)

## Источники
Cav1.2 (L-type) — voltage-gated Ca²⁺ канал, HVA.
- Single-channel **γ = 20–30 пС** (unitary, L-type).
- Активация V½ = **−11.0 мВ** (2 мМ Ca²⁺, k=6.9).
- Инактивация V½ = **−37.3 мВ** (voltage-dependent).
- Ca²⁺-зависимая инактивация (CDI) — calmodulin.

## Биологические свойства Cav1.2

| Свойство | Значение |
|---|---|
| Single-channel γ | 20–30 пС (≈25) |
| Активация V½ | −11 мВ (HVA) |
| Инактивация V½ | −37.3 мВ (VDI) |
| CDI | да (calmodulin, Ca²⁺-зависимая) |

## Наша реализация (ca_l_type_current, hh_biological.cpp + mod_84)

```cpp
Vhalf=−25 (state[3]), k=5 (state[4]), g_Ca=1.0 (state[2]);
m-гейт: Boltzmann V½=−25, tau=3;
h-гейт: VDI V½=−35,k=8 × CDI (Ca/1 µM), tau=50;
P_Ca = g_Ca * density * GHK_P_PER_CM_S;  open = m²·h;  z=2.
```

## Сверка

| Параметр | Cav1.2 | Наш | Статус |
|---|---|---|---|
| Single-channel γ | 20–30 пС | g_Ca=1.0 (безразм.) | ❌ магия |
| Активация V½ | −11 мВ | −25 мВ | ⚠️ |
| Инактивация V½ | −37.3 мВ | −35 мВ | ≈ ✅ |
| CDI | да | да (Ca/1 µM) | ✅ |
| P-шкала | — | GHK_P_PER_CM_S=1e-4 | ❌ магия |

## Статус: ✅ ДОСТОВЕРНО (исправлено 2026-08-14, согласовано; m²·h 2026-08-18)

1. g_Ca=1.0 → **γ=25 пС** (single-channel L-type).
2. V½ активации −25 → **−11 мВ**, k 5→6.9 (Cav1.2).
3. Инактивация V½ −35 → **−37.3 мВ** (Cav1.2).
4. Конверсия γ→P для **z=2**: `GHK_GAMMA_PS_TO_P_CM_S_DIV = RT/(4F²·c_ref) = 6.93e-18`.
5. Убрать GHK_P_PER_CM_S; Ca·1000 → MM_TO_UM (плагин).
6. Тест test_ca_l_type (γ неизменен, m-релаксация V½=−11, CDI) — зелёный.
7. density — чистая плотность каналов (калибруется в интеграции, этап 6).
8. Гейтинг m·h → **m²·h** (2026-08-18) — L-type, два гейта активации. Тест test_ca_l_type дополнен сверкой h-гейта V½=−37.3.

## Раздельная VDI/CDI (исправлено 2026-09-22, Фаза 0)
Было: единый h-гейт `h_inf = VDI(V)×CDI(Ca)` с одной τ_h=50 мс — смешивал два процесса.
Стало: два независимых гейта (полная биология):
- **h_VDI** (state[1]) — voltage-dependent, V½=−37.3 (k=8), τ_VDI=150 мс (быстрая VDI-компонента; Catterall 2005 ~190 мс).
- **h_CDI** (state[5]) — Ca²⁺-dependent, Kd=1 µM, τ_CDI=35 мс (CDI 32–41 мс, кальмодулин).
open = m²·h_VDI·h_CDI. Константы `CAV12_TAU_VDI_MS`/`CAV12_TAU_CDI_MS`/`CAV12_CDI_KD_UM` в modifier_helpers.h (single-source: fallback + плагин mod_84).
