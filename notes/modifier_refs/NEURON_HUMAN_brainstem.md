# NEURON_HUMAN — Brainstem (аудит, группа 9)

**Источник:** агент (WebSearch).

## Дыхательный ритм / стволовые пейсмейкеры
- **PreBotzinger** — глутамат (Dbx1+/VGluT2). **NaP + ICAN (CAN)** + SK/KNa; RMP −55..−60; insp-берсты 3–5 Гц. ⚠️ нет ICAN.
- **PPTg_Cholinergic** — ACh; тонический пейсмейкер.

## Вестибулярный / ретикулярная формация
- **Vestibular** — глутамат (часть медиального — GABA); тонический 50–100 Гц.
- **Pontine_Reticular, GigantocellularReticular** — глутамат (часть pontine ACh); NRGC сома 100 мкм, миелин.

## Слуховой ствол
- **CochlearNucleus_Bushy** — глутамат; **Kv3.1+Kv1.1**+Na, BK; phase-lock; миелин.
- **SuperiorOlive_MSO** — глутамат; Kv1 (low-threshold K) + HCN; ITD.
- **MNTB_Principal** — **глицин**; Kv3.1+Kv1; calyx of Held; миелин.
- **DStellate_Cochlear** — **глицин** (radiate multipolar VCN→DCN). ⚠️ описание «DCN» — на самом деле VCN.
- **InferiorColliculus** — глутамат (часть GABA).
- **SpiralGanglion** — глутамат; тип I миелин.

## Черепные мотонейроны (ACh, миелин да)
Oculomotor, Trochlear, Abducens, Facial, Vagus(NA), Accessory, Hypoglossal, TrigeminalMotor_MN.
Канонический набор: **Na + KvDR + SK (AHP) + Ca (CaP/Cav1.3) + HCN (Ih)**. ⚠️ в коде только Na/K2P — **SK, HCN, персистентный Ca отсутствуют**.

## Парасимпатические преганглионарные (ACh)
- **SupSalivatory, InfSalivatory, DorsalMotorVagus** — ACh, миелин.
- **EdingerWestphal** — ⚠️ EWpg=ACh (верно), но «EW» в атласах человека = urocortin-1 (НЕ холинергическая). Уточнить.

## Прочие
- **MLR_Cuneiform** — глутамат (CnF).
- **Parabrachial** — глутамат (+CGRP).
- **RedNucleus** — глутамат (rubrospinal, миелин).
- **TrigeminalMesencephalic** — глутамат; первично-сенсорный внутри ЦНС, миелин.
- **NTS_Viscerosensory** — глутамат (+нейропептиды).
- **PAG_Neuron** — гетерогенно GABA + глутамат.
- **RVM_ONCell** — глутамат/GABA; **RVM_OFFCell** — GABA ✅.
- **AreaPostrema** — глутамат.

## Главные расхождения
PreBötC нет ICAN; черепные МН нет SK/HCN/Ca; D-stellate описание; EW urocortin.

## Источники
- Del Negro 2002 J Neurophysiol 87:457; 2005 J Neurosci 25:446 (PMID 15647488) — NaP/ICAN.
- Wang 1998 J Physiol 509:183 (PMID 9547392) — Kv3.1 MNTB/bushy.
- Doucet & Ryugo 1997 J Comp Neurol 379:63 — D-stellate.
- Kozicz 2011 J Comp Neurol 519:1413 (PMID 21452224) — EWpg/EWcp.

---

## Дополнение: литературные каналы и плотности γ/Po/G (агент, 2026-09-02)

**Формула пересчёта (как в коде):** `density [мкм⁻²] = G [мСм/см²] · 10 / (γ [пСм] · Po)`.
Для вольт-зависимых каналов Po — гейт (m³h и т.п.), для leak/persistent — steady-state Po
(гейтинг передаётся в `open_prob` модификатора, НЕ дублировать в density — см. примечание таламуса).

### Одноканальная проводимость γ и Po (из аудированных справочников модификаторов)
| Канал | γ (пС) | Po | Примечание / источник |
|---|---|---|---|
| Nav1.6 (fast) | 20.7 | пик ~0.45; persistent 0.016 | [[NAV16_141]]; [[NaP_105]] |
| NaP (persistent Nav1.6) | 20.7 | **0.016** | Mes V V½ −58.7 мВ (Wu 2005) |
| Kv1.1 (KDR) | 9–14 (≈12) | гейт m | [[KV_DR_123]] |
| Kv3.1 | 27 | гейт | Grissmer 1992 |
| Kv4 (A-ток) | ~4 голый / ~7.5 с DPP6 | гейт m·h | [[KV42_85]] |
| K2P TREK-1 | 100 | **0.012** | [[K2P_107]] |
| K2P TASK1 / TASK3 | ~14 / ~40 | ~0.012 (leak) | Meuth 2003 (см. таламус) |
| HCN1 / HCN2 | ~1 / ~1.5 | V-завис. (V½ −69/−95) | [[HCN1-4_129-132]] |
| SK2 | 10 | Ca²⁺-завис. (EC50 0.4 µM, Hill 4) | [[SK_103]] |
| BK (α+β4) | 250 | V+Ca²⁺ (Horrigan-Aldrich) | [[BK_104]] |
| KNa Slack (KCNT1) | 180 | Na⁺-завис. (EC50 ~41 мМ) | [[KNA_121]] |
| Cav1.3 (L) | 25 (Ba²⁺) | Po_max ~0.2 | [[CAV13_1]] |
| Cav2.1 (P/Q) | 9–19 (≈15) | гейт m²h | [[CAV21_140]] |
| Cav3.2 (T) | 7.2 | Po 0.1–0.3 | [[PIR_TTYPE_96]] |
| NALCN (leak Na) | 27 | ~1 (всегда открыт) | [[NALCN_133]] |
| CAN (TRPM4/5) | ~25 (TRPM4) | Ca²⁺-завис. | Pace 2007; Crowder 2007 (НЕ в палитре) |

### Таблица «тип → канал → γ/Po/G → density → локализация → источник»
| Тип | Канал | γ (пС) | Po | G (мСм/см²) | density (мкм⁻²) | Локализация | Источник |
|---|---|---|---|---|---|---|---|
| PreBotzinger | NaP (Nav1.6) | 20.7 | 0.016 | 0.133 (gNaP=2.8 нС/2100 мкм²) | ~4 | сома диффузно | Butera 1999 (PMID 10400966); Del Negro 2002 |
| PreBotzinger | Nav fast | 20.7 | 0.45 (пик) | 1.33 (gNa=28 нС) | ~1.4 | сома | Butera 1999 |
| PreBotzinger | KDR (Kv) | 12 | гейт | 0.53 (gK=11.2 нС) | ~4.4 | сома | Butera 1999 |
| PreBotzinger | CAN (TRPM4/5) | ~25 | Ca²⁺-завис. | нужна лит-ра (gCAN — 12-кан. модель Purvis 2007, PMID 17167061) | — | через Ca²⁺ | Pace 2007; Crowder 2007 |
| PreBotzinger | KNa Slack | 180 | Na⁺-завис. | нужна лит-ра (gNaK — 12-кан. модель Purvis 2007, PMID 17167061) | — | сома (терминация вдоха) | Krey 2010; Revill 2021 |
| PreBotzinger | TASK (K2P) | 14–40 | 0.012 | 0.095–0.33 (gLeakK 2–7 нС; burst 2–3 нС → 0.095–0.14) | — | сома (утечка) | Koizumi & Smith 2008 (J Neurosci 28:1773); Butera 1999 (площадь 2100 мкм²) |
| PreBotzinger | SK2 | 10 | Ca²⁺-завис. | нужна лит-ра | — | сома (sigh/AHP) | Picardo 2013 |
| RedNucleus | Nav fast | 20.7 | гейт | нужна лит-ра | — | сома/аксон (миелин) | guinea-pig RN J Physiol 1985 (PMID 2410599) |
| RedNucleus | HVA Ca (L/P/Q) | 25/15 | гейт | нужна лит-ра | — | сома-дендрит (Ca-спайк, TTX-резист.) | J Physiol 1985 |
| RedNucleus | SK/BK (sAHP) | 10/250 | Ca²⁺-завис. | нужна лит-ра | — | сома (sAHP, Co²⁺-блок) | J Physiol 1985 |
| RedNucleus | Ih (HCN) | 1–1.5 | V-завис. | нужна лит-ра | — | сома-дендрит (Cs⁺-чувств. саг) | J Physiol 1985; Al 1989 |
| RedNucleus | T-Ca (rebound) | 7.2 | 0.1–0.3 | нужна лит-ра | — | сома-дендрит (rebound) | Al 1989 (rat slices) |
| NTS_Viscerosensory | Kv4 (A-ток) | 4–7.5 | гейт | нужна лит-ра | — | сома-дендрит (fast ~50/медл. ~200 мс) | Saliba (Kv4 NTS) |
| NTS_Viscerosensory | Kv2 (KDR) | ~8–10 | гейт | нужна лит-ра | — | сома (огранич. частоты, гиперполяр.) | PMC11192486 |
| NTS_Viscerosensory | SK (apamin AHP) | 10 | Ca²⁺-завис. | нужна лит-ра | — | сома | Tell & Jean 1993 (PMID 7509858) |
| NTS_Viscerosensory | T-Ca (Cav3) | 7.2 | 0.1–0.3 | нужна лит-ра | — | сома-дендрит | (подтвердить) |
| PPTg_Cholinergic | A-ток (IA) | 7.5 | гейт | нужна лит-ра | — | сома-дендрит (Type II, задержка ПД) | Takakusaki 1997 |
| PPTg_Cholinergic | KCNQ4/M-ток | 7–11 | гейт | нужна лит-ра | — | сома (CB1-модуляция) | PMC6503533 |
| PPTg_Cholinergic | HCN (Ih) | 1–1.5 | V-завис. | нужна лит-ра | — | сома-дендрит (пейсмекер) | (нужна лит-ра) |
| MLR_Cuneiform | Nav/NaP + Ih + персист. входящий | 20.7/1 | гейт | нужна лит-ра | — | сома (глутамат, MLR) | Sci Rep 2025 (PMC12639178) |
| Parabrachial | GIRK-подобный K (μ/GABAb/M2) | ~25–30 | G-завис. | нужна лит-ра | — | сома-дендрит (общий K-канал) | Christen 1990 |
| Parabrachial (Calca/CGRP) | Nav (Scn2b/Scn4b, resurgent/NaP) | 20.7 | гейт | нужна лит-ра | — | сома (гипервозбудимость) | bioRxiv 2026 |
| PAG_Neuron | NaP | 20.7 | 0.016 | нужна лит-ра | — | сома (пейсмейкер, НЕОБХОДИМ) | Dougalis 2017 (PMID 28367595) |
| PAG_Neuron | Ih (HCN) | 1–1.5 | V-завис. | нужна лит-ра | — | сома-дендрит (не обязателен) | Dougalis 2017 |
| PAG_Neuron | T-Ca LVA | 7.2 | 0.1–0.3 | нужна лит-ра | — | сома-дендрит | Dougalis 2017 |
| PAG_Neuron | A-ток + SK + BK + KDR | 7.5/10/250/12 | гейт | нужна лит-ра | — | сома (коллект. модуляция частоты) | Dougalis 2017 |
| RVM_ONCell | Nav fast | 20.7 | гейт | нужна лит-ра | — | сома (FS/SS подтипы) | Zagon 1997 |
| RVM_OFFCell | Nav fast (SS чаще) | 20.7 | гейт | нужна лит-ра | — | сома (disinhibited μ) | Zagon 1997 |
| RVM (5-HT) | T-Ca + SK (широкий ПД, высокий R_in) | 7.2/10 | гейт | нужна лит-ра | — | сома | Zhang 2006 |
| TrigeminalMesencephalic | NaP (Nav1.6) | 20.7 | 0.016 | нужна лит-ра | — | сома (осцилляции/резонанс) | J Neurosci 2006 (PMID 16571748) |
| TrigeminalMesencephalic | Resurgent Na | 20.7 | гейт | нужна лит-ра | — | сома (пик AHP) | J Neurosci 2006 |
| TrigeminalMesencephalic | Ih (HCN1/2) | 1–1.5 | V-завис. | нужна лит-ра | — | сома-дендрит (саг/резонанс) | Hsiao (Mes V) |
| TrigeminalMesencephalic | Kv1 (4-AP, низкопорог.) | 9–14 | гейт | нужна лит-ра | — | сома (резонанс, порог) | Neuroscience 2009 (PMID 19591906) |
| TrigeminalMotor_MN | TASK1/TASK3 (leak K) | 14/40 | 0.012 | нужна лит-ра | — | TASK1 сома / TASK3 дендриты (R_in, рекрутинг) | Saito (dl-TMN) |
| TrigeminalMotor_MN | SK/BK (AHP) | 10/250 | Ca²⁺-завис. | нужна лит-ра | — | сома (mAHP) | Hsiao 1997 (PMID 9212246) |
| TrigeminalMotor_MN | Ih (HCN) | 1–1.5 | V-завис. | нужна лит-ра | — | сома (5-HT ↑Ih) | Hsiao 1997 |
| TrigeminalMotor_MN | HVA Ca (L/N/P/Q) | 25/2.76/15 | гейт | нужна лит-ра | — | сома-дендрит | Hsiao 1997 |
| Pontine_Reticular | T-Ca (LTS) | 7.2 | 0.1–0.3 | нужна лит-ра | — | сома (LTS, деинакт. от гиперполяр.) | Greene 1986 (PMID 3775364) |
| Pontine_Reticular | Ih (саг) | 1–1.5 | V-завис. | нужна лит-ра | — | сома (SubC) | Brown 2006 |
| Pontine_Reticular | A-ток (transient outward) | 7.5 | гейт | нужна лит-ра | — | сома (инакт. в REM) | Gerber 1989 |
| GigantocellularReticular | Nav (инактивируется при −23 мВ) | 20.7 | гейт | нужна лит-ра | — | сома 100 мкм (миелин) | Yen & Chan 1993 (PMID 8264977) |
| AreaPostrema | Nav (TTX-S, пик −30) | 20.7 | гейт | нужна лит-ра | — | сома | Brain Res 1995 (PMID 8821750) |
| AreaPostrema | KDR (TEA) + IA (4-AP, ~75%) | 12/7.5 | гейт | нужна лит-ра | — | сома | Brain Res 1995 |
| AreaPostrema | Ito (fast/slow) | ~4–8 | гейт | нужна лит-ра | — | сома (late/interrupted spiking) | Brain Res Bull 2003 |
| AreaPostrema | Ih (HCN, ~62%) | 1–1.5 | V-завис. | нужна лит-ра | — | сома (пейсмекер, Erev −36) | Mitoh (Okayama) |
| AreaPostrema | Kir (Ba²⁺-чувств.) | 30–40 | ~1 | нужна лит-ра | — | сома | Brain Res 1995 |

### Пассивные параметры и морфология (RMP / R_in / площадь)
| Тип | RMP (мВ) | R_in | Площадь / морфология | Источник |
|---|---|---|---|---|
| PreBotzinger | −46 ± 1 | 340 МΩ | сома ~319 мкм²; Cm 69 пФ; дендриты r 0.4–0.8 мкм, l 1.4–2.1 мм | Hayes 2017; Del Negro 2002; Butera (Cm 21 пФ → 2100 мкм²) |
| RedNucleus | −60…−65 | ~4 МΩ (in vivo), выше в срезах | крупные руброспинальные, миелин; спайк ~71 мВ / 0.88 мс | J Physiol 1985; Al 1989 |
| NTS_Viscerosensory | −55…−65 | 300–700 МΩ | средние сома, 2–4 дендрита | Tell & Jean 1993 |
| PPTg_Cholinergic | −60…−65 | высокое (short-dur) / низкое (long-dur) | ACh, тонический пейсмекер | Takakusaki 1997 |
| MLR_Cuneiform | −69 (5-HT MLR) | 1410 МΩ | ~5.1 первичн. дендритов, l ~1407 мкм; Cm 36 пФ | Sci Rep 2025; Exp Brain Res 2019 |
| Parabrachial | −60…−65 | нужна лит-ра | глутамат (+CGRP) | Christen 1990 |
| PAG_Neuron | −55…−65 | 300–700 МΩ | гетерогенно GABA+глутамат | Dougalis 2017 |
| RVM_ONCell/OFFCell | −55…−60 | нужна лит-ра | FS (~0.27 мс) / SS (~0.44 мс) спайки | Zagon 1997 |
| TrigeminalMesencephalic | −55…−60 | нужна лит-ра | первично-сенсорный, крупный миелин | J Neurosci 2006 |
| TrigeminalMotor_MN | −60…−70 | обратно пропорц. соме | TASK1 сома / TASK3 дендриты | Saito |
| Pontine_Reticular | деполяр. в REM | нужна лит-ра | LTS + Ih + A-ток | Greene 1986; Brown 2006 |
| GigantocellularReticular | −22.9 ± 0.6 (очень деполяр.) | 3.51 / 77.5 МΩ (2 кластера) | τ 0.56 / 1.27 мс; сома 100 мкм | Yen & Chan 1993 |
| AreaPostrema | −55 (культура) / −54 (диссоц.) | 214 МΩ (культура) / 3.1 ГΩ (диссоц.) | CVO, нет ГЭБ | Brain Res 1995; Adrenomedullin 2003 |

**Локализация (градиенты):** TASK1 → сома / TASK3 → дендриты (TMN); A-ток Kv4 → сома-дендрит (NTS, PPTg, AP); Ih/HCN → сома-дендрит (саг/пейсмекер); T-Ca → сома-дендрит (LTS/rebound); NaP → диффузно (пейсмекер PreBötC/PAG/Mes V); KNa Slack → сома (терминация вдоха); CAN (TRPM4/5) → через глобальный Ca²⁺.

**Главные находки:**
1. **PreBötC = тот же прототип, что головастиковый RESP_CPG** (NaP γ=20.7/Po=0.016 + CAN TRPM4/5 + KNa Slack γ=180 + TASK). Butera 1999 даёт эталонные G: gNa/gK/gNaP/gLeak = 28/11.2/2.8/2.8 нС (Cm 21 пФ).
2. **PAG пейсмейкер зависит от NaP** (Ih/IA/Ca не обязательны) — Dougalis 2017.
3. **TrigeminalMesencephalic** = NaP + resurgent Na (Nav1.6) + Ih HCN1/2 + Kv1 (4-AP) — НЕ чистый T-Ca.
4. **TrigeminalMotor_MN** TASK (не K2P TREK-1) задаёт R_in/рекрутинг; + SK/BK + Ih + HVA Ca.
5. **AreaPostrema** = Nav(TTX-S) + KDR + IA + Ito + Ih(62%, пейсмекер) + Kir.
6. **Pontine_Reticular** LTS (T-Ca) инактивируется деполяризацией в REM; **Gigantocellular** — крайне деполяризованный покой (−23 мВ) → Na-инактивация.

### Новые источники
- Butera RJ, Rinzel J, Smith JC (1999) J Neurophysiol 82:382 (PMID 10400966) — gNa/gK/gNaP/gLeak.
- Greene RW, Haas HL (1986) Science 234:738 (PMID 3775364) — pontine LTS.
- Brown RE et al. (2006) Neuroscience — SubC Ih/LTS.
- Gerber U et al. (1989) J Physiol — mPRF repetitive firing.
- Yen CT, Chan SHH (1993) Neurosci Lett 159:5 (PMID 8264977) — NRGC passive.
- «Ionic conductance … guinea-pig red nucleus» (1985) J Physiol 362:161 (PMID 2410599).
- Al (1989) — red nucleus rat slices (rebound/Ih).
- Tell F, Jean A (1993) J Neurophysiol 70:2379 (PMID 7509858) — NTS NMDA oscillations.
- Saliba L — Kv4 A-currents NTS.
- PMC11192486 — Kv2 NTS.
- Takakusaki K et al. (1997) Neuroscience — PPTg cholinergic Type I/II.
- PMC6503533 — PPT cholinergic KCNQ4.
- Dougalis AG et al. (2017) J Comput Neurosci 41:305 (PMID 28367595) — vlPAG/DRN dopamine.
- Zagon A et al. (1997) — RVM FS/SS in vivo.
- Zhang L et al. (2006) — RVM serotonergic heterogeneity.
- Wu N et al. (2005) J Neurophysiol — Mes V NaP V½ −58.7.
- Hsiao CF et al. (2006) J Neurosci 26:3412 (PMID 16571748) — Mes V Na currents.
- Saito M — trigeminal motor TASK recruitment.
- Hsiao CF et al. (1997) J Neurophysiol 77:2910 (PMID 9212246) — 5-HT TMN.
- Christen M et al. (1990) — lateral parabrachial K conductance.
- Funahashi M et al. (1995) Brain Res 705:199 (PMID 8821750) — area postrema membrane.
- Mitoh Y — area postrema Ih pacemaker (Okayama).
- Sci Rep 2025 (PMC12639178) — precuneiform/CnF MLR properties.
- Koizumi & Smith 2008 J Neurosci 28:1773 (PMID 18305140) — PreBötC K+-dominated leak (TASK-like): gLeakK 2.0–7.0 нС (burst 2.0–3.0 нС), gNaP 2.0–4.4 нС.
- Purvis, Smith, Koizumi, Butera 2007 J Neurophysiol 97:1515 (PMID 17167061) — 12-кан. PreBötC модель: gNaP 2.5 нС, gTonic 0.2 нС, E_L −57.5 мВ (CellML/FieldML `purvis_smith_koizumi_butera_2007.cellml`; gCAN/gNaK/gKCa — вытащить из файла модели).
- Dougalis 2017 ModelDB 226010 — vlPAG/DRN dopamine (gNaP/gH/gCaT/gA — значения в .mod файлах, G не в абстракте).
- Penington & Tuckwell 2012 Brain Res 1449:60 (PMID 22410293) — dorsal raphe I_A ĝ = 20.5 нС (whole-cell).
- Tuckwell & Penington 2014 Prog Neurobiol 118:59 (PMID 24784445) — dorsal raphe модель (Na/KDR/IA/IM/T/L/N/SK/BK/IH/leak; G не в абстракте).

---

## Дополнение 2: черепные МН + слух/вестибулярные (агент, 2026-09-02)

γ/Po берём из таблицы выше; плотность = `G·10/(γ·Po)` (для вольт-зависимых Po=гейт → density=G·10/γ).

### Черепные мотонейроны (Oculomotor/Trochlear/Abducens/Facial/Vagus(NA)/Accessory/Hypoglossal)

Канон — гипоглоссальный MN (наиболее изучен): **Nav1.6 (fast+persistent), Kv1/Kv2 (KDR),
Kv4 (A), BK (fAHP), SK2 (mAHP), Cav1.3 (дендритный PIC), Cav2.2/Cav2.1 (пресинапс), HCN (Ih)**.
Экстраокулярные (Oculomotor/Trochlear/Abducens) — самые быстрые: **Kv1.1 + Kv3.1b** (fast-firing).

| Тип | Канал | γ | Po | G (мСм/см²) | density (мкм⁻²) | Локализация | Источник |
|---|---|---|---|---|---|---|---|
| Hypoglossal MN | Nav1.6 transient | 20.7 | гейт | ~17.5 (сома) | ~8.5 | AIS > сома > дендрит | Purvis & Butera 2005 (gNa 0.7 µS, C=0.04 µF) |
| Hypoglossal MN | NaP (Nav1.6) | 20.7 | 0.016 | AIS 11–15; сома 0.44; дендр. 0.015 | AIS ~330–450; сома ~13; дендр. ~0.45 | AIS/холмик >> сома > дендрит | PMC3289480 (Powers & Binder модель) |
| Hypoglossal MN | KDR (Kv1/Kv2) | 12 | гейт | ~32.5 (сома) | ~27 | сома/прокс. дендрит | Purvis & Butera 2005 (gKdr 1.3 µS) |
| Hypoglossal MN | A-type (Kv4.2) | 5 | гейт | ~25 | ~33–50 | дендриты (↑дист.) | Purvis & Butera 2005 (gKA 1.0 µS) |
| Hypoglossal MN | SK2 (mAHP) | 10 | Ca²⁺ | сома 7.6; дендр. 0.04 | ~7.6 | сома | PMC3289480; Purvis & Butera (gSK 0.3 µS → 7.5) |
| Hypoglossal MN | BK (fAHP) | 250 | Ca²⁺/V | в составе KCa | нужна лит-ра | сома | McLarnon 1995 |
| Hypoglossal MN | HCN (Ih) | 1 | гейт | сома 0.09–0.175 | ~0.9–1.75 | сома/прокс. дендрит | PMC3289480; Bayliss 1994 |
| Hypoglossal MN | Cav1.3 (L, PIC) | 25 | m²h | дендр. 0.16 | ~0.064 | дендриты (прокс.-сред.) | PMC3289480; Powers & Binder 2001 |
| Hypoglossal MN | Cav3.x (T) | 7.2 | m·h | ~2.5 | ~3.5 | сома/дендрит | Purvis & Butera 2005 (gT 0.1 µS) |
| Hypoglossal MN | Cav2.2 (N) | 2.76 (Ba 12–22) | m²h | ~1.25 | — | пресинапс (NMJ) | Purvis & Butera 2005 (gN 0.05 µS) |
| Hypoglossal MN | Cav2.1 (P/Q) | 15 | m²h | ~1.25 | ~0.83 | пресинапс (NMJ) | Purvis & Butera 2005 (gP 0.05 µS) |
| Hypoglossal MN | leak (K2P) | 100 | 0.012 | ~0.125 | ~1.0 | диффузно | Purvis & Butera 2005 (gLeak 0.005 µS) |
| Extraocular MN | Kv3.1b | 27 | гейт | нужна лит-ра | нужна лит-ра | сома/AIS (fast-firing) | Mayadali 2021 Brain Struct Funct (PMID 38957435) |
| Extraocular MN | Kv1.1 | 12 | гейт | нужна лит-ра | нужна лит-ра | сома/AIS | Mayadali 2021 |
| Facial MN | SK (sAHP) | 10 | Ca²⁺ | нужна лит-ра | нужна лит-ра | сома | Magariños-Ascone 1999; in situ SK1/2/3 |
| Facial MN | BK (fAHP) | 250 | Ca²⁺/V | нужна лит-ра | нужна лит-ра | сома | McLarnon 1995 |

> ⚠️ Purvis & Butera нормированы: C_m=0.04 µF при 1 µF/см² → экв. площадь ~4000 мкм².
> gSK 7.5 и gH 0.125 совпадают с независимой моделью PMC3289480 (7.6 и 0.09–0.175).

**Пассивные (Hypoglossal, взрослая крыса):** RMP **−70.6 ± 0.8 мВ** (Bayliss 1994; Haddad −80 ± 2),
R_in **12.2 ± 0.9 МОм** (взросл.; неонат. 44.5), AHP 6.4 мВ / τ 31 мс; τ_m ↓~4× за 2 недели. Порог ~ −55…−50.
**Abducens (кошка, Grantyn 1978):** R_in **1.2–4.8 МОм (ср. 2.3)**, rheobase 4.7 нА,
порог-деполяризация 10.5 мВ, дендрит:сома = 7.6, скорость аксона 25–84 м/с (миелин).

### Парасимпатические преганглионарные (DorsalMotorVagus / SupSalivatory / InfSalivatory / EdingerWestphal)

DMV — канон: **SK-опосредованный mAHP** (апамин-чувствительный, τ~155 мс), медленный AHP =
SK + I_A (Kv4); низко- и высокопороговые Ca²⁺-токи; низкая частота разряда (1–10 Гц).

| Тип | Канал | γ | Po | G | density | Локализация | Источник |
|---|---|---|---|---|---|---|---|
| DMV | SK2 (mAHP) | 10 | Ca²⁺ | нужна лит-ра (эталон мотонейрон. SK/mAHP gKCa=50 мС/см² — ModelDB 267176, НЕ DMV-специфично) | нужна лит-ра | сома | Sah & McLachlan 1992 (J Neurophysiol 68:1834) |
| DMV | I_A (Kv4) | 5 | гейт | нужна лит-ра | нужна лит-ра | сома/дендрит | Sah 1992 (медл. AHP) |
| DMV | Cav (HVA+LVA) | — | — | нужна лит-ра | нужна лит-ра | сома/дендрит | Sah 1992 (низко-/высокопорог. Ca-спайки) |

> ⚠️ В коде парасимпатические преганглионарные (DMV/Salivatory/EW) «голые» — нет SK/I_A/Ca →
> не воспроизводится медленный mAHP и низкочастотный разряд.

### Вестибулярные (Vestibular = medial vestibular nucleus, MVN)

Serafin 1991 (Exp Brain Res 84:426): **Type A** (~32%) — широкий ПД + одиночный глубокий AHP,
транзиентная A-подобная ректификация (4-AP-резистентная), малые HVA Ca-спайки; **Type B** (~47%) —
тонкий ПД + быстрый+медленный AHP, большие HVA Ca-спайки, Ca-плато, persistent Na, субпопуляция
с LVA (T-type) Ca-спайками → burst; **Type C** (~20%).

Quadroni & Knöpfel 1994 (J Neurophysiol 72:1911, ModelDB 53876): 9 проводимостей — gNa, gNap,
gCa(LVA), gCa(HVA), gK(fast), gK(slow), gK(A), gH, gK(AHP). Burst = плотность gCa(LVA); плато =
плотность gNap; ритмический burst Type B = дендритный NMDA или ↓gK(AHP).

| Тип | Канал | γ | Po | G | density | Локализация | Источник |
|---|---|---|---|---|---|---|---|
| MVN Type A/B | Cav3.x (T, LVA) | 7.2 | m·h | Type B 0.166 (Type A — нет) | 0.23 (B) | сома/дендрит | Serafin 1991; Quadroni & Knöpfel 1994 (gCa LVA, ModelDB 53876) |
| MVN Type A/B | HCN (Ih) | 1 | гейт | 0.11 (A) / 0.066 (B) | 1.1 (A) / 0.66 (B) | сома/дендрит | Quadroni & Knöpfel 1994 (gH) |
| MVN Type A/B | Kv1 (A-type, 4-AP) | 12 | гейт | 1.829 (A) / 0.755 (B) | 1.5 (A) / 0.63 (B) | сома/дендрит | Quadroni & Knöpfel 1994 (gK(A)); Serafin 1991 |
| MVN Type A/B | BK/SK (AHP) | 250/10 | Ca²⁺ | 2.167 (A) / 2.716 (B) | — (gK(AHP), см. таблицу ниже) | сома | Serafin 1991; Quadroni & Knöpfel 1994 (gK(AHP)) |
| MVN Type B | NaP | 20.7 | 0.016 | 0.033 (A) / 0.0236 (B) | 1.0 (A) / 0.71 (B) | сома | Serafin 1991; Quadroni & Knöpfel 1994 (gNap) |

### Слуховой ствол

**CochlearNucleus_Bushy** (Rothman & Manis 2003): gNa 75, gKHT(Kv3.1) 11, gKLT(Kv1) 0–15 мСм/см²;
состав Na + IHT(Kv3.1) + ILT(Kv1 low-threshold) + IA(Kv4) + Ih(HCN) (+KCNQ в cnmodel).
Быстрая τ_m → точное phase-lock (primarylike).

| Тип | Канал | γ | Po | G (мСм/см²) | density (мкм⁻²) | Локализация | Источник |
|---|---|---|---|---|---|---|---|
| Bushy | Nav1.6 | 20.7 | гейт | 75 | ~36 | AIS/аксон, сома | Rothman & Manis 2003 |
| Bushy | Kv3.1 (IHT) | 27 | гейт | 11 | ~4.1 | сома (реполяр.) | Rothman & Manis 2003; Wang 1998 |
| Bushy | Kv1 (ILT) | 12 | гейт | 0–15 | ~0–12.5 | сома | Rothman & Manis 2003 (phasic) |
| Bushy | HCN (Ih) | 1 | гейт | gh (abs 0.5–20 нС) | нужна лит-ра | сома (ко-экспрессия с ILT) | Rothman & Manis 2003 |
| Bushy | KCNQ (Kv7/M) | 9 | гейт | нужна лит-ра | нужна лит-ра | сома | cnmodel (pbmanis) — роль уточнить |

**MNTB_Principal** (Wang 1998 J Physiol 509:183): **Kv3.1 (IHT, TEA 1 мМ, DTX-нечувств.)** →
fast-spiking (следит до 200 Гц); **низкопороговый Kv1 (ILT, DTX-чувств.)** → предотвращает
повторные спайки. Модель (C=0.01 нФ): gNa 50, gKLT 2, gKv3.1 15, gL 0.2 мСм/см².

| Тип | Канал | γ | Po | G (мСм/см²) | density (мкм⁻²) | Локализация | Источник |
|---|---|---|---|---|---|---|---|
| MNTB | Nav1.6 | 20.7 | гейт | 50 | ~24 | AIS/сома (calyx-постсинапс) | Wang 1998; модель |
| MNTB | Kv3.1 | 27 | гейт | 15 | ~5.6 | сома (calyx) | Wang 1998 |
| MNTB | Kv1 (ILT) | 12 | гейт | 2 | ~1.7 | сома | Wang 1998 (DTX-чувств.) |
| MNTB | leak | 100 | 0.012 | 0.2 | ~1.7 | диффузно | модель |

**SuperiorOlive_MSO** (Scott/Mathews 2005, Mathews 2010): **Kv1.1 (IKLT)** ↑после открытия слуха,
градиент затухает вдоль дендрита с λ=74 мкм; **Ih/HCN** с тем же градиентом; перисоматический
Nav. Khurana 2011: Ih+IKLT взаимодействуют, ↑R_in до 60% → точное ITD.

| Тип | Канал | γ | Po | G | density | Локализация (градиент) | Источник |
|---|---|---|---|---|---|---|---|
| MSO | Kv1.1 (IKLT) | 12 | гейт | нужна лит-ра | нужна лит-ра | дендриты, λ=74 мкм (↓дист.) | Mathews 2010; Scott 2005 |
| MSO | HCN (Ih) | 1 | гейт | нужна лит-ра | нужна лит-ра | дендриты (тот же градиент) | Khurana 2011 |
| MSO | Nav (перисоматич.) | 20.7 | гейт | нужна лит-ра | нужна лит-ра | сома/AIS | Scott 2005 |

**InferiorColliculus**: HCN (Ih, sag), Kv3.1 (TEA-чувств., акт. > −20 мВ) + Kv3.3 + Kv4,
T-type Ca (rebound, субпопуляция), HVA Ca (L 21% / N 29% / P 22% / Q 16% / R 12%).
Деполяризованный RMP, низкий R_in. Типы: sustained/onset/pause-build/rebound.

**SpiralGanglion**: **Nav1.6** (сома), **Kv7.2/7.3/7.4 (KCNQ)**, Kv1.1/1.2/1.4/1.6, Kv3.x,
Kv4.2/4.3, HCN. RMP **−62 ± 9** (Szabó 2002) / **−67.3** (Santos-Sacchi 1993); R_in **170 ± 8 МОм**
(P5–8). Kv1 → RMP+порог; HCN → только RMP. Type I миелинизирован (сома — loose myelin,
центральный аксон — компактный), Type II немиелинизирован.

**DStellate_Cochlear** (VCN multipolar/chop): gNa/gKLT/gKHT — по Rothman & Manis 2003 Type I
(stellate) — ниже gKLT чем bushy; нужна лит-ра для точных G.

### Площади / пассивные параметры (типовые)
| Тип | Сома (d) | RMP | R_in | Порог | Миелин |
|---|---|---|---|---|---|
| Hypoglossal/краниальные MN | 30–50 мкм (экв. 4000 мкм²) | −70 | 12–21 МОм (гипоглосс.); 2.3 (abducens) | ~−50 | да (25–84 м/с) |
| Extraocular MN | 25–40 мкм | −60…−65 | 1–5 МОм | низкий | да |
| DMV/Salivatory/EW | 20–30 мкм | −55…−65 | высокий | ~−45 | да (тонкий) |
| MVN | 20–35 мкм | −55…−65 | ~50–100 МОм | ~−50 | да (проекции) |
| MNTB principal | 15–20 мкм | −60…−65 | ~50–100 МОм | ~−50 | да |
| Bushy | 15–25 мкм | −60…−65 | низкий | ~−50 | да |
| MSO | 15–20 мкм | −55…−60 | низкий | ~−50 | да |
| IC | 15–25 мкм | деполяр. (−55…−60) | низкий | ~−50 | да/нет |
| SpiralGanglion type I | 25–30 мкм | −62…−67 | ~170 МОм (неонат.) | ~−45 | да (type I) |

### Локализация и градиенты (сводка)
- **Nav1.6** → AIS/перехваты Ранвье у всех слуховых/моторных (плотность AIS на 1–2 порядка выше сомы).
- **Kv3.1** → сома (fast-spiking: extraocular MN, MNTB, bushy, IC) — НЕ дендриты.
- **Kv1 (low-threshold)** → сома/дендриты: MSO дендритный градиент ↓ (λ=74 мкм); MNTB/bushy сома.
- **Kv4 (A)** → дендриты ↑дистально (гипоглоссальный, facial, DMV).
- **HCN** → сома/прокс. дендрит (гипоглосс., MVN); MSO — дендритный градиент.
- **T-Ca (Cav3)** → сома/дендрит (MVN Type B burst, IC rebound).
- **Cav1.3** → дендриты (PIC мотонейронов).
- **Cav2.1/Cav2.2** → пресинаптический бутон (NMJ у MN, calyx у MNTB-входов, лента у SGN).
- **BK/SK** → сома (AHP); **KCNQ/K2P** → диффузно.

### Новые источники (дополнение 2)
- Purvis & Butera 2005 J Neurophysiol 93:2005 (PMID 15653786) — гипоглоссальный MN модель (gNa/Kdr/KA/SK/H/Ca).
- Powers & Binder (модель PMC3289480) — NaP/HCN/KCa/Cav1.3 плотности.
- Bayliss, Viana, Bellingham, Berger 1994 J Neurophysiol — RMP/R_in гипоглосс.
- Grantyn & Grantyn 1978 Exp Brain Res 31 — abducens R_in/порог.
- Mayadali 2021 Brain Struct Funct (PMID 38957435) — Kv1.1/Kv3.1b/HCN human abducens/trochlear.
- Magariños-Ascone 1999 — facial MN типы I/II.
- Sah & McLachlan 1992 J Neurophysiol 68:1834 — DMV SK/mAHP.
- Serafin 1991 Exp Brain Res 84:426 — MVN Type A/B.
- Quadroni & Knöpfel 1994 J Neurophysiol 72:1911 — MVN модель.
- Rothman & Manis 2003 J Neurophysiol 89 — bushy/VCN модель (gNa 75, gKHT 11, gKLT 0–15).
- Mathews 2010; Scott 2005; Khurana 2011 — MSO Kv1/HCN.
- Szabó 2002; Santos-Sacchi 1993 — SGN RMP/R_in.
- McLarnon 1995 Prog Neurobiol (PMID 8787033) — мотонейронные K-токи: BK γ=240 пС (140 мМ K⁺) / 110 пС (5 мМ K⁺) — подтверждает BK γ≈250 из [[BK_104]].
- Powers et al. 2012 ModelDB 267176 (KAHP.mod) — мотонейронный mAHP/SK: gKCa,max = 0.05 С/см² = 50 мС/см² (эталон, НЕ DMV-специфично).

---

## MVN (Vestibular) — ЛИТЕРАТУРНЫЕ ПРОВОДИМОСТИ НАЙДЕНЫ (Quadroni & Knöpfel 1994, ModelDB 53876)

gbar в **С/см² = мС/см²** (сома, подогнаны авторами; компартмент-специфичны — сома > дендриты):

| Канал | Type A | Type B | Механизм |
|---|---|---|---|
| gNa | **20.17** | **43.0** | `na` |
| gNap | 0.033 | 0.0236 | `nap` |
| gCa(LVA, T) | **нет** | **0.166** | `lva` |
| gCa(HVA) | 1.11 | 2.385 | `hva` |
| gK(fast) | 24.07 (n³) | 37.53 (n²) | `akfast`/`bkfast` |
| gK(slow) | 0.298 | 0.519 | `kslow` |
| gK(A) | 1.829 | 0.755 | `a` |
| gH (Ih) | 0.11 | 0.066 | `h` |
| gK(AHP, SK/BK) | 2.167 | 2.716 | `ahp` |

**γ/Po (из modifier_refs):** Nav1.6 γ=20.7, NaP γ=20.7/Po=0.016, T-Ca γ=7.2, HVA Ca γ=25, KDR γ=11, Kv4 γ=5, HCN γ=1, SK γ=10, BK γ=250.

**density = G·10/(γ·Po)** (или G·10/γ для гейтированных; Po в open_prob). Type A/B = подтипы Serafin 1991 (Type A = широкий ПД, Type B = двойной AHP + burst).

### SGN (спиральный ганглий) — данные узла Ранвье (Negm & Bruce 2014, JARO 2017 PMC5352613)
Стохастическая модель узла: γNa=25.69 пС (N=1000), γK=50 пС (N=166), γKLT=13 пС (N=166), γH=13 пС (N=100), Cm=0.0714 пФ. ⚠️ Это УЗЕЛ Ранвье (не сома) → density = N/площадь узла (~4.7 мкм²): Na~213, Kv~35, KvLT~35, HCN~21 мкм⁻² (только для AIS/узла, НЕ сома).
