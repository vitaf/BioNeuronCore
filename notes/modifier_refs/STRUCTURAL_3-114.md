# Структурные / generic модификаторы — типы 3, 4, 5, 109, 111, 112, 114

Аудит #14 (последняя партия модификаторов). Дата: 2026-08-21. Метод: [[modifier-audit-method]].

## Оценка

| ID | Модификатор | Статус | Механизм |
|----|-------------|--------|----------|
| 4 | Myelin | ✅ честно | структурный, return 0, блок ионотропных через dispatch |
| 114 | Connexin Cx36 | ✅ честно | структурный, коннексон-синтез/деградация, return 0 |
| 111 | Thermal noise | ✅ честно | физ.фактор Johnson-Nyquist, xorshift32 (осознанное исключение) |
| 109 | Glial Kir4.1 | ✅ честно | GHK + инвард-ректификация + K_o-зависимость |
| 112 | Kv7 M-current | ✅ честно | GHK + PKC-модуляция (PIP2) |
| 3 | Metabotropic | ❌ → GHK | generic GPCR: Ом-GIRK → GHK K⁺ |
| 5 | Synaptic release | ❌ → GHK | Ca²⁺-сенсор + facilitation/depression: Ом-SK → GHK K⁺ |

## Диагноз

1. **Metabotropic (3)**: generic GPCR-шаблон. Суммарный NT → occupancy → GIRK. Ток был
   Ом (`-g_GIRK·(V−E_K)`); переведён на GHK K⁺ (как gi_o_ghk_effect).
2. **Synaptic release (5)**: Ca²⁺-сенсор (Hill n=2, K_d=0.5µM) → facilitation/depression →
   K⁺ ток (SK-подобный, AHP). Был Ом (`-g_SK·(V−E_K)`); переведён на GHK.

## Границы

Меняется ТОЛЬКО тело функции + плагины + тесты. Структуры, `state[]`, `#pragma pack`,
BION, диспетчеры — НЕ трогаются. ID сохраняются.
