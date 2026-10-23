# NEURON ORN — обонятельный рецепторный нейрон (головастик Xenopus)

Справочник модификаторного состава + плотностей. Тип: **спайковый** (единственный в партии сенсорных трансдукторов).

## Источник (литература)
- **Li & Lin (2018)** PNAS «Ca²⁺-activated Cl current predominates in threshold response of mouse olfactory receptor neurons» — **CaCC (ANO2/TMEM16B)** даёт **~80%** порогового ответа; без него порог ×7.
- **Fyke & Belonio** «Shaping smell: chloride currents dictate olfactory transduction kinetics».
- **Обзор** (PMC4119848) — каскад: odorant → OR → Golf → AC3 → **cAMP↑ → CNG** (Na⁺/Ca²⁺ вход) → **Ca²⁺ → ANO2 (Cl⁻ выход, деполяризация)** → **Nav-спайки**.

## Биология
- Спайковый нейрон: cilia содержат **только CNG + CaCC**; спайковые каналы (Nav/Kv) — в дендрите/соме/аксоне.
- Трансдукция: CNG (Na/Ca вход) → CaCC (Cl выход, усиливает деполяризацию) → Nav-спайки.
- Частота спайков кодирует концентрацию одоранта (сигмоидально).

## Модификаторный состав сомы
| Канал | Мод. | Роль / источник |
|---|---|---|
| Olfactory GPCR (Golf→cAMP) | OLFACTORY_RECEPTOR 108 | трансдукция; PMC4119848 |
| **CaCC (ANO2/TMEM16B)** | **новый изоформа #8** | ~80% порогового ответа; Li & Lin 2018 |
| Nav (спайковый) | AXON_NAV16 141 / NAV12 142 | спайки (дендрит/сома/аксон) |
| Kv (реполяризация) | KV_DELAYED_RECTIFIER 123 | спад спайка |
| K-leak | K2P 107 | покой |
| Na/K-насос | PUMP_NAK 7 | гомеостаз |
| Ca-гомеостаз | CA_BUFFER 102 / PMCA 98 / NCX 99 | CNG Ca²⁺-приток + адаптация |

## Сверка (было → стало, tadpole_develop.cpp)
| Было (неверно) | Стало (выверено) |
|---|---|
| IONOTROPIC_NAK (HH кальмара) | AXON_NAV16/12 (спайковый Nav) + KV_DR |
| OLFACTORY_RECEPTOR | оставить (трансдукция) |
| — | + CaCC/ANO2 (#8) — ~80% ответа |
| — | + Ca-гомеостаз (CNG Ca²⁺-вход) |

## Статус
🟡 Состав выверен (Li & Lin 2018, PMC4119848). **Блокировано на #8** — нужен CaCC (ANO2/TMEM16B) как новая изоформа. Nav — использовать AXON_NAV16/12 (не HH кальмара).
