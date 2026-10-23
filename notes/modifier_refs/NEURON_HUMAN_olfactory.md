# NEURON_HUMAN — Обоняние (аудит, группа 6)

**Источник:** агент (WebSearch).

- **OlfactoryReceptorNeuron_Human (ORN)** — глутамат ✅. Биполярный: дендрит knob+цилии → аксон в гломерул. Каналы: **CNG (cAMP)**, **CaCC TMEM16B/anoctamin-2**, Nav (TTX), T-Ca. RMP −52..−90. ⚠️ нет CNG, нет CaCC, нет дендрита (dendrite_count=0), нет T-Ca.
- **Mitral** — глутамат ✅. Апикальный дендрит → 1 гломерул, 6–8 латеральных; аксон LOT миелин ✅. Каналы: Nav, **Kv1.3**, **HCN (дистальный апикальный дендрит)**, INaP, KNa/Slack. ⚠️ нет Kv1.3/HCN/INaP; латеральных дендритов 4 (надо 6–8); Cav1.3 в апикале вместо HCN.
- **Tufted** — глутамат ✅. **INaP (интринсивный bursting θ 1–8 Гц)**, HCN1 (низкий), Kv1.3. ⚠️ нет INaP/HCN, bursting не смоделирован.
- **Periglomerular** — GABA+DA ✅. **HCN1 (~4× больше tufted)**, Nav, Kv3.1. ⚠️ нет HCN1/Kv3.1.
- **OlfactoryGranule** — GABA ✅. Без аксона; Nav-спайки в дендритах (Pressler 2019), VGCC. ⚠️ Nav только на соме, нет VGCC.
- **OlfactoryEnsheathingGlia** — глия ✅. Kir4.1 только в мукозальных OEC; voltage-gated K; Cx43. ✅ модель корректна для мукозального типа.

## Главные пропуски
ORN: CNG/CaCC; bulbar: HCN/Kv1.3/INaP (локализация Ih в апикальном дендрите mitral).

## Источники
- Billig 2011 Nat Neurosci (PMID 21516098) — TMEM16B ORN.
- Sci Rep srep00050 (PMID 22355572) — Ih mitral.
- Fadool 2011 PLoS One (PMID 21966386) — Kv1.3 mitral.
- Hayar 2004 J Neurosci — ET INaP bursting.
- Holderith 2003 Eur J Neurosci (PMID 12887416) — HCN1 PG 4× ET.
- Pressler 2019 J Neurosci (PMID 31662426) — granule дендритные спайки.
- Isaacson & Strowbridge 1998 Neuron (PMID 9581766) — reciprocal дендродендритные.
- Smith 2020 J Neurosci Res (PMID 31797433) — OEC Kir4.1 mucosal-only.

---

## Литературные плотности γ/Po/G (дополнено 2026-09-02)

**Формула:** `density = G·10/γ` (G мС/см², γ пС); Po/гейтинг — в open_prob, НЕ в density.

### Mitral (человек)
**RMP** ~ −60…−65 мВ (человеческие значения — нужна лит-ра); **R_in** ~50–100 МОм (грызун, нужна сверка).

| Канал | γ (пС) | Po | G (мС/см²) | Локализация (градиент) | Источник |
|---|---|---|---|---|---|
| HCN (Ih) | 1 (HCN2 ~1.5) | V-зависимый | нужна лит-ра (только пА: дист. туфт 26.5±23.3 пА vs сома 3.5±4.9 пА; V½=−88, P_Na/P_K=0.30) | апикальный дендрит (популяционная разнородность, НЕ чёткий градиент) | Angelo & Margrie 2011 Sci Rep 1:50 |
| INaP (Nav1.6 persist.) | 20.7 | **0.016** | нужна лит-ра | сома | — |
| Kv1.3 (KCNA3) | **14–24** (14 остеокласт / 24 PM) | гейт | нужна лит-ра (>80% наружного K⁺-тока; мС/см² не дано) | сома/дендрит | Fadool 2011, [PMID 21966386](https://pubmed.ncbi.nlm.nih.gov/21966386/) |
| Nav fast | 20 (Nav1.x) | гейт m³h | сома/аксон ↑ (числ. G не дано) | сома/аксон | Bhalla & Bower 1993 |
| KDR / KCa (BK γ=250) | 12 / 250 | — | нужна лит-ра (числ. G не дано) | сома | Bhalla & Bower 1993 |

> **Bhalla & Bower 1993** (пространственное распределение, НЕ плотности): Ca²⁺ + медленный K
> сосредоточены в **гломерулярном туфте**; Na + быстрый K — **сома/аксон**; KA и KCa — у сомы.

> ⚠️ **«HCN градиент в апикальный дендрит»:** Angelo 2011 даёт **популяционную** разнородность
> sag (ZD7288-чувствительный Ih, активация <−75 мВ), а НЕ количественный проксимально-дистальный
> градиент. HCN1-иммунозолото: **в mitral не детектируется**, в external tufted — умеренно.
> Численный градиент плотности Ih — **«нужна лит-ра»** (не выдумывать).

### Tufted (external tufted, ET)
**INaP + Ih + T-Ca + BK** → интринсивный bursting 1–8 Гц (Liu & Shipley 2008 J Neurosci; Hayar 2004).
γ: INaP 20.7 (Po 0.016), HCN1 1, T-Ca 7.2–9, BK 250. G — «нужна лит-ра».

### Periglomerular (PG)
GABA+DA. **HCN1 ~4× больше ET** (Holderith 2003); Kv3.1 (γ — нужна лит-ра). G — «нужна лит-ра».

### OlfactoryGranule
GABA, без аксона; Nav-спайки в дендритах (Pressler 2019), VGCC (Cav). γ из системных (Nav 20, Cav 25); G — «нужна лит-ра».

### OlfactoryReceptorNeuron (ORN)
CNG (γ=29 пС гомомер CNGA2 / 12–15 пС гетеромер CNGA2/A4/B1b — [JBC](https://www.jbc.org/article/S0021-9258(20)80083-0/), Qu 2000), CaCC/TMEM16B (γ=2 пС — [[CACC_146]]), Nav (TTX), T-Ca (γ=9). RMP −52…−90. G — «нужна лит-ра».

**Площади (типичные, грызун; человеческие — нужна лит-ра):** mitral сома r≈10–15 мкм; апикальный дендрит l≈300–700 мкм, r=1–2 мкм (2πrl ~ 1900–8800 мкм²); 6–8 латеральных дендритов l≈500–1000 мкм. ET/PG сома r≈6–10 мкм.

## Найденные плотности (проверка 2026-09-03)
- **Kv1.3 (mitral)**: 60–80% наружного K⁺-тока; общий наружный ток ~5.2 нА при +40 мВ (mitral) / ~3.7 нА (tufted). Точного G (мС/см²) нет → density-плейсхолдер 0.3, калибровка этап 6. Источники: [PLoS One 2011 (Fadool)](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0024921), [PMC2737549](https://pmc.ncbi.nlm.nih.gov/articles/PMC2737549/).
- **CNG (ORN цилии)**: ~8 каналов/µm² (цилиарная мембрана) / ~1000/µm² (dendritic knob) / ~57/µm² (сома); 700–2300 молекул/µm длины цилии; пик cAMP ~100 µМ. G ~10–1300 мС/см² → density-плейсхолдер 0.5, этап 6. Источники: [Flannery 2006 Biophys J](https://www.cell.com/biophysj/fulltext/S0006-3495(06)71718-0), [PMC1479050](https://www.ncbi.xyz/pmc/articles/PMC1479050/).

## Дополнение (2026-09-08) — численные G найдены (ModelDB, сохранено локально)

Выкачано в `analiz/modeldb_gbar/` (git-бэкап): `mitral_bhalla1993` (ModelDB 2487), `mitral_rubincleland2006` (64296), `externaltufted_viertel2019` (240364), `cav33_nrt_destexhe1996` (17663).

| Тип | Канал | G (мС/см²) | Источник | density (применено) |
|---|---|---|---|---|
| Mitral | Na fast | 153 (сома); 33/22.6 (втор.дендр.) | Bhalla & Bower 1993 (mit4.hoc gnafast=0.1532) | IONOTROPIC_NAK 1.2 (уже ≈) |
| Mitral | Kv1.3/Kfast | 196 → **452** (пересчёт 5.2 нА, C_m=62 пФ) | Fadool 2011 (5.2 нА @+40мВ, 60–80% Kv1.3) | **238 (γ=19)** [🟢 измер.ток] |
| Mitral | KDR/Kslow | 2.8 (сома); 20 (туфт) | Bhalla & Bower 1993 | — |
| Mitral | BK (kca3) | 14.2 (сома) | Bhalla & Bower 1993 (gkbar_kca3=0.0142) | — |
| Mitral | INaP | 0.42 (дендрит) | Rubin & Cleland 2006 (gbar_NaP=0.00042) | **12.7 (применено 0.15→12.7)** |
| Mitral | HCN2/Ih | 2.4 (дендрит) | Rubin & Cleland 2006 (gkhbar_Ih=0.0024) | **15.6 (γ=1.54, HCN2 — HCN1 не детектируется)** |
| Tufted (ET) | INaP | 8.32 → **0.396** (÷21, C=21 µF/см²) | Viertel & Borisyuk 2019 (ModelDB 240364) | **12 (γ=20.7/Po=0.016)** |
| Tufted (ET) | HCN1 | 20 → **0.952** (÷21, C=21) | Viertel & Borisyuk 2019 (gH) | **11.3 (γ=0.84)** |
| Tufted (ET) | T-Ca | 15 → **0.714** (÷21, C=21) | Viertel & Borisyuk 2019 (gLVA) | **1.0 (γ=7.2, добавлен в состав)** |
| PG | HCN1 | ~0.1 (0.101 нС сома) | Arruda & Roque 2013 PLoS One (Table 1) | **1.2 (γ=0.84)** |
| PG | Kv3.1 | НЕ в модели | Arruda & Roque 2013 (модель без Kv3.1) | нужна лит (оставлен 0.6) |
| TAN | Cav2.2 | НЕ НАЙДЕНО | Codianni & Rubin 2023 (ModelDB недоступен) | нужна лит (оставлен 0.3) |
| **Cav3.3 (nRT)** | T-type | **0.8** (0.2–0.8) | Destexhe 1996 ModelDB 17663 (gcabar_it2=0.0008) | **0.73 (применено)** |

## Дополнение (2026-09-08, агент) — ORN/Granule/OEG (следующий шаг)

| Тип | Канал | G (мС/см²) | Источник | density (применено) |
|---|---|---|---|---|
| ORN | CNG (cAMP) | ~1.04 (3.64 нС цилии) | Dougherty 2009 (ModelDB 136097) | **0.8 (γ=13)** |
| ORN | CaCC/ANO2 (TMEM16B) | ~0.24 (0.83 нС) | Dougherty 2009 | **1.2 (γ=2)** |
| ORN | Nav (TTX), T-Ca | НЕ НАЙДЕНО | — | нужна лит |
| Granule | Na (сома/дендрит) | 161 / 135.5 | Bhalla & Bower 1993 | **80.5 / 67.75 (γ=20)** |
| Granule | KA (A-type) | 8.8 | Bhalla & Bower 1993 (gkbar_kA=0.0088) | **17.6 (γ=5, добавлен)** |
| Granule | Kslow / KM | 131.3 / 133.4 | Bhalla & Bower 1993 | нужна лит (γ неточные + G завышены) |
| Granule | VGCC (Cav) | НЕ НАЙДЕНО | — | нужна лит |
| OEC | Kir4.1 / VGK / Cx43 | НЕ НАЙДЕНО (только качественно) | Smith 2020 (PMID 31797433) | нужна лит |

Сохранено локально: `analiz/modeldb_gbar/{orn_dougherty2009, olfactorybulb_davison2003, olfactorygranule_simoes2014, olfactorygranule_onajodar2017}/`.

## Надёжность источника G (градация, продублирована в комментариях neuron_library.h)
- 🟢 **измеренный ток** — пересчёт из whole-cell пА/пФ/нС: Mitral Kv1.3 (5.2 нА), ORN CNG/CaCC (3.64/0.83 нС).
- 🟡 **полная морфологическая модель** — G распределены по компартментам, верифицированы: Mitral HCN2/INaP (Rubin 2006), Granule Na/A-type (Bhalla 1993).
- 🔴 **reduced/однокомпартментная** — G подогнаны, не измерены: Tufted INaP/HCN/T-Ca (Viertel 2019, ÷21 C_m), PG HCN1 (Arruda 2013).

- ⚠️ Все G из ModelDB — **Ohm** (S/см²), перевод в GHK density по `density = G·10/(γ·Po)` (для гейтированных — `G·10/γ`).
- PG (HCN1/Kv3.1) и Cav2.2 (TAN) — численного G нет, нужен полный текст (Arruda & Roque 2009; Yan & Surmeier 1996).
