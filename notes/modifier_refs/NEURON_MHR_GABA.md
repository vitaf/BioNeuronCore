# NEURON_MHR_GABA — ГАБКергический ретикулоспинальный тормозной нейрон (головастик)

**Файл кода:** `tadpole_types.h` (MHR_GABA=28) + `tadpole_develop.cpp`
**Статус:** справочник (аудит, задача 27)

## Литературный прототип
GABAergic mid-hindbrain reticulospinal (MHR) нейроны — тормозят плавание/escape.
- **Perrins, Walford & Roberts 2002** (J Neurosci 22:4229, PMID 12019340): GABAergic MHR
  тормозят swimming; активны при плавании → подавляют escape-рефлекс.
- Van Rheede 2015: тектальный выход глутаматергический (OFF-клетки), GABA — MHR + внутритектальное.

## Нейротрансмиттер
GABA (`nt_released=1`) — тормозной.

## Состав (GABAergic тормозной ретикулоспинальный, аналог aIN)
- Na (IONOTROPIC_NAK) — спайк.
- Kv (KV_DELAYED_RECTIFIER) — реполяризация.
- K2P (утечка) — покой.
- SK (AHP) — тормозной интернейрон (post-burst AHP).
- Na/K-АТФаза (PUMP).

## Плотности
- «нужна литература G» — density = G·10/(γ·Po) на этапе 6.
- Стартовые: как aIN (GABAergic тормозной).

## Локализация
- Ретикулоспинальная (mid-hindbrain, вентральный ствол). Нисходящая проекция на AVOID (GABA) + ствол.

## Связанные
[[modifier-audit-method]], [[neuron-density-po]], [[NEURON_aIN]]
