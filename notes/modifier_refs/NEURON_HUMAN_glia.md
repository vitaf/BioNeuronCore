# NEURON_HUMAN — Глия (аудит, группа 12)

**Источник:** агент (WebSearch).

## Макроглия ЦНС
- **Astrocyte Proto/Fibrous** — Kir4.1 (KCNJ10), AQP4, GLAST/GLT-1, Na/K-АТФаза α2, Cx43. RMP −80..−90. ⚠️ в коде нет AQP4 и GLAST.
- **Oligodendrocyte** — Kir4.1, Cx47/Cx32. Миелин да. ✅.
- **NG2_Polydendrocyte** — Na_v, K_v (K_A/K_DR), Ca-проницаемый AMPA + GABA_A; спайков нет. ⚠️ нет Na_v/K_v (есть Kir4.1+насос).
- **Müller_Glia** — Kir4.1 (>90% проводимости, RMP→−85), AQP4, Kir2.1, GLAST.
- **Bergmann_Glia** — GLAST/GLT-1 (высочайшая плотность), Kir4.1, AMPA.

## Микроглия
- **Microglia** — **Kir2.1** (главный RMP), Kv1.3, TRPM7, K2P. RMP −40..−60. ⚠️ **в коде Kir4.1 — ОШИБКА, нужен Kir2.1**; RMP −70 → −40..−60.

## Глия ПНС
- **SchwannCell** — Kir, Na/K-АТФаза. Миелин да.
- **SatelliteGlia_DRG** — Kir4.1, Cx43. RMP −80.
- **EntericGlia** — Kir4.1, Cx43.

## Не-глия (спорная категория)
- **Pericyte** — mural-клетка ГЭБ; каналы бедны; RMP −40..−60. ⚠️ помечена "Glia".
- **ChoroidPlexusEpithelium** — Na/K-АТФаза, NKCC1, AQP1; RMP −40..−60.
- **MeningealCell** — фибробласт; RMP −40.
- **EndothelialCell_CNS** — ГЭБ; GLUT1, P-gp; RMP −30..−50.
- **VascularSmoothMuscle_CNS** — Cav1.2 (L-type), BK, K_v. ✅ Cav1.3 ≈ верно.
- **Pinealocyte** — L-type Ca; RMP −50.

## Ключевые расхождения
1. Microglia: Kir4.1 → **Kir2.1**; RMP −70 → −40..−60.
2. Астроциты: отсутствуют AQP4 и GLAST/GLT-1.
3. NG2: не хватает Na_v/K_v.
4. Pericyte/Meningeal/Endothelial/VSMC/Pinealocyte — не глия (перекатегоризация).

## Источники
- Djukic 2007 J Neurosci 27:11354 — Kir4.1 KO (PMID 17942742).
- Olsen & Sontheimer 2008 Glia 56:1549 — Kir4.1 глия.
- Bergles 2000 Nature 405:187 — глутаматные синапсы OPC.
- Kofuji 2000 J Neurosci 20:5733 — Kir4.1 KO Мюллера.
- Nagelhus 2004 Glia 47:33 — Kir4.1/AQP4 эндофуты.

---

## Дополнение: литературные плотности γ/Po/G (агент, 2026-09-02)

**Формула пересчёта (как в коде):** `density [мкм⁻²] = G [мСм/см²] · 10 / (γ [пСм] · Po)`.

### Одноканальная проводимость γ и Po (литература)
| Канал | γ (пСм) | Po | Источник |
|---|---|---|---|
| **Kir4.1** (KCNJ10, гомомер) | **25 ± 3** (входящий) | ~0.5–0.8 @ покой −80 | J Neurosci 1997 17:7725 (Мюллер) |
| Kir4.1/Kir5.1 (гетеромер) | 40–60 | — | Physiol Rev 2009; KCNJ16 |
| Kir2.1 (KCNJ2) | 30 | V-зависимый (падает при депол.) | Kir_106.md |
| Kir2.2 (KCNJ12) | ~30–40 | V-зависимый | — (перициты) |
| TREK-1 (KCNK2, K2P) | 100 (113–120 в астроцитах) | **0.012** покой; 0.06–0.26 акт. (AA/pH/давление) | K2P_107.md; PMID 24402080 |
| TWIK-1 (KCNK1, K2P) | ~34 | leak | Zhou 2009 J Neurosci |
| Kv1.3 (KCNA3) | ~13 (13–17) | V-зависимый (Po→1 @ +40) | Grissmer 1990 (T-лимфоциты) |
| Kv1.2 / Kv1.5 | ~17 / ~8 | V-зависимый | — (VSMC) |
| BK (KCa1.1) | 250–300 | Ca²⁺-зав. | — |
| Cav1.2 (L-type) | ~25 (Ba²⁺) | низкий (0.05–0.1) | — |
| Cav3.1 (T-type) | 9.5 | 0.1–0.3 | Biophys J 2008 |
| TRPM7 | 40–105 | — | микроглия |
| P2X4 / P2X7 | ~9 | АТФ-зав. | — |
| Cx43 (gap junction) | ~100 | — | — |
| AQP4 | 0 (вода, НЕ ионный) | — | Nagelhus 2004 |
| GLT-1/GLAST (EAAT2/1) | транспортер (оборот 10–40 / 16 с⁻¹) | — | Zerangue & Kavanaugh 1996 |

### Таблица «тип → канал → γ/Po/G → density → локализация → источник»
| Тип | Канал | γ (пСм) | Po | G (мСм/см²) | density (мкм⁻²) | Локализация | Источник |
|---|---|---|---|---|---|---|---|
| Astrocyte_Protoplasmic | **Kir4.1** | 25 | ~0.6 | **0.175** (зрелый) | **~0.12** | периваск. эндофуты + сома | Djukic 2007; Olsen & Sontheimer 2008 |
| Astrocyte_Protoplasmic | **TREK-1/TWIK-1** (K2P) | 113–120/34 | 0.012 покой; 0.06–0.26 акт. | осн. passive (≥80% гиппокамп) — G (мСм/см²) **нужна лит-ра** | нужна лит-ра | диффузно | Zhou 2009 J Neurosci 29:8551; PMID 24402080 |
| Astrocyte_Protoplasmic | AQP4 | вода | — | — | — | эндофуты (ортогональные массивы) | Nagelhus 2004 |
| Astrocyte_Protoplasmic | GLAST/GLT-1 (EAAT1/2) | оборот 16/10–40 с⁻¹ | — | транспортер (НЕ G) | **GLAST 2300 / GLT-1 8500 мкм⁻²** (CA1 radiatum) | перисинаптические отростки | Lehre & Danbolt 1998 PMID 9786982; GLAST_147.md |
| Astrocyte_Protoplasmic | Na/K-АТФаза α2 | I_max 2 µA/см² | — | 0.002 | нужна лит-ра | эндофуты | GLIAL_NAK_PUMP_110.md |
| Astrocyte_Protoplasmic | Cx43 | ~100 | — | — | — | gap-junction синцитий | — |
| Astrocyte_Fibrous | Kir4.1 + TREK-1 + AQP4 + Cx43 | 25/100 | — | как proto | — | белое вещество | — |
| Oligodendrocyte | Kir4.1 | 25 | ~0.6 | нужна лит-ра (KO → R×5, ΔVm +25 мВ) | — | тело + проксим. отростки (НЕ компактный миелин) | Neusch; Larson 2016 |
| Oligodendrocyte | Cx47/Cx32 (gap) | — | — | — | — | миелиновые петли | — |
| Oligodendrocyte | **МИЕЛИН**: g-ratio ~0.6, интернод ~100×D | — | — | — | — | аксон (интерноды) | Hildebrand 1993; MYELIN_4.md |
| Microglia | **Kir2.1** (главный RMP) | 30 | V-зав. (→1 @ гиперпол.) | **0.27 (M2/IL-4); 0.12 (M0); 0.08 (LPS/M1)** | 0.09 (M2) | тело | Nguyen 2017; Glia 2017 PMID 27696527 |
| Microglia | **Kv1.3** (актив.) | ~13 | V-зав. (Po→1 @ +40) | **0.24–0.48 (M1)**; 0.04–0.14 (in vivo LPS) | 0.18–0.37 | тело | Nguyen 2017; Glia 2017 PMID 27696527 |
| Microglia | TRPM7, P2X4/P2X7, THIK-1 (K2P) | 40–105 / 9 | — | нужна лит-ра | — | тело/процессы | — |
| NG2_Polydendrocyte | Na_v (Nav1.2) | ~20 | малый (window @ −40) | **≈0.2 (peak, из 9.5–15.7 пА/пФ)** | — | тело/процессы | PMID 17981402 (Neurosci 2007); Larson 2016 |
| NG2_Polydendrocyte | Kv1.3/Kv1.5/Kv1.6 (K_DR) + Kv1.4/Kv4 (K_A) | ~13 | V-зав. | **1.6 (K_DR @ +80, из 268 пА/пФ)**; K_A — 70% клеток (G нет) | — | диффузно | Larson 2016; A2A/CGS21680 (OPC) |
| NG2_Polydendrocyte | Ca²⁺-прониц. AMPA (GluA2⁻) + GABA_A | (рецептор) | — | нужна лит-ра | — | синаптические | Bergles 2000 Nature 405:187 |
| Müller_Glia | **Kir4.1** | 25 | ~0.6 | **≈0.5 (целоклет., R_in 25 МΩ → G 40 нС / area ~8000 мкм²)**; >90% в эндофутах | — | **эндофуты (94% конд.)** + тело | Kofuji 2000; J Neurosci 1997 17:7725 |
| Müller_Glia | AQP4 | вода | — | — | — | эндофуты (с Kir4.1, DGC) | Nagelhus 2004 |
| Müller_Glia | Kir2.1 + GLAST | 30 | — | — | — | тело | — |
| Bergmann_Glia | **GLAST/EAAT1** | оборот 16 с⁻¹ | — | **~4700 мкм⁻²** (макс. плотность!) | 4700 | перисинапс. отростки Purkinje-шипиков | Lehre & Danbolt 1992 |
| Bergmann_Glia | Kir4.1 | 25 | ~0.6 | нужна лит-ра | — | тело/волокна | — |
| Bergmann_Glia | AMPA (GluA1/GluA4) | (рецептор) | — | нужна лит-ра | — | волокна | — |
| SchwannCell | Kir + Na/K-АТФаза | 25 | — | нужна лит-ра | — | тело | — |
| SchwannCell | **МИЕЛИН**: g-ratio PNS ~0.63, интернод ~100×D | — | — | — | — | 1 аксон (миелиниз.) | Hildebrand 1993 |
| SatelliteGlia_DRG | Kir4.1 | 25 | ~0.6 | нужна лит-ра | — | тело (обёртка сомы нейрона) | Vit 2008; Tang 2010 |
| SatelliteGlia_DRG | Cx43 | ~100 | — | — | — | gap-junction | — |
| EntericGlia | Kir4.1 + Cx43 + P2Y4 (ATP) | 25 | — | нужна лит-ра | — | ганглии ЖКТ | — |
| Pericyte | **Kir2.2** (Kcnj12, НЕ Kir2.1) | ~30 | V-зав. | нужна лит-ра | — | тело | Physiol 2025 (Harraz) |
| Pericyte | K_ATP + Kv1 + BK (малый) + TRPC3 (ensheathing) + Cav1.2 | — | — | — | — | тело/отростки | Harraz 2025; TRPC3 PMC12434803 |
| ChoroidPlexusEpithelium | Na/K-АТФаза (апикально) + NKCC1 (апикально) + AQP1 (апикально) | — | — | — | — | апикальная (CSF) мембрана | Damkier 2013 Physiol Rev |
| MeningealCell | фибробласт (каналы бедны) | — | — | — | — | оболочки | — |
| EndothelialCell_CNS | Kir2.1 + GLUT1 + P-gp + Cx40/Cx43 + TRPV4 | 30 | — | — | — | ГЭБ (люминальная/аблюминальная) | Physiol 2025 |
| VascularSmoothMuscle_CNS | **Cav1.2** (L-type) | 25 | низкий | нужна лит-ра | — | мембрана (сокращение) | Albarwani 2003 |
| VascularSmoothMuscle_CNS | **BK** (KCa1.1) | 250–300 | Ca²⁺-зав. | нужна лит-ра | — | мембрана (STOC) | — |
| VascularSmoothMuscle_CNS | **Kv1.2/Kv1.5** (resting Em) | ~17/8 | V-зав. | нужна лит-ра | — | мембрана | Albarwani 2003 J Physiol |
| Pinealocyte | **L-type Ca (α1D/Cav1.3, Cav1.4 mRNA)** | 25 | низкий | нужна лит-ра | — | тело | Chik 1997 J Neurochem |
| Pinealocyte | **BK** (Ca²⁺-актив. K, CTX-чувств.) | 250–300 | Ca²⁺-зав. | нужна лит-ра | — | тело | Freschi & Parfitt 1986 |
| Pinealocyte | T-type Cav3.1 (NE ↑) | 9.5 | 0.1–0.3 | нужна лит-ра | — | тело | Cell/NE T-type (Biophys J) |

### Площади и пассивные параметры (типовые, нужна точная морфометрия)
| Тип | Сома | RMP | R_in | Примечание |
|---|---|---|---|---|
| Astrocyte (proto) | d 7–10 мкм → сома ~150–300 мкм²; домен до ~8·10⁴ мкм² | **−80…−90** | <5 МОм (пассивная) | passive: линейная I–V |
| Astrocyte (fibrous) | белое вещество | −80…−90 | <5 МОм | — |
| Oligodendrocyte | d 8–12 мкм | ~−80 | — | до ~50 миелин. сегментов |
| Microglia | d 7–10 мкм | **−40…−60** | несколько ГОм (трудно мерить) | рамфицированная |
| NG2/OPC | d 8–12 мкм | ~−70…−80 (близко E_K) | высокое | нет спайков |
| Müller | сома d ~8 мкм, клетка тянется через всю сетчатку (l ~100–200 мкм, area ~6–10·10³ мкм²) | **−85** | 25 ± 3 МОм (→G_total ~40 нС) | endfoot = 94% проводимости |
| Bergmann | сома в слое Пуркинье, радиальные волокна к pia | −80…−90 | — | GLAST 4700 мкм⁻² |
| Schwann | сома + обёртка 1 аксона | ~−40…−70 | — | миелин/немиелин |
| Satellite_DRG | обёртка сомы DRG-нейрона | −80 | — | — |
| Pericyte | тонкие отростки на капилляре | −40…−60 | высокое | НЕ глия |
| VSMC_CNS | веретено, артериолы | **−60…−40** | — | НЕ глия |
| Pinealocyte | d ~10 мкм | **−43** | — | НЕ глия |

### Ключевые уточнения (к первоначальному аудиту)
1. **K2P (TREK-1/TWIK-1) — главный passive-ток астроцитов** (≥80% в гиппокампе), а НЕ только Kir4.1. Kir4.1 = ~48% passive в спинальных астроцитах. Оба нужны.
2. **Kir4.1 γ = 25 ± 3 пСм** (гомомер); гетеромер Kir4.1/Kir5.1 = 40–60 пСм (в коде держать 25, если гомомер).
3. **Microglia Kir2.1** подтверждён (γ=30 пСм; Newell & Schlichter 2005). Kv1.3 — маркер активации (M1 30–60 пА/пФ).
4. **Bergmann GLAST ≈ 4700 мкм⁻²** — конкретная плотность для GLAST_147.md (макс. в мозге, 6× GLT-1).
5. **Pericyte = Kir2.2 (Kcnj12)**, НЕ Kir2.1; K_ATP — главный. Перекатегоризация «не глия» подтверждена.
6. **VSMC RMP −60…−40** (не глия); Cav1.2/BK/Kv1.2-1.5 подтверждены.
7. **Pinealocyte RMP −43 мВ**; L-type α1D/Cav1.3 (+Cav1.4 mRNA) + BK + T-type (NE↑).

### Новые источники
- J Neurosci 1997 17:7725 — Kir4.1 γ=25 пСм в Мюллер-клетках (Kofuji).
- Zhou 2009 J Neurosci 29:8551 — TWIK-1/TREK-1 = passive conductance астроцитов.
- Newell & Schlichter 2005 J Physiol — микроглия K⁺/Cl⁻, Vm.
- Nguyen 2017 / Glia 2017 65:71 — Kv1.3/KCa3.1/Kir2.1 микроглии (M1/M2), плотности пА/пФ (PMID 27696527).
- Lehre & Danbolt 1998 J Neurosci 18:8751 — GLAST/GLT-1 мкм⁻² (CA1: GLAST 2300/GLT-1 8500; Bergmann GLAST 4700) (PMID 9786982).
- PMID 17981402 — Na-ток NG2 гиппокампа (9.5–15.7 пА/пФ, TTX IC₅₀ 39.3 нМ).
- PMID 24402080 — TREK-1 в астроцитах (γ 113–120 пС, Po 0.06–0.26 акт.).
- Lehre & Danbolt 1992 — GLAST 4700 мкм⁻² Bergmann (первич.).
- Hildebrand 1993 — g-ratio/интернод.
- Harraz 2025 Physiol Rev — перициты/эндотелий капилляров.
- Damkier 2013 Physiol Rev — choroid plexus транспортёры.
- Albarwani 2003 J Physiol — Kv VSMC церебральных артерий.
- Chik 1997 J Neurochem; Freschi & Parfitt 1986 — пинеалоциты.

---

## НЕ НАЙДЕНО / НЕ СООТВЕТСТВУЕТ — для этапа 6 (калибровка)

### НЕ СООТВЕТСТВУЕТ (исправлено 2026-09-03)
- **Microglia**: Kir4.1 → **Kir2.1** (ошибка); RMP −70 → **−50** (−40..−60); +Kv1.3 (M1-маркер).
- **Astrocyte proto/fibrous**: добавлены TREK-1 (главный passive ≥80%) + GLAST/GLT-1.
- **NG2**: добавлены Na_v (Nav1.2) + K_v (Kv1.3) — ток есть, спайков нет.
- **Bergmann/Müller**: добавлен GLAST (Bergmann 4700 мкм⁻² — макс., Lehre & Danbolt 1992).

### НАЙДЕНО (G заполнены, 2026-09-05)
- **Microglia Kir2.1**: G = 0.27 (M2/IL-4) / 0.12 (M0) / 0.08 (LPS/M1) мСм/см² — пересчёт из Kir-плотности 9.4/4.2/2.8 пА/пФ @ −120 мВ (E_K ≈ −85). PMID 27696527.
- **Microglia Kv1.3**: G = 0.24–0.48 мСм/см² (M1, из 30–60 пА/пФ @ +40 мВ); in vivo LPS 5.06→17.58 пА/пФ. PMID 27696527.
- **NG2 Na_v**: G ≈ 0.2 мСм/см² (peak, из 9.5–15.7 пА/пФ, TTX IC₅₀ 39.3 нМ). PMID 17981402.
- **NG2 K_DR**: G ≈ 1.6 мСм/см² (из 268 ± 45 пА/пФ @ +80 мВ, культура OPC); K_A — в ~70% клеток (G нет).
- **GLAST/GLT-1** (транспортер, НЕ G-проводимость): плотность молекул **GLAST 2300 / GLT-1 8500 мкм⁻²** (CA1 radiatum); **Bergmann GLAST 4700 мкм⁻²** (молекулярный слой). Lehre & Danbolt 1998 PMID 9786982.
- **Müller Kir4.1**: G ≈ 0.5 мСм/см² (целоклет., R_in 25 МΩ → G_total 40 нС при area ~8000 мкм²); >90% в эндофутах. Kofuji 2000.
- **TREK-1 γ уточнён**: 113–120 пС в астроцитах (не 100); Po покой 0.012, акт. 0.06–0.26. PMID 24402080.

### НЕ НАЙДЕНО (точные G отсутствуют — `нужна лит-ра`)
- **Astrocyte TREK-1/TWIK-1** — G (мСм/см²) не публикуется отдельно (passive = сумма K2P + Kir4.1; Zhou 2009: quinine −58%; Frontiers 2016 KO: «нет вклада» — спор).
- **Oligodendrocyte Kir4.1** G — есть только KO-эффект (R×5, ΔVm +25 мВ), не мСм/см².
- **NG2 K_A** (Kv1.4/Kv4) — G нет (только «~70% клеток экспрессируют»).
- **SchwannCell Kir / SatelliteGlia_DRG Kir4.1 / EntericGlia Kir4.1** — G нет (только качественно).
- **Pericyte Kir2.2 / VSMC Cav1.2·BK·Kv1.2-1.5 / Pinealocyte L·BK·T** — G нет (категория «не глия»; VSMC — только число каналов 3060–3470/миоцит).
- **Kir4.1 гетеромер Kir4.1/Kir5.1** (γ 40–60) — в коде гомомер 25 пС (упрощение).

### Упрощения модификаторов (не идеально, помечено)
- **Kv1.1 KDR** — один m-гейт вместо тетрамерной кооперативности (n⁴).
- **Kv1.3** — V½ точная не найдена (использую −30 как Kv1.x).
- **TREK-1** — без механо-/термо-чувствительности (статический Po=0.012).
- **AQP4** — водный канал (НЕ ионный) не моделируется в NeuronBlueprint → задача #33 (осмос).

### Глия без градиентов
Глиальные клетки не поляризованы (сома/отростки диффузные) → `gradient=0` для всех глиальных каналов. Исключение — Bergmann GLAST в перисинаптических отростках (компартмент, не градиент).
