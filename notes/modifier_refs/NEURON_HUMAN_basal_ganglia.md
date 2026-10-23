# NEURON_HUMAN — Базальные ганглии (аудит, группа 3)

**Источник:** агент (WebSearch).

- **MSN_D1/D2** — GABA; средний шипиковый (~10⁴ шипиков). Kir2 (KCNJ2, down-state/RMP −85), **Kv4.2 (A-ток)**, KCNQ/Kv7 (M-ток), Cav1.3, SK, BK. Up/down-state. ⚠️ **нет Kv4.2 (A-ток) — канонический канал MSN**; шипики не смоделированы.
- **MSN_D1D2** — GABA; D1+D2 (~5% NAc shell). Как MSN.
- **Cholinergic_TAN** — ACh; тонический пейсмекер 2–10 Гц. **HCN + NaP + CaV2.2 (N-type)→SK**; Cav1.3→sAHP. ⚠️ **код CAV13 (L-type) для SK-связи вместо CaV2.2 (N-type)**.
- **FS_Striatal** — GABA (PV+); fast-spiking >200 Гц; Kv3.1/3.2, Kv4.2; gap junctions (Cx36). ✅.
- **LTS_Striatal** — GABA (SOM/NPY); T-type (Cav3), высокое R_in. ✅.
- **Calretinin_Striatal** — GABA; интернейрон-селективный; литературы мало.
- **GPe** — GABA; автономный пейсмекер; **HCN2 (доминантный)+HCN1 + Na** (Chan 2004); in vivo 40–60 Гц. ⚠️ код «50–80 Гц» завышено.
- **GPi** — GABA (PV+), выход БГ; HCN1–4, rebound. ⚠️ код идентичен GPe — биологически различаются.
- **SubthalamicNucleus** — глутамат ✓; **NaP + SK** (Bevan & Wilson 1999), T-type, HCN, Kv4.3. ✅.

## Главные расхождения
MSN нет Kv4.2; TAN N-type (не L) для SK; GPe частота; GPi состав HCN.

## Источники
- Bevan & Wilson 1999 J Neurosci 19:7617 (PMID 10460267).
- Chan 2004 J Neurosci 24:9921 (PMID 15525777) — HCN GPe.
- Surmeier 2007 Trends Neurosci.
- Shen 2007 Nat Neurosci — Kir2 MSN.
- Goldberg & Wilson 2005 J Neurosci — TAN CaV2.2–SK.
- Tepper 2010 Front Neuroanat 4:150.

---

## Дополнение: литературные плотности γ/Po/G (агент, 2026-09-02)

**Формула пересчёта (как в коде):** `density [мкм⁻²] = G [мСм/см²] · 10 / (γ [пСм] · Po)`.

### Одноканальная проводимость γ и Po (литература)
| Канал | γ (пСм) | Po | Примечание / источник |
|---|---|---|---|
| Nav1.6 | 20.7 ± 0.7 | пик 0.45; persistent 0.016 | Chatelier 2010 Pflügers Arch |
| Kir2.1 / Kir2.2 | ~30–33 / ~40 | ~1 (leak/выпрямл.) | 150 мМ K⁺, −100 мВ |
| Kv7/KCNQ (M) | нативный 7–11 (KCNQ2/3 ~9 CHO; KCNQ2 ~18 oocyte) | низкий, V-зав. | Selyanko 2001; Schwake 2000 |
| Kv3.1 | 27 | — | Grissmer 1992 |
| Kv4.2 (A) | ~4 (голая), ~7.5 (с DPP6) | — | J Neurosci 2009 |
| Cav1.3 (L) | ~18 (15–28 по Ba²⁺) | Po_max 0.2–0.23 | HEK/внутр. волоск. клетки |
| Cav2.2 (N) | 2.76 (2 мМ Ca²⁺); 16.4 (Ba²⁺) | — | физиол. Ca²⁺ |
| Cav3.1 (T) | 9.5 (Ca²⁺) | 0.1–0.3 | Biophys J 2008 |
| HCN1 / HCN2 | ~0.84 (рекомб. HCN1); нативный ~9.7 | V-зав. (V₁/₂ −81…−95) | bioRxiv 2024; Simeone 2005 |
| SK2 | 4–14 (≈11) | Ca²⁺-зав. | — |

### Таблица «тип → канал → γ/Po/G → density → локализация → источник»
| Тип | Канал | γ (пСм) | Po | G (мСм/см²) | density (мкм⁻²) | Локализация | Источник |
|---|---|---|---|---|---|---|---|
| MSN D1/D2 | Kir2.1/2.2 (down-state −85) | 30–40 | ~1 | **0.14** | ~0.04 | сома-дендрит | Wolf 2005 ModelDB 112834 (G_KIR=1.4e-4 S/см²); Shen 2007 |
| MSN D1/D2 | Kv4.2 (fast A = KAF) | ~7.5 | — | **225 (прокс); 20.6 (дист.) ⚠️ модельная точечная @35°C, НЕ применять напрямую** (сверка #45) | этап 6 | дендриты (дистальный ↓) | Wolf 2005 ModelDB 112834 (G_KAF=0.225/0.0206 S/см², кинетика Tkatch 2000) |
| MSN D1/D2 | Kv7/KCNQ (M-ток) | 7–11 | — | **нужна лит.** (в Wolf/Moyer M-ток отсутствует; Krp=1 мС/см²) | нужна лит. | перисоматич. | — |
| MSN D1/D2 | Cav1.3 (L) | ~18 | 0.2 | **P = 4.25e-7 см/с** (пермеабельность, не G) | этап 6 (GHK→G) | сома-дендрит/шипики | Wolf 2005 (P_CAL13); Surmeier 2007 |
| MSN D1/D2 | SK2 | ~11 | Ca²⁺-зав. | **145 ⚠️ модельная точечная @35°C, НЕ применять напрямую** (сверка #45) | этап 6 | сома | Wolf 2005 ModelDB 112834 |
| MSN D1/D2 | HCN2 | ~0.84 | — | **нужна лит.** (в Wolf 2005 HCN отсутствует) | нужна лит. | дендриты | — |
| Cholinergic_TAN | HCN2 (пейсмекер) | ~0.84 | — | **нужна лит.** | нужна лит. | сома-дендрит | Goldberg & Wilson 2005; Codianni & Rubin 2023 (есть табл. параметров) |
| Cholinergic_TAN | SK2 (AHP) | ~11 | Ca²⁺-зав. | **нужна лит.** | нужна лит. | сома | Goldberg & Wilson 2005 |
| Cholinergic_TAN | Kv7/KCNQ | 7–11 | — | **нужна лит.** | нужна лит. | сома | — |
| Cholinergic_TAN | Cav2.2 (N) → SK | 2.76 (физиол. Ca²⁺) | — | **нужна лит.** | нужна лит. | сома-дендрит | Goldberg & Wilson 2005 (γ=2.76 пС) |
| SubthalamicNucleus | Nav1.6 (resurgent) | 20.7 | 0.45 | 49 ⚠️ (Na в .mod — плейсхолдер 1e-7, реальное значение в дистрибуции) | ~53 | AIS/сома | Do & Bean 2003; Gillies & Willshaw 2006 |
| SubthalamicNucleus | Kdr (Kv2.1) | — | — | **3.84** ⚠️ (KDR.mod gk=3.84e-3 S/см²; ×Q10 1.6 = 6.1 при 36°C) | — | сома | Gillies & Willshaw 2006 (ModelDB 74298) |
| SubthalamicNucleus | Kv3.1 | 27 | — | **15** (Kv31.mod gk=0.015 S/см²) | ~5.6 | сома | Gillies & Willshaw 2006 (ModelDB 74298); Grissmer 1992 γ |
| SubthalamicNucleus | A-ток (Kv4.3) | ~7.5 | — | **нет в модели** ⚠️ (в STN-модели Kv4 A-ток отсутствует; быстрый K = Kv3.1) | — | — | Gillies & Willshaw 2006 |
| SubthalamicNucleus | Cav3.1 (T) | 9.5 | 0.1–0.3 | **1** ⚠️ (CaT.mod gcaT=0.001 S/см²) | ~3.5 | сома-дендрит | Gillies & Willshaw 2006 (ModelDB 74298) |
| SubthalamicNucleus | Cav2.2 (N) | 2.76 | — | **12** (HVA.mod gcaN=0.012 S/см²) | ~43 | дендриты | Gillies & Willshaw 2006 (ModelDB 74298) |
| SubthalamicNucleus | HCN2 | ~0.84 | — | **1** (Ih.mod gk=0.001 S/см²) | ~12 | дендриты | Gillies & Willshaw 2006 (ModelDB 74298) |
| SubthalamicNucleus | SK (AHP) | ~11 | Ca²⁺-зав. | **0.1** ⚠️ (sKCa.mod gk=1e-4 S/см²) | — | сома | Bevan & Wilson 1999; Gillies & Willshaw 2006 |
| GPe | Kv3.1 | 27 | — | **нужна лит.** (Gunay 2008 PAR_ROW не расшифрован) | нужна лит. | сома | Gunay 2008 ModelDB 114639 |
| GPe | HCN2 (>HCN1) | ~0.84 | — | **нужна лит.** (Gunay 2008) | нужна лит. | дендриты | Chan 2004; Gunay 2008 |
| GPe (упрощ. Terman/Rubin 2004) | gNa/gK/gCa/gT/gAHP/gLeak | — | — | 120/30/0.15/0.5/30/0.1 | — | (однокомп.) | Terman 2002; Rubin & Terman 2004 |
| GPi | Kv3.1 | 27 | — | **нужна лит.** | нужна лит. | сома | — |
| GPi | HCN1–4 (rebound) | ~0.84 | — | **нужна лит.** | нужна лит. | дендриты | — |

### Площади сегментов (2πrl) и пассивные параметры — типовые, нужна точная морфометрия
| Тип | Сома (d, area) | RMP | Порог | R_in |
|---|---|---|---|---|
| MSN D1/D2 | d 10–15 мкм → ~300–700 мкм²; шипик (post_area) ~0.5–1 мкм² ×10⁴ | down −85 мВ (Kir2), up −55…−45 мВ | ~−40 мВ | 50–150 МОм (down-state, высокое) |
| Cholinergic_TAN | d 20–30 мкм (крупные) → ~1250–2800 мкм² | −55…−60 мВ | ~−45 мВ | 100–200 МОм |
| SubthalamicNucleus | d 20–25 мкм → ~1250–2000 мкм² | −55…−60 мВ | ~−45 мВ | 100–200 МОм |
| GPe | d 20–30 мкм → ~1250–2800 мкм² | −55…−65 мВ | ~−50 мВ | 100–300 МОм |
| GPi | d 20–30 мкм | −55…−60 мВ | ~−50 мВ | 100–300 МОм |

**Локализация (градиенты):** Kir2 → сома-дендрит (задаёт down-state); Kv4.2 (A) → дендриты (дистальный ↑); Kv7/KCNQ → перисоматический; Cav1.3 → сома-дендрит/шипики; Cav2.2 → дендриты (у TAN сома-дендрит → SK); Cav3.1 → сома-дендрит; HCN2 → дендриты (пейсмекер); Kv3.1 → сома (fast-spiking); SK2 → сома; Nav1.6 → AIS/сома (resurgent).

**Новые источники:** Chatelier 2010 Pflügers Arch (Nav1.6 single-channel); Do & Bean 2003 J Neurophysiol (Nav1.6 resurgent STN, PMID 15056687); Gillies & Willshaw 2006 (STN G-значения); Terman 2002 / Rubin & Terman 2004 (GPe упрощ. G-значения); **Wolf et al. 2005 J Neurophysiol + ModelDB 112834 (база MSN, `nacb_msp/baseline_values.txt`); Moyer 2007 J Neurophysiol (расширение Wolf 2005)**; Selyanko 2001 J Physiol (KCNQ2/3 γ); Grissmer 1992 (Kv3.1 γ=27 пСм); Biophys J 2008 (Cav3.1 γ); Gunay 2008 J Neurosci 28:7476 + ModelDB 114639 (GPe); Codianni & Rubin 2023 Brain Struct Funct (TAN).

---

## РАСХОЖДЕНИЯ / ИСПРАВЛЕНИЯ (аудит 2026-09-05, сверка с .mod-файлами ModelDB)

1. **ModelDB 112834 = Wolf et al. 2005, НЕ Moyer 2007.** Moyer 2007 — надстройка (D1/D2-модуляция) над той же базой. Значения G взяты из `nacb_msp/baseline_values.txt` (Wolf 2005): G_KIR=1.4e-4, G_KAF=0.225/0.0206, G_KAS=0.0104/0.00095, G_KRP=0.001, G_SKKCA=0.145, G_BKKCA=0.001 S/см²; G_NAF=1.5 S/см² (сома) / 0.0195 (дендриты); Ca-каналы — пермеабельности (см/с), НЕ G.
2. **STN-значения из прошлой сессии НЕ совпадают с Gillies & Willshaw 2006 (.mod-файлы ModelDB 74298).** Фактические G: KDR (Kv2.1)=3.84, Kv3.1=15, Ih=1, sKCa=0.1, CaT=1, HVA(gcaL=2, gcaN=12) мС/см². Kv4 A-тока в модели НЕТ. Старые значения (Kdr=57, A=5, T=5, SK=1) — перепроверить источник (вероятно, из другой модели/бумаги).
3. **MSN Ca-каналы заданы пермеабельностью** (P_CAL=6.7e-6 Cav1.2, P_CAL13=4.25e-7 Cav1.3, P_CAN=1.0e-5 Cav2.2, P_CAQ=6.0e-6 Cav2.1, P_CAR=2.6e-5 Cav2.3, P_CAT=4e-7 Cav3, см/с). Для `density=G·10/(γ·Po)` нужен перевод P→G (GHK chord-conductance, см. скил).
4. **MSN «аномалия» РАЗРЕШЕНА (сверка #45, 2026-09-06):** G_KAF=225, G_SKKCA=145, G_NAF=1500 мС/см² — реальные значения `baseline_values.txt` (единицы S/см², чтение верное), но это МОДЕЛЬНЫЕ точечные плотности при 35°C (celsius=35, qfact=3) под специфичную морфологию MSP. Физиологические ориентиры: Kv4.2 A-ток ~1–10 мС/см² (Tkatch 2000), SK ~0.1–1 мС/см². → НЕ переносить напрямую как density (правило [[adapt-extracted-gbar]]: адаптировать под GHK + наши ионы/температуру). Kir2=0.14 мС/см² — совпадает с физиологией (Shen 2007), использовать можно. Полный репозиторий: `analiz/modeldb_gbar/msn_wolf2005/`.
5. **TAN**: численные G (HCN/SK/Cav2.2/Kv7) — нужна лит. (таблица параметров Codianni & Rubin 2023, и Goldberg & Wilson 2005 для γ=2.76 пС). Экспериментально TAN: HCN + NaP + Cav2.2→SK (sAHP), Kir (пауза).
6. **GPe/GPi**: точные G (Kv3.1, HCN) — нужна лит. Gunay 2008 (ModelDB 114639) имеет ручную модель `GENESIS_PAR_ROW="500 1 .1 10 20 2 4 .3 1 1 1"` (S/м²), но маппинг параметр→канал в `readGPparams.g` недоступен (403) — дорасшифровать.
