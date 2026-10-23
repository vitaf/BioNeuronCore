# NEURON LC_NEURON — норадренергические нейроны (головастик, аналог locus coeruleus)

Справочник модификаторного состава. Тип: **нейромодуляторный** (норадреналин, volume transmission).

## Биология
- **Тонический пейсмейкер/burster** — тоническая активность 1–3 Гц, ритмические локальные потенциалы ~1 Гц.
- Высвобождает **норадреналин** → α1 на dIN (Gq→↑Ca²⁺), α2 на cIN (Gi/o→GIRK). Arousal/внимание.
- **Пейсмейкер LC зависит от Ca²⁺-каналов**: согласованное действие **L- и T-type Ca²⁺** каналов (concerted action).
- **SK-каналы** — ключевой модулятор частоты пейсмейкера LC (в моделях болезни Паркинсона — ускорение пейсмейкера + нарушение AHP при SK-дисфункции; активация SK предотвращает).
- **α2 авторецептор** (соматодендритный, Gi/o) — блокада ритма LC/LFP при 100–250 нМ, снижение Ca²⁺-базовой линии.

## Модификаторный состав (ПОЛНЫЙ — литература)
| Мод. | Белок/ток | Роль |
|---|---|---|
| IONOTROPIC_NAK (0) | fast Na⁺ | фронт спайка |
| KV_DR (123) | delayed rectifier K⁺ | реполяризация |
| **CAV13 (1)** | L-type Ca²⁺ (Cav1.3) | пейсмейкерный Ca²⁺-вход |
| **PIR_TTYPE_CALCIUM (96)** | T-type Ca²⁺ (Cav3.x) | низкопороговый Ca²⁺-вход |
| **SK_CHANNEL (103)** | SK (KCa2.x) | AHP, модулятор частоты |
| HCN (I_H) | HCN2 (130) | «sag» (менее критичен, чем Ca/SK) |
| K2P_LEAK (107) | leak | покой |
| PUMP_NAK (7) | Na/K-АТФаза | гомеостаз |

> **α2 авторецептор** — Gi/o-метаботропный (↓Ca²⁺, GIRK). На соме.

## Текущий код (сверка)
```cpp
// neuron_library.h: LC_NEURON() — НЕПОЛНЫЙ
IONOTROPIC_NAK(0.8), PUMP_NAK(0.4), K2P_LEAK(0.04), HCN2(0.2)
```
| Аспект | Статус |
|---|---|
| Na⁺/K⁺/leak/HCN2 | ✅ есть |
| **L-type Ca (Cav1.3)** | ❌ ОТСУТСТВУЕТ — пейсмейкер LC зависит от L+T Ca |
| **T-type Ca (Cav3)** | ❌ ОТСУТСТВУЕТ |
| **SK** | ❌ ОТСУТСТВУЕТ — ключевой модулятор частоты |
| **α2 авторецептор** | ❌ ОТСУТСТВУЕТ |

## Литература
- **A concerted action of L- and T-type Ca²⁺ channels regulates locus coeruleus pacemaking** (Frontiers in Synaptic Neuroscience 2019, Volgushev M. ed.).
- **Enhanced firing of locus coeruleus neurons and SK channel dysfunction in prodromal Parkinson's disease** — SK-активация предотвращает ускорение пейсмейкера LC.
- **Autocrine Neuromodulation and Network Activity Patterns in the Locus Coeruleus of Newborn Rat Slices** — α2-агонисты блокируют LC-ритм (100–250 нМ).

## Литературные плотности (для калибровки — этап 6)
- **L-type (Cav1.3)**: γ ≈ 3–4 пС. **T-type (Cav3.x)**: γ ≈ 8 пС. SK: γ ≈ 10 пС.
- **NaP (TTX-нечувствительный персистентный Na)** — важный кандидат пейсмейкера LC (Tuckwell 2015: «sustained by a TTX-insensitive persistent sodium current», De Carvalho 2000; Alvarez 2002). Механизм пейсмейкера LC спорный: NaP ИЛИ согласованное действие L+T Ca²⁺ + SK (Frontiers 2019).
- ⚠️ Точные G (мС/см²) для LC — из статей LC-пейсмейкера (De Carvalho 2000, Sanchez-Padilla 2014, Frontiers 2019). Числовые таблицы не в открытом доступе — извлечь на этапе 6.

## Статус
❌ **НЕПОЛНЫЙ** — отсутствуют L-type Ca, T-type Ca, SK (ключевые для пейсмейкера LC), α2. Модификаторы MOD_CAV13, MOD_PIR_TTYPE_CALCIUM, MOD_SK_CHANNEL реализованы — добавить в состав.

## Дополнение (2026-09-08) — синаптические рецепторы (ингибиторные входы)

| Рецептор | Направление | Источник |
|---|---|---|
| GABAA | ингибирование (prepositus hypoglossi, gigantocellularis) | Ennis & Aston-Jones 1989 |
| glycine | глицинергические входы | LC-глицинергическое ингибирование |

- **G (плотность):** НЕ публикована → маркер «этап 6». Только `addReceptor` (без addMod) — конвенция головастиковых.
- **Применено:** `addReceptor(GABAA + GLYCINE)`.
