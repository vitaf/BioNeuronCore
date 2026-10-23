# NEURON_HUMAN — Гипоталамус + гипофиз (аудит, группа 7-8)

**Источник:** агент (WebSearch).

- **POMC** — анорексигенный, глутамат + α-MSH/β-эндорфин. **TRPC5** (лептин/инсулин), **KATP**. ⚠️ оба отсутствуют.
- **AgRP/NPY** — орексигенный, GABA+AgRP+NPY. **KATP (SUR1), GIRK (Kir3.2), HCN, KCNQ (Kv7)**; пейсмекер. ⚠️ нет (только HUNGER_SENSOR).
- **Oxytocin_Magnocellular** — Na (вкл NaP), Kv (I_A), Ca (L/N/P/Q/T), BK/SK, TRPM4-fDAP, Ih. Аксон в нейрогипофиз НЕмиелинизирован.
- **Vasopressin_Magnocellular** — те же + sDAP/fDAP (TRPM4/5); фазный burst 7–15 Гц. Аксон немиелинизирован ✅.
- **CRH_Neuron** — парвоцеллюлярный PVN, глутамат+CRH.
- **TRH_Neuron** — парвоцеллюлярный PVN, глутамат+TRH.
- **GnRH_Neuron** — **HCN1, Cav1.3/2.2/2.3, T-type, TRPC4, SK3**; burst-пульс. ⚠️ только Cav1.3.
- **GHRH_Neuron** — аркуатное, глутамат+GHRH.
- **Somatostatin_Neuron** — перивентрикулярное, GABA+соматостатин.
- **Kisspeptin_Neuron** — **Ih(HCN1)+I_T(Cav3.1)** → rebound-burst.
- **SCN_Circadian** — GABA; **HCN, BK, L-type (Cav1.2/1.3), A-type, KvDR, TASK/TREK**. ⚠️ только Cav1.3.
- **MammillaryBody** — глутамат, цепь Пейпеца.
- **Tanycyte** — глия, не нейрон; Kir4.1, Cx43 ✅.
- **TMN_Histamine** — гистамин; **HCN + persistent Na + Ca** — пейсмекер <4 Гц. ⚠️ HCN отсутствует.
- **VLPO_Sleep** — GABA+галанин; sleep-active.
- **Corticotroph** — CRH→cAMP→L-type Ca; Ca-спайки.
- **Gonadotroph** — GnRH→IP₃→Ca; **SK** (не BK).
- **Thyrotroph** — TRH→IP₃→Ca.
- **Somatotroph** — GHRH→cAMP; **T-type+L-type, BK** — plateau-burst.
- **Lactotroph** — спонтанный plateau-burst (BK); тоническое D₂-торможение.

## Главные расхождения
AgRP (KATP/GIRK/HCN/KCNQ), POMC (TRPC5), SCN/TMN (HCN/BK/L-type), GnRH (T-type/HCN/TRPC4); гипофиз сведён к Cav1.3 без BK/SK.

## Источники
- van den Top 2004 Nat Neurosci 7:493 (PMID 15097991) — AgRP пейсмекер.
- van den Top 2007 Neuroscience 144:815 — KATP/SUR1.
- Qiu 2018 J Neuroendocrinol (PMC5957276) — POMC TRPC5.
- Armstrong 2010 J Neuroendocrinol — OT/VP.
- Zhang 2013 Mol Cell Endocrinol 367:85 (PMID 23305677) — GnRH.
- Colwell 2011 Nat Rev Neurosci 12:553 — SCN.
- Takahashi 2006 J Neurosci 26:10292 — TMN.
- Stojilkovic 2005 (PMID 15860411) — гипофиз.

---

## Дополнение: гипофиз (эндокринные клетки) — каналы, γ/Po/G, плотности (агент, 2026-09-02)

**Формула пересчёта (как в коде):** `density [мкм⁻²] = G [мСм/см²] · 10 / (γ [пСм] · Po)`.

### Гормон/передатчик и рецепторный контур (5 типов)
| Тип | Гормон | Стимул (рецептор→каскад) | Торможение |
|---|---|---|---|
| Corticotroph | ACTH (POMC-продукт) | CRH (CRHR1)→cAMP/PKA (+AVP→PKC) | глюкокортикоиды |
| Gonadotroph | LH/FSH | GnRH (GnRHR, Gq)→IP₃→Ca²⁺ | — |
| Thyrotroph | TSH | TRH (TRHR, Gq)→IP₃→Ca²⁺ | T3/T4 |
| Somatotroph | GH | GHRH (Gs)→cAMP→PKA; ghrelin (GHS-R1a) | соматостатин (Gi) |
| Lactotroph | PRL | TRH (Gq); эстрадиол | дофамин D₂ (Gi→Kir) — тоническое |

### Одноканальная проводимость γ и Po (литература)
| Канал | γ (пСм) | Po | Примечание / источник |
|---|---|---|---|
| Nav (TTX-чувств.) | ~20 (Nav1.2/1.6 20.7) | пик 0.45; persistent 0.016 | Chatelier 2010; GH3-ток −82.9 пА/пФ @−5 мВ (Na/Hille) |
| Cav1.3 (L-type) | 25 (Ba²⁺); ~2.5–3 (физиол. 2 мМ Ca²⁺) | Po_max 0.2–0.23 | CAV13_1.md; basal_ganglia табл. |
| Cav3.1 (T-type) | 9.5 (Ca²⁺) | 0.1–0.3 | Biophys J 2008 |
| BK (KCa1.1) | 250 | Ca/V-завис. (0.2–0.99) | BK_104.md |
| SK (KCa2) | 4–14 (≈11) | Ca-завис. | basal_ganglia табл. |
| Kir/GIRK (Kir3) | ~30 | ~1 (leak/выпрямл.) | — |
| TASK-1 (KCNK3) | ~14 | фоновая (открыт в покое) | Talley 2000; TASK-обзор |
| TASK-3 (KCNK9) | ~28–38 | фоновая | TASK-обзор (Mol Interv 2003) |
| KvDR | ~27 (Kv3.1) | V-завис. | Grissmer 1992 |
| TRPC1 (неселект. катион) | ~30–40 (неселект.) | конститутивно/Gq | Kucka 2010 — лактотрофы/GH3 |

### Таблица «тип → канал → γ/Po/G → density → локализация → источник»
> G модельные пересчитаны nS→мСм/см² через площадь клетки (см. «Площади»): соматотроф d=15 мкм (A≈707 мкм², ×0.141), кортикотроф d=12 мкм (A≈452 мкм², ×0.221).

| Тип | Канал | γ (пСм) | Po | G | density (мкм⁻²) | Локализация | Источник |
|---|---|---|---|---|---|---|---|
| Somatotroph | Cav1.3 (L-type) | 25 (Ba²⁺) | 0.2 | 0.74 нС ≈ 0.105 мСм/см² | ~0.2 (γ=25) / ~1.8 (γ=3) | плазмалемма | Tsaneva-Atanasova 2007; 2013 Cav1.3↑ |
| Somatotroph | Cav3.1 (T-type) | 9.5 | 0.2 | 0.105 нС ≈ 0.015 мСм/см² | ~0.08 | плазмалемма | Tsaneva-Atanasova 2007 |
| Somatotroph | KvDR | 27 | V-зав. | 3.85 нС ≈ 0.54 мСм/см² | ~0.5 (Po≈0.5) | плазмалемма | Tsaneva-Atanasova 2007 |
| Somatotroph | Kir | 30 | ~1 | 15.75 нС ≈ 2.23 мСм/см² | ~0.7 | плазмалемма | Tsaneva-Atanasova 2007 |
| Somatotroph | BK (ближний) | 250 | Ca/V | 0.55 нС ≈ 0.078 мСм/см² | ~0.006 | у поры Cav (нанодомен) | Tsaneva-Atanasova 2007; Nowacki 2010 |
| Somatotroph | BK (дальний) | 250 | Ca/V | 10 нС ≈ 1.41 мСм/см² | ~0.06–0.3 | плазмалемма | Tsaneva-Atanasova 2007 |
| Corticotroph | Cav1.2/1.3 (L) | 25 | 0.2 | 9.0 нС ≈ 1.99 мСм/см² | ~0.4 (γ=25) / ~3.3 (γ=3) | плазмалемма | Shorten 2000 (модель) |
| Corticotroph | Cav3 (T) | 9.5 | 0.2 | 10.0 нС ≈ 2.21 мСм/см² | ~1.2 | плазмалемма | Shorten 2000 |
| Corticotroph | K-Ca (BK) | 250 | Ca/V | 0.09 нС ≈ 0.020 мСм/см² | ~0.0008 | плазмалемма | Shorten 2000; Duncan 2015 (BK: spike→burst) |
| Corticotroph | Kir | 30 | ~1 | 0.3 нС ≈ 0.066 мСм/см² | ~0.02 | плазмалемма | Shorten 2000; CRH ингибирует Kir |
| Gonadotroph | SK (KCa2) | 11 | Ca(ER-высвоб.) | нужна литература | нужна литература | сома (у ER Ca-сайтов) | Kukuljan 1992; Tse & Hille 1992 |
| Gonadotroph | Cav3 (T-type) | 9.5 | 0.2 | нужна литература | нужна литература | плазмалемма | Stutzin & Stojilkovic 1989 |
| Gonadotroph | Nav (TTX-чувств.) | 20 | 0.45 | нужна литература | нужна литература | плазмалемма | Tiwari & Sikdar (αT3-1) |
| Gonadotroph | NALCN (фон. Na⁺) | — | — | нужна литература | нужна литература | плазмалемма | bioRxiv 2021 (NALCN гипофиз) |
| Thyrotroph | SK (K-Ca, ER-высвоб.) | 11 | Ca-завис. | нужна литература | нужна литература | сома | Ritchie 1987; Simasko 1991 |
| Thyrotroph | Cav3 (T-type) | 9.5 | 0.2 | нужна литература | нужна литература | плазмалемма | Suzuki 1990; Roussel 1992 (TSH) |
| Thyrotroph | Cav1 (L-type) | 25 | 0.2 | TRH ↓~35% | нужна литература | плазмалемма | Simasko 1991 |
| Lactotroph | BK (KCa1.1) | 250 | Ca/V | высокая экспрессия | нужна литература | плазмалемма | Van Goor 2001 Mol Endocrinol 15:1222 |
| Lactotroph | TASK-1/TASK-3 (K2P) | 14 / 30 | фоновая | фоновая утечка K⁺ | нужна литература | плазмалемма | Talley 2000; Stojilkovic 2010 обзор |
| Lactotroph | Cav1.3 (L-type) | 25 | 0.2 | нужна литература | нужна литература | плазмалемма | Stojilkovic 2010 |
| Lactotroph | Kir3/GIRK (D₂) | 30 | ~1 | нужна литература | нужна литература | плазмалемма | Ingram 1986 (D₂ → гиперполяр. 30–35 мВ) |
| Lactotroph | TRPC1 (неселект.) | 30–40 | конститут. | нужна литература | нужна литература | плазмалемма | Kucka 2010 |

### Площади (сфера 4πr², аксона нет) и пассивные параметры
| Тип | d сомы | Площадь | RMP | Паттерн |
|---|---|---|---|---|
| Somatotroph | 12–18 мкм | ~450–1000 мкм² (≈707) | ~−50 мВ | plateau-burst (BK), пейсмекер |
| Lactotroph | 10–15 мкм | ~314–700 мкм² | ~−50 мВ | спонтанный plateau-burst; D₂ гиперполяр. до −80…−85 |
| Corticotroph | 10–12 мкм | ~314–452 мкм² | ~−50…−55 мВ | spiking → bursting (CRH/BK) |
| Gonadotroph | 10–14 мкм | ~314–600 мкм² | ~−50…−60 мВ | одиночные спайки + GnRH-Ca-осцилляции |
| Thyrotroph | 10–12 мкм | ~314–452 мкм² | ~−50…−60 мВ | TRH: гиперполяр. (SK) → деполяр. |

**Ключевые факты (честность):**
1. **BK только в соматотрофах/лактотрофах** (не в гонадотрофах) — задаёт plateau-burst; в гонадотрофах вместо BK — **SK**, сопряжённый с IP₃/ER-высвобождением Ca²⁺ (не с AP-входом Ca²⁺). Van Goor 2001; Kukuljan 1992.
2. **TASK (K2P)** — фоновая утечка K⁺ лактотрофов, ингибируется TRH (Gq); D₂-торможение PRL идёт через Kir/GIRK-гиперполяризацию + закрытие Cav. Прямых γ-измерений именно в лактотрофах мало — взяты гетерологичные γ(TASK-1=14, TASK-3≈30 пСм).
3. **Corticotroph CRH** = cAMP/PKA → (а) L-type Ca↑ (сдвиг V-зависимости/рост G), (б) Kir↓ → деполяризация; T-type задаёт возбудимость, но не генерирует AP напрямую. LeBeau 1997; Shorten 2000.
4. **Somatotroph GHRH** = cAMP/PKA → L- и T-type Ca↑; PKC → K↓. Cav1.3 — основной L-type (ghrelin/GHRP-6 селективно ↑ Cav1.3 + G_max). Chen 2009 (Na/Hille); 2013 PMID 24455243.
5. **Фоновый Na⁺ (NALCN)** держит RMP вблизи порога во всех типах гипофиза — убрать NALCN → гиперполяризация и молчание. bioRxiv 2021.
6. **γ(Ca) зависит от носителя**: в физиол. 2 мМ Ca²⁺ L-type ≈2.5–3 пСм (против 25 пСм в Ba²⁺) → density для Ca-каналов приводится диапазоном, нужна калибровка по носителю.

**Новые источники:** Stojilkovic/Tabak/Bertram 2010 Endocrine Reviews 31:845–915 (PMID 20650859, PMC3365841); Stojilkovic 2006 Biol Res 39:403; Van Goor 2001 J Neurosci 21:5902 (PMID 11487613); Van Goor 2001 Mol Endocrinol 15:1222 (PMID 11435620); Tsaneva-Atanasova 2007 J Neurophysiol 98:131 (PMID 17493919); Nowacki 2010 Physica D 239:485; Shorten/Robson/McKinnon/Wall 2000 J Theor Biol (CRH-кортикотроф); LeBeau 1997 Biophys J 73:1263 (PMID 9284294); Duncan 2015 (PMC4358680); Kucka 2010 (TRPC1 лактотрофы/GH3); Kukuljan 1992; Tse & Hille 1992; Stutzin & Stojilkovic 1989; Ritchie 1987 J Physiol; Simasko 1991 Endocrinology 128:2015 (PMID 1900783); Roussel 1992 (PMID 1284919); Suzuki 1990; Talley 2000 Neuron 25:399 (TASK-1); Ingram/Bicknell/Mason 1986 Endocrinology 119:2508; NALCN-гипофиз bioRxiv 2021 (10.1101/2021.08.02.454810); Cav1.3-ghrelin 2013 (PMID 24455243).

---

## Дополнение: гипоталамические НЕЙРОНЫ — каналы, γ/Po/G → density, пассив, морфология, миелин (агент, 2026-09-02)

**Формула (как в коде):** `density [мкм⁻²] = G [мСм/см²] · 10 / (γ [пСм] · Po)`, Po = peak (транзиентные) / steady (персистентные, leak).
⚠️ **Гипоталамические НЕЙРОНЫ: готовых G (мСм/см²) в открытой литературе почти нет** — публикуются пассивные
свойства (RMP/R_in/τ) и СОСТАВ токов, но не плотности. Поэтому density для большинства = «нужна литература»
(этап 6 — калибровка G по RMP/R_in как target). **C_m гипоталамуса ≈ 1 µF/см²** (классика; кора = 0.5, Eyal 2016).

### γ/Po нейрональных каналов (γ — из modifier_refs проекта; Po — литература)
| Канал | Модификатор | γ (пСм) | Po | Примечание |
|---|---|---|---|---|
| Nav1.6 transient | NAV16 (141) | 20.7 | ~0.5 (peak) | AIS/сома |
| Nav1.6 persistent (NaP) | NaP (105) | 20.7 | 0.016 | TMN/SCN пейсмейкер |
| Kv1.1 KDR | KV_DR (123) | 9–14 (≈11) | ~0.7 | реполяризация |
| Kv4.2 A-type | KV42 (85) | 5 | ~0.6 | AgRP/Kiss1/SCN/TMN |
| Kv7/KCNQ (M) | (нет) | ~7 | ~0.4 | AgRP KCNQ3; POMC (нужна лит.) |
| HCN1 (Ih) | HCN1 (129) | 1 | Po=1 (g_h max) | GnRH/Kiss1/SCN; V½ −69 |
| HCN2/4 (Ih) | HCN2/4 (130/132) | 1 | Po=1 | TMN Ih V½ −98 (≈HCN2/4) |
| SK2/SK3 | SK (103) | 10 | Ca (EC50 0.4 µM) | магноцеллюлярные |
| BK | BK (104) | 250 | Ca/V (HA α+β4) | магноцеллюлярные |
| Cav1.3 (L) | CAV13 (1) | 25 | ~0.2 | GnRH/SCN; V½ −35 |
| Cav1.2 (L) | CA_L_TYPE (84) | 25 | ~0.2 | SCN |
| Cav2.1 P/Q | CAV21 (140) | 15 | — | нейросекреция |
| Cav2.2 (N) | (нет) | ~14–19 | — | OT/VP → mAHP/sAHP |
| Cav2.3 (R) | (нет) | ~14 | — | VP sAHP |
| Cav3.1 (T) | PIR_TTYPE (96, γ=7.2) | 7–9.5 | V½_акт −49 | POMC/Kiss1/GnRH/SCN (изоформа-завис.) |
| TREK-1 (K2P) | K2P (107) | 100 | 0.012 | SCN/VLPO leak |
| TASK-1/3, THIK-1/2 | (нет) | 14–38 | фон | SCN TRESK, VLPO |
| Kir2.x | Kir (106) | 30 | V-завис. | POMC Kir2.3, CRH |
| GIRK (Kir3.x) | (нет) | ~30–39 | ~1 | AgRP/NPY, POMC, Kiss1 |
| KATP (Kir6.2/SUR1) | (нет) | ~70–80 | — | POMC/AgRP |
| TRPC4/5 | (нет) | ~40–60 | — | POMC, GnRH, Kiss1 |
| TRPM4/5 | (нет) | ~23–25 | Ca | OT/VP sDAP/fDAP |
| NALCN (Na-leak) | NALCN (133) | 27 | 1 | SCN дневной |

⚠️ GIRK/KATP/TRPC4/5/TASK/THIK/TRPM4/5/Cav2.2/Cav2.3 — каналы, **ПОКА НЕ реализованные** в проекте.

### Таблица «тип → канал → γ/Po/G → density → локализация → источник»
| Тип | Канал | γ (пСм) | Po | G (мСм/см²) | density (мкм⁻²) | Локализация/градиент | Источник |
|---|---|---|---|---|---|---|---|
| POMC | TRPC5 | ~50 | — | нужна лит. | нужна лит. | сома (лептин/инсулин→PI3K/PLCγ) | Qiu 2014 PMC4183666; Qiu 2018 PMC5957276 |
| POMC | KATP (Kir6.2/SUR1) | ~75 | — | нужна лит. | нужна лит. | сома (Zn²⁺ тормозит) | Qiu 2014; Parton 2007 |
| POMC | GIRK1/2 | ~35 | ~1 | нужна лит. | нужна лит. | сома (RMP) | Choi 2024 PMC11567913 |
| POMC | Kv7/KCNQ | ~7 | ~0.4 | нужна лит. | нужна лит. | сома | «нужна лит.» |
| POMC | Kir2.3 | 30 | V | нужна лит. | нужна лит. | сома | «нужна лит.» |
| POMC | Cav3.1 (T) | 7–9.5 | V½ −49 | нужна лит. | нужна лит. | соматодендрит | «нужна лит.» |
| AgRP/NPY | GIRK (Kir3.2) | ~39 | ~1 | нужна лит. | нужна лит. | сома (RMP −47.7) | Choi 2024 PMC11567913; Smith 2023 |
| AgRP/NPY | KATP (SUR1) | ~75 | — | нужна лит. | нужна лит. | сома | van den Top 2007 |
| AgRP/NPY | Ih (HCN) | 1 | 1 | нужна лит. | нужна лит. | сома | van den Top 2004 |
| AgRP/NPY | I_A (Kv4.2) | 5 | ~0.6 | нужна лит. | нужна лит. | сома | van den Top 2004 |
| AgRP/NPY | KCNQ3 (M) | ~7 | ~0.4 | нужна лит. | нужна лит. | сома | Stincic 2021 Mol Metab |
| OT_Magno | Nav (вкл. NaP) | 20.7 | 0.5/0.016 | 70 (сома) | ~68 | сома/AIS | Komendantov 2007 (gNa soma 70 000 µS/см²) |
| OT_Magno | Kv A-type | 5 | ~0.6 | 15–20 (дендрит) | ~50–67 | соматодендрит | Komendantov 2007 (gA pd 15 000 / sd 20 000 µS/см²) |
| OT_Magno | Ca L/N/P/Q/T | 25/15/7.2 | ~0.2 | L 0.4; N 0.26 | ~0.8 (L) | соматодендрит | Komendantov 2007 (gCa,L 400 / gCa,N 260 µS/см²) |
| OT_Magno | BK (fAHP) | 250 | Ca/V | 3.5 | ~0.28 | сома | Komendantov 2007 (gBK 3500 µS/см²); Greffrath 2004 |
| OT_Magno | SK (mAHP/sAHP) | 10 | Ca | 0.01 | ~0.02 | сома | Komendantov 2007 (gSK 10 µS/см²); Greffrath 2004 |
| OT_Magno | TRPM4 (sDAP) | ~23 | Ca | 0.015–0.045 (gCAN) | ~0.013–0.039 | сома | Komendantov 2007 (gCAN 15–45 µS/см²) |
| OT_Magno | Ih (HCN) | 1 | 1 | нужна лит. (Ih мал ~30–50 пА, нет плотности) | нужна лит. | сома | Pires da Silva 2016 J Neurosci (PMID 27807172) |
| VP_Magno | + TRPM4/5 (sDAP/fDAP) | ~23 | Ca | 0.015–0.045 (gCAN) | ~0.013–0.039 | сома; burst 7–15 Гц | Komendantov 2007; Ghamari-Langroudi 2010 |
| VP_Magno | N→mAHP, R-type→sAHP | 14–19 | — | N 0.26 | ~0.9 | сома | Komendantov 2007 (gCa,N 260 µS/см²); Greffrath 2004 |
| GnRH | HCN1 | 1 | 1 | нужна лит. | нужна лит. | сома (E2 ↑) | Zhang 2013 PMID 23305677 |
| GnRH | Cav1.3 | 25 | ~0.2 | нужна лит. | нужна лит. | сома | Zhang 2013 |
| GnRH | Cav2.2/2.3 | 14–19 | — | нужна лит. | нужна лит. | сома | Zhang 2013 |
| GnRH | Cav3.x (T) | 7.2 | V½ −49 | нужна лит. | нужна лит. | сома (burst) | Zhang 2013 |
| GnRH | TRPC4 | ~50 | — | нужна лит. | нужна лит. | сома (kisspeptin→Gq) | Zhang 2013; PMC4459938 |
| GnRH | SK3 | 10 | Ca | нужна лит. | нужна лит. | сома (E2 ↓) | Zhang 2013 |
| Kisspeptin (KNDy) | HCN1/2 (Ih) | 1 | 1 | нужна лит. | нужна лит. | сома (E2 ↑3.4×) | Zhang 2013; PMC6103748 |
| Kisspeptin | Cav3.1 (IT) | 7–9.5 | V½ −49 | нужна лит. | нужна лит. | сома (E2 ↑6×) | Zhang 2013; Bosch 2013 |
| Kisspeptin | TRPC5, GIRK, Kv4.2 | ~50/35/5 | — | нужна лит. | нужна лит. | сома | Bosch 2024 (arcuate) |
| SCN | NALCN (Na-leak) | 27 | 1 | нужна лит. | нужна лит. | сома (день ↑) | Harvey 2020 Physiol Rev |
| SCN | BK + Cav1.2/1.3 | 250/25 | — | нужна лит. | нужна лит. | сома (день: L→β2→BK-инакт.) | Harvey 2020 |
| SCN | TRESK/TASK (K2P) | 14–38 | фон | нужна лит. | нужна лит. | сома (ночь ↑) | Lambert 2020 Nat Commun 11:4327 |
| SCN | HCN (Ih) | 1 | 1 | нужна лит. | нужна лит. | сома (rebound) | de Jeu & Pennartz; Harvey 2020 |
| SCN | T-type (rebound) | 7.2 | V½ −49 | нужна лит. | нужна лит. | сома | Pennartz 1998 PMID 9503337 |
| SCN | A-type + KvDR | 5/11 | — | нужна лит. | нужна лит. | сома | Colwell 2011 |
| MammillaryBody | Ca (LTS+ITP) | 25/7.2 | — | нужна лит. | нужна лит. | соматодендрит (burst) | Alonso & Llinás 1992 PMID 1432086 |
| MammillaryBody | Na + KDR | 20.7/11 | — | нужна лит. | нужна лит. | сома/AIS | Alonso & Llinás 1992 |
| TMN_Histamine | Ih (HCN) | 1 | 1 | нужна лит. | нужна лит. | сома; V½ −98, τ 823 мс @−90 | Kamondi & Reiner 1991 PMID 1812224 |
| TMN | NaP (TTX-чувств.) | 20.7 | 0.016 | нужна лит. | нужна лит. | сома (препотенциал) | Uteshev 1995; Takahashi 2006 |
| TMN | I_A + Ca-зав. K | 5 | ~0.6 | нужна лит. | нужна лит. | сома | Haas & Reiner 1988 |
| TMN | Ca (L/N) | 25/14 | — | нужна лит. | нужна лит. | сома | Stevens & Haas |
| VLPO_Sleep | TASK/THIK (K2P) | 14–38 | фон | нужна лит. | нужна лит. | сома (изофлуран закрывает THIK-1/2) | Moore 2012 PMC3628836 |
| VLPO | α2 → GIRK/Kir | ~35 | ~1 | нужна лит. | нужна лит. | сома (NE тормозит) | Moore 2012; J Neurosci 34:16385 |

### Пассивные свойства + морфология + миелин (нейроны)
- **POMC (ARC):** RMP — нужна лит. (GIRK поддерживает; лептин/инсулин деполяризуют через TRPC5). Сома d ~10–15 мкм (r 5–7.5); аксон → PVN/LHA. Миелин нет.
- **AgRP/NPY (ARC):** RMP **−48.3 мВ** (−40…−60), **R_in 1.3 ГΩ**, спонтанный пейсмейкер (van den Top 2004). GIRK-блок (tertiapin-Q): −47.7→−44.9 мВ (Choi 2024). Порог ~−40 мВ. Сома d ~13.4 мкм (r 6.7), 1–3 дендрита. Миелин нет.
- **OT/VP Magno (SON/PVN):** срез (Stern & Armstrong 1996) OT RMP −63.4 мВ / R_in 255 МΩ / τ 15 мс; VP −58.9 мВ / 244 МΩ / 15.7 мс. Изолированные (Oliet & Bourque 1992): RMP −64.1 мВ, R_in 571 МΩ, τ 8.7 мс. Сома d ~20–30 мкм (r 10–15), 1–3 толстых дендрита; аксон → нейрогипофиз, **немиелинизирован** ✅.
- **CRH/TRH (парво PVN):** CRH — Matovic 2020 J Physiol (JP279666) стресс ↓ возбудимость + ↑ площадь; Pati 2020 — солевая нагрузка → Kir-гиперполяр. RMP/R_in/C_m — нужна лит. Сома d ~10–12 мкм; аксон → медианная эминенция. Миелин нет.
- **GnRH:** высокий R_in, малая C_m (~10 пФ) — точные RMP/R_in нужна лит. Мигрирующие, сома d ~10–15 мкм, биполярные; аксон → медианная эминенция. Миелин нет.
- **GHRH (ARC):** спонтанная 2–3 Гц (burst ≤10 Гц), крупная AHP; ghrelin прямо стимулирует. RMP/R_in — нужна лит. Миелин нет.
- **Somatostatin (перивентр.):** GABA+соматостатин; NPW деполяризует (↓GH); low-threshold Ca-спайк — нужна лит. Миелин нет.
- **Kisspeptin (KNDy, ARC+RP3V):** rebound-burst; порог гиперполяризации **−84.7 мВ** (V½ де-инактивации I_T); E2 ↑ Ih ×3.4, I_T ×6 (Zhang 2013). Миелин нет.
- **SCN:** RMP **−32 (день)/−43.9 (ночь)** (перфорир.); R_in **2.0/1.3 ГΩ**; C_m **11 пФ**; τ **29/23 мс** (Pennartz 1998; de Jeu 1998). Интрацеллюлярно (старые) ≈ −60 мВ. Сома d ~8–12 мкм; area ≈ 1100 мкм² (C_m 11 пФ при 1 µF/см²). Миелин нет.
- **MammillaryBody:** RMP **−57±5.5 мВ**, R_in **176±83 МΩ**, spike 58 мВ, сома **8–12 мкм**; LTS+ITP (ITP ≥100 мс), burst 0.9 Гц (Alonso & Llinás 1992). Mammillothalamic тракт миелинизирован (тонкий); соматический аксон — нужна лит.
- **TMN_Histamine:** RMP **~−50 мВ**, спонтанная 2–4 Гц (<4 Гц); R_in 150–500 МΩ (sharp)/~1 ГΩ (whole-cell); τ ~20 мс; широкий спайк 1.8–4 мс, медленная AHP 100–200 мс. **Ih V½ −98 мВ** (≈HCN2/4, НЕ HCN1 −69), E_rev −35 мВ, τ 823 мс @−90 / 280 мс @−130 / 302 мс deact @−60 (Kamondi & Reiner 1991). Варикозные аксоны, немиелинизированы.
- **VLPO_Sleep:** GABA+галанин; изофлуран деполяризует (закрывает THIK-1/2), NE тормозит через α2. RMP/R_in — нужна лит. Миелин нет.
- **Tanycyte (глия, НЕ нейрон):** Kir4.1 + Cx43 ✅; ПД нет; барьер/транспорт 3-го желудочка.

### Ключевые расхождения с кодом (нейроны)
POMC/AgRP (GIRK, KATP, TRPC5, KCNQ, T-Ca) отсутствуют; SCN (NALCN/BK/TRESK/HCN/T-type) сведён к Cav1.3;
TMN Ih (V½ −98 → HCN2/4) отсутствует; VLPO (TASK/THIK, α2) отсутствует; GnRH/Kisspeptin (HCN/T-type/TRPC4) отсутствуют;
магноцеллюлярные (BK/SK/TRPM4) отсутствуют; MammillaryBody (LTS/ITP Ca-каналы) — нет.

### Новые источники (к существующим)
- Qiu 2014 (PMC4183666), Qiu 2018 (PMC5957276) — POMC TRPC5.
- Choi 2024 Molecules and Cells 47:100122 (PMC11567913) — GIRK1/2 RMP POMC/NPY.
- Smith 2023 PLOS Biol — GIRK2 AgRP (метаболизм); Stincic 2021 Mol Metab — KCNQ3 AgRP.
- Stern & Armstrong 1996 J Neurosci 16:4861 (PMID 8756418) — пассив OT/VP.
- Oliet & Bourque 1992 J Physiol 455:291 — изолированные MNC (RMP −64, R_in 571 МΩ).
- Greffrath 2004 (PMID 15214861) — BK/SK AHP SON, N/R-type сопряжение.
- Ghamari-Langroudi & Bourque 2010 — TRPM4/5 sDAP/fDAP, muscarinic→phasic.
- Zhang 2013 Mol Cell Endocrinol 367:85 (PMID 23305677) — GnRH (E2).
- Bosch 2013 (PMC3882370), Bosch 2024 — Kiss1 Ih+IT, E2-модуляция.
- Harvey 2020 Physiol Rev 100:1415 (physrev.00027.2019) — SCN каналы/циркадианность.
- Lambert 2020 Nat Commun 11:4327 — TRESK (K2P) SCN.
- Pennartz 1998 J Physiol 506:775 (PMID 9503337) — SCN кластеры/rebound/RMP/R_in.
- Alonso & Llinás 1992 J Neurophysiol 68:1321 (PMID 1432086) — маммиллярные тела.
- Kamondi & Reiner 1991 J Neurophysiol 66:1902 (PMID 1812224) — TMN Ih (V½ −98).
- Uteshev 1995 / Takahashi 2006 J Neurosci 26:10292 — TMN persistent Na.
- Moore 2012 (PMC3628836); J Neurosci 34:16385 — VLPO THIK-1/2 K2P, α2.
- Komendantov 2007 J Comput Neurosci 23 — модель MNC (распределение G сома/дендриты; точные G — полный текст).
- Matovic 2020 J Physiol (JP279666) — CRH стресс-пластичность.

---

## Дополнение: найденные литературные G (плотности проводимости) — агент, 2026-09-05

Правила: только реальные G. `density = G·10/(γ·Po)`. Nav AIS = 500 мСм/см² (проект); NALCN не AIS.

### 1. Магноцеллюлярные OT/VP (SON/PVN) — ЕДИНСТВЕННЫЙ полный плотностный источник
**Komendantov, Trayanova & Tasker 2007, J Comput Neurosci 23(2):143–168 (DOI 10.1007/s10827-007-0024-z, PMID 17484044).**
Многокомпартментная модель MNC: сома-цилиндр **15 мкм × 25 мкм**, первичный дендрит 2×50 мкм, вторичный 1×200 мкм. **C_m = 1 µФ/см²**, E_Na = +55, E_K = −100 мВ, R_a = 200 Ω·см. Таблица 1 в µS/см² (перевод в мСм/см² = ÷1000):

| Ток | Сома (мСм/см²) | Первичный дендрит | Вторичный дендрит |
|---|---|---|---|
| Na (fast) | **70** | 50 | 10 |
| K (DR) | **4** | — | — |
| K A-type (I_A) | — | 15 | 20 |
| Ca L-type | **0.4** | 0.22 | 0.46 |
| Ca N-type | **0.26** | 0.1 | — |
| SK (KCa2) | **0.01** | 0.02–0.04 | 0.005–0.025 |
| BK (KCa1.1) | **3.5** | 3.0 | — |
| CAN (неселект. катион, sDAP-субстрат ≈ TRPM4/5) | **0.015–0.045** | 0.03–0.09 | — |

⚠️ **Ih (HCN) и T-type Ca в таблице Комендантова ОТСУТСТВУЮТ** — для них плотности нет.
Ih магноцеллюлярных: малый, ~30–50 пА, R_in 1.1 ГΩ (Pires da Silva 2016 J Neurosci 36:11298, PMID 27807172; Erickson 1990 J Neuroendocrinol 2:301 — Ih в 50/99 клеток, активируется < −75 мВ). → **плотность Ih/T-Ca = «нужна лит.»**.

### 2. SCN (циркадные) — Sim & Forger 2007 (J Biol Rhythms 22:… «Modeling the Electrophysiology of SCN Neurons»)
Модель в ТОТАЛЬНЫХ проводимостях (nS), не плотностях: **C = 5.7 пФ**, g_K = 14 нС, g_L = 1/11 нС (~0.09 нС), E_K = −97, E_L = −29, E_Na = +45, E_Ca = +61 мВ. g_Na/g_Ca — «не извлечены». При C_m = 1 µФ/см² (площадь ≈ 570 мкм²): g_K ≈ 2.5 мСм/см², g_L ≈ 0.016 мСм/см². ⚠️ HCN/T-type/BK/NALCN плотности в SCN — **«нужна лит.»** (модель Sim&Forger их не содержит).

### 3. GnRH — Moran, Moenter & Khadra 2016 (J Comput Neurosci 40(3):297–315, DOI 10.1007/s10827-016-0598-4)
Частичные значения (square-wave режим, ТОТАЛЬНЫЕ нС, не плотности): g_K = 140 нС, g_HVA = 6 нС, g_s (медленный входящий) = 0.83 нС, g_KCa = 0.833 нС. g_NaF/g_NaP/g_A/g_Ih/g_LVA — «подобраны под voltage-clamp», значения в полном тексте Table 2 не извлечены → **«нужна лит.»** (полный текст).

### 4. TMN (гистаминовые) — Haas & Reiner 1988 (J Physiol 399:633–646, PMID 3404470)
Только пассив/качество (ПЛОТНОСТЕЙ НЕТ): RMP ≈ −50 мВ, R_in 176±42 МΩ, τ 19.8 мс, спайк 75 мВ, AHP 300–600 мс (Cs⁺-чувствит., Ca-зависимая). Ih: V½ −98 мВ, τ 823 мс @−90 (Kamondi & Reiner 1991). → **все плотности TMN (Ih/NaP/I_A/Ca) = «нужна лит.»** — публикуются свойства, не G.

### 5. Гипофиз — гонадотроф/тиреотроф/лактотроф
Плотности G (мСм/см²) для Nav/T-Ca/SK/NALCN (гонадотроф), SK/T-Ca/L-Ca (тиреотроф), BK/TASK/Cav1.3/GIRK/TRPC1 (лактотроф) в открытой литературе **не найдены** (публикуются качественные токи, не плотности). → **«нужна лит.»**. (Соматотроф и кортикотроф — G уже есть: Tsaneva-Atanasova 2007 / Shorten 2000, см. таблицу выше.)

### 6. НЕ найдены (остаются «нужна лит.», плотности в лит. нет/за пэйволлом)
- **POMC** (TRPC5/KATP/GIRK/KCNQ/Kir2.3/T-Ca), **AgRP/NPY** (GIRK/KATP/HCN/I_A/KCNQ3) — качественный состав подтверждён (Choi 2024 GIRK1/2 RMP; van den Top 2004 пейсмейкер), но плотности не публикуются.
- **Kisspeptin/KNDy** — модели сетевые (Voliotis 2021 eLife 71252, грубые ODE), не conductance-based; gH/gT плотностей нет.
- **MCH** — conductance-модели с плотностями не найдены.
- **MammillaryBody, VLPO, CRH/TRH, GHRH, Somatostatin** — пассив/качество есть, плотностей нет.
- **Raphe/LC/orexin** — см. NEURON_HUMAN_neuromodulatory.md (Tuckwell 2014 полная таблица — в PDF articles/, значения за пэйволлом; orexin Williams REU — тотальные µS).

**Вывод:** единственный ПОЛНЫЙ плотностный источник для гипоталамуса = Komendantov 2007 (магноцеллюлярные). Остальные типы — этап 6 (калибровка G по RMP/R_in/C_m как target, R_in из литературы: AgRP 1.3 ГΩ, SCN 2.0/1.3 ГΩ, TMN 176 МΩ, MNC 571 МΩ, MammillaryBody 176 МΩ).

---

## MCH + Pituicyte — применено в neuron_library.h (2026-09-07)

Плотности калиброваны по RMP/R_in (лит. плотностей γ/Po/G нет — conductance-моделей не существует).

### MCH neuron (лат. гипоталамус) — `Human::MCH()`
**Состав (density калибровка):** IONOTROPIC_NAK 1.0 (TTX-S спайк, порог ~−30), PUMP_NAK 0.5, TASK 0.05 (K2P leak — задаёт RMP −61), KV42 0.2 (KA/A-type — задержка спайка), KV_DELAYED_RECTIFIER 0.2 (адаптация), SK 0.02 (AHP), BK 0.1 (реполяризация), GIRK 0.3 (баклофен), CAV13 0.2 (HVA Ca L), CAV22 0.1 (HVA Ca N). **НЕТ Ih/HCN, НЕТ T-type** (канонически; Ih только в MCH+/CART− субпопуляции).
**Targets:** RMP −61.6 мВ (Eggermann 2003) / −57.5 (культура Gao), Rin 400–1000 МΩ, Cm 58–70 пФ, silent (<0.25 Гц).
**Источники:** Eggermann 2003 J Neurosci 23:1557 (PMID 12629156); Gao & van den Pol 2003 J Neurophysiol (DOI 10.1152/jn.00593.2003); van den Pol 2004 Neuron (PMID 15157424); Linehan & Hirasawa 2018 (A-like, Cm); Adekunle 2024 (MCH+/CART− Ih).

### Pituicyte (нейрогипофиз, глия) — `Human::Pituicyte()`
**Состав (пассивная глия, НЕ спайкует):** K2P_LEAK 0.5 (фоновый K⁺ — доминанта покоя), PUMP_NAK 0.4, BK 0.05 (сигнальный ATP→P2Y→Ca→BK), SK 0.02 (сигнальный apamin), IONOTROPIC_CL 0.1 (Cl-leak, VRAC-подобный объём-чувствительный). **НЕТ Na-спайков/VGCC/VGNC** (Burnard 1991). nt_released=−1 (глия).
**Targets:** RMP −38.7 мВ, Rin 15.6 МΩ (Burnard 1991), Cm ~10–30 пФ (геометрия). G_rest ≈ 1/Rin ≈ 64 пС.
**Источники:** Burnard, Pittman, MacVicar 1991 J Neuroendocrinol 3:433 (PMID 19215489 — единственный RMP/Rin); Troadec 2000 Pflügers Arch (ATP/P2Y→K⁺ BK/SK); Hussy 2004 (VP/OT→таурин через VRAC).
