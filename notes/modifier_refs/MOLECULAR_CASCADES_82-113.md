# Молекулярные каскады (сигнальные ферменты) — типы 82, 83, 86-92, 113

Аудит #12. Дата: 2026-08-21. Метод: [[modifier-audit-method]].

## Природа категории

Это СИГНАЛЬНЫЕ ферменты (киназы/фосфатазы/синтазы/протеазы) — не ионные токи.
`return 0`, обновляют `state[]` (фракция активного фермента 0..1). Вход — Ca²⁺, ATP,
кросс-модуляция (CaMKII/ERK/mTORC1/PKA/DAG). Выход — downstream-эффект (фосфорилирование,
синтез, деградация, transcription_signal).

**density = относительная концентрация фермента** (1 = reference). Масштабирует СКОРОСТЬ
каскада: `dt/τ → dt·density/τ` (enzyme_amount ≡ density; объём сегмента — общий множитель,
сокращается при нормировке). density=0 → каскад выключен, density=1 → reference.

## Литература и параметры

| ID | Фермент | Механизм | Kd / Hill | τ | Источник |
|----|---------|----------|-----------|-----|----------|
| 82 | CaMKII | Ca²⁺/CaM → автофосф. Thr286 | Hill n=4 (4 Ca²⁺-сайта CaM), Kd(CaM)≈1µM | сек–мин | Lisman 2012 |
| 83 | Calcineurin | PP2B, высокое сродство CaM | Hill n=2, Kd(CaM)≈0.05–0.5µM (ниже CaMKII) | мс–сек | Klee 1998 |
| 86 | eCB synthesis | Ca²⁺ + DAG (PLCβ→DAGLα) → 2-AG | EC50(Ca²⁺)≈0.5µM, Hill n=2 | сек | Kano 2009 |
| 87 | MAPK/ERK | Ca²⁺→Ras→Raf→MEK→ERK→pCREB | Hill n=3 (Ras) | ERK ~5 мин, ядро ~30 мин | Sweatt 2004 |
| 88 | IKK/NF-κB | Ca²⁺→IKK→IκB↓→NF-κB→Zif268 | Kd(Ca²⁺)≈0.3µM | мин–час | Albensi 2007 |
| 89 | mTORC1 | двухключевой: AA(Rag/Rheb) + PI3K/AKT; ATP/AMPK↓ | Kd(AA)≈0.3 | мин | Saxton 2017 |
| 90 | Protein synth | mTORC1→S6K1 + 4E-BP→eIF4E | — | мин–час | Saxton 2017 |
| 91 | Cofilin | ERK→RSK→LIMK→pCofilin (инакт.)→F-actin | — | мин | Bamburg 1999 |
| 92 | MMP9 | ERK+NF-κB→MMP-9, TIMP↓, ECM↓ | — | час | Vafadari 2015 |
| 113 | DARPP-32/PP1 | PKA→p-DARPP-32(Thr34)→ингиб. PP1 | IC50(PP1)≈1nM | мин | Greengard 1999 |

## Диагноз нечестности

1. **DARPP-32 (113): `PKA_act = m->state[2]`** — суррогат. Должен читать честный пул
   `segment_pka_act(s)` (MOD_CAMP_PKA 135, создан в #9). state[2] никем не заполняется.
2. **eCB (86): синтез зависит только от Ca²⁺** — биологически 2-AG = PLCβ→DAG→DGLα.
   Должен включать `segment_dag(s)` (MOD_PLC_IP3_DAG 136, #10).
3. **mTORC1 (89): `AA = m->state[3]`** — аминокислотный сенсор читается из собственного
   state[3]. В модели нет среды AA — это внешний parametric input (нутриентная доступность).
   Задокументировать; `AKT = AA·0.8` — упрощение PI3K (инсулин/факторы роста ≡ нутриенты).
4. **CaMKII (82) / Calcineurin (83): мёртвый `act_rate`** (`(void)act_rate`) — удалить.
5. **Подгоночные множители** (0.8/0.9/0.7/0.5) — нормировки амплитуды каскада 0..1,
   задокументированы как mean-field (не физические константы).

## Решения (согласовано 2026-08-21)

1. **enzyme_amount (density)** — ввести: скорость каскада × density.
2. **Суррогаты → честные входы**: DARPP-32←segment_pka_act, eCB←segment_dag.
3. **mTORC1 AA** — оставить state[3] как parametric input (нутриенты), задокументировать.
4. **Мёртвый код** — убрать act_rate.
5. **Kd/Hill/τ** — сверить с литературой (выше).

## Границы

Меняется ТОЛЬКО тело функции + плагины + тесты. `Segment/Modifier`, `state[]` размер,
`#pragma pack`, BION, диспетчеры — НЕ трогаются. ID (82, 83, 86-92, 113) сохраняются.
