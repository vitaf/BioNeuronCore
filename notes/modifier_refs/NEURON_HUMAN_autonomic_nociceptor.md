# NEURON_HUMAN — Вегетативные + ноцицепторы + прочие (аудит, группа 13-15)

**Источник:** агент (WebSearch).

## Вегетативные
- **Preganglionic_Autonomic** — ACh; B-волокно, миелинизировано.
- **Postganglionic_Sympathetic** — норадреналин; N-тип Ca (Cav2.2); RMP −55.
- **Postganglionic_Parasympathetic** — ACh.
- **Intracardiac_Neuron** — ACh (вагальный).

## Энтеральные
- **Enteric_Myenteric** — ACh + субстанция P (S-нейроны), NOS/VIP (тормозные).
- **Enteric_Submucosal** — ACh + VIP.
- **IPAN_Enteric** — Dogiel II, AH-физиология (Ca-зависимые K, BK/IK); CGRP/SP.

## Ноцицепторы / TRP-сенсоры
- **C_Fiber_Nociceptor** — глутамат + CGRP/SP; **Nav1.7/1.8/1.9, TRPV1**; немиелинизировано; RMP −60.
- **A_Delta_Nociceptor** — глутамат; Nav1.7/1.8; тонко миелинизировано.
- **TRPM8_ColdSensor** — TRPM8 (<25°C).
- **TRPV3_WarmSensor** — TRPV3/TRPV4 (31–39°C).
- **TRPA1_Nociceptor** — TRPA1 (<17°C, аллилизотиоцианат).
- **Itch_C_Fiber** — GRP/MrgprA3, гистамин-H1.

## Волосковые клетки
- **InnerHairCell** — MET (TMC1), **Cav1.3** (ribbon), BK; глутамат; RMP −60.
- **OuterHairCell** — MET, престин, **KCNQ4 (Kv7.4)**; RMP −70.
- **VestibularHairCell_TypeI** — KCNQ (K_L); **TypeII** — BK.

## Прочие
- **CarotidBody_Glomus** — гипоксия → TASK-1/3 закрытие → Cav → дофамин/ATP/ACh.
- **TasteReceptorCell** — тип II ATP (CALHM1/3); тип III 5-HT; солёное ENaC.
- **PancreaticBetaCell** — **K_ATP (Kir6.2/SUR1)**, Cav1.2/1.3, Nav1.7; RMP −70.
- **PancreaticAlphaCell** — T-type Ca (Cav3.1/3.2), Nav1.3.
- **EnterochromaffinCell** — 5-HT; Piezo2.
- **MedialHabenula** — ACh + субстанция P.
- **LateralHabenula** — глутамат; HCN, T-type Ca.
- **MedialSeptum** — ACh + GABA + глутамат; theta-пейсмейкер (HCN/Kv7).
- **LateralSeptum** — GABA.
- **BNST** — GABA (+часть глутамат); CRF.
- **SubfornicalOrgan** — AT₁R + Naₓ/TRPV1 → жажда.
- **OVLT** — осмосенсор (TRPV1/TRPV4, Naₓ).

## Ключевой вывод
TRP-типы стоят как `MOD_PHYSICAL_SENSOR` (суррогат) — по No Surrogates это конкретные неселективные катионные каналы (TRPM8/TRPV3/TRPA1/TRPV1), кандидаты на отдельные модификаторы.

## Источники
- Cox 2006 Nature 444:894 (PMID 17167479) — SCN9A.
- Caterina 1997 Nature 389:816 (PMID 9349813) — TRPV1.
- McKemy 2002 Nature 416:52 (PMID 11882888) — TRPM8.
- Ashcroft 1984 Nature 312:446 (PMID 6090963) — K_ATP.
- Brandt & Moser 2005 J Neurosci 25:11577 — Cav1.3 IHC.
- Kim 2009 J Physiol 587:2963 — TASK-1/3 каротидное тело.

---

## Дополнение: литературные плотности γ/Po/G (агент, 2026-09-02)

**Формула пересчёта (как в коде):** `density [мкм⁻²] = G [мСм/см²] · 10 / (γ [пСм] · Po)`.
Po/гейтинг — в open_prob, НЕ в density. Где G не найдена в литературе — честно «нужна лит-ра» (не выдумывать).

### Одноканальные проводимости γ и Po (литература)
| Канал | γ (пСм) | Po | Источник |
|---|---|---|---|
| Nav1.6 (эталон Nav) | 20.7 | пик 0.45 / persist. 0.016 | Chatelier 2010; [[NAV16_141]] |
| Nav1.7/1.8/1.9 | ~20 (Nav1.7); **Nav1.8 = 11 ± 1** (DRG, TTX-R) | гейт m³h | Nav1.8: J Neurosci 2018 (Hoffmann); TTX-R одиночн. канал |
| TRPV1 | **~90–100** (рекомб., +80 мВ); нативный DRG **~20–46** | 0.03 (0.1 µM капсаицин) → 0.70 (10 µM) | PNAS 2000 (PMID 11106397) |
| TRPM8 | **~70–75** (HEK); нативный DRG **~22** | растёт при охлаждении (NPo) | McKemy 2002; J Neurosci 2002 |
| TRPA1 | **~98** (физ. градиенты); E920A 172/99 | — | Harvard/Dash; Earley 2012 B J Pharmacol |
| TRPV3 | 172/22 (или 197 @ +40..80, 48 @ −V) | — | Physiol Soc JP (таблица TRP) |
| TRPV4 | ~60–100 (+60 мВ), 88/310 | — | Physiol Soc JP (таблица TRP) |
| Cav2.2 (N-type) | **2.76** (2 мМ Ca²⁺, физ.) / 16.4 (10 мМ Ba²⁺) | — | OATD Toronto; N>L>T (Biophys J 2010) |
| Cav1.3 (L) | **18** (5 мМ Ba²⁺, IHC мышь) | Po max ~0.23; акт. порог ~−70 мВ | Zampini 2010 J Physiol 588:3151 (PMC2817446) |
| BK (KCa1.1) | 250 | Ca²⁺/V-зав. | [[BK_104]] |
| SK2 (KCa2.2) | ~10 | Ca²⁺-зав. | [[SK_103]] |
| K2P TREK-1 (KCNK2) | **112** (150 мМ KCl, DRG нативный) | **0.012** (покой); ~12% фонового K DRG | [[K2P_107]]; Kang & Kim 2006 |
| TASK-1 / TASK-3 | **14** / **32**; гетеромер TASK-1/3 ~32 (42 в низк. Mg²⁺) | ~1 (leak) | Kim 2009 J Physiol 587:2963 |
| KCNQ2 / KCNQ3 / KCNQ2/3 | 18 / 7 / 8–22 (~16 шум) | PIP₂-зав. | J Neurosci (M-current) |
| KCNQ4 (Kv7.4) | **2.31** | NPo 0.61 (cell-attached, −50 мВ) | Biophys J 2014 (HEK293) |
| ENaC | 4–7 (высокоселект. 4–5; неселект. 21) | — | PMC1458778 |
| Kv1.1 | 9–14 | гейт | [[KV_DR_123]] |
| HCN | ~1 (HCN2 ~1.5) | V-зав. | [[HCN1-4_129-132]] |

### Таблица «тип → канал → γ/Po/G → density → локализация → источник»

| Тип | Канал | γ (пСм) | Po | G (мСм/см²) | density (мкм⁻²) | Локализация | Источник |
|---|---|---|---|---|---|---|---|
| Preganglionic_Autonomic | Nav (сем.) + Kv1.1 + K2P-leak | 20 / 9–14 / 100 | гейт / гейт / 0.012 | нужна лит-ра | нужна лит-ра | сома+аксон (B-волокно, миелин) | — |
| Postganglionic_Sympathetic | **Cav2.2 (N)** | 2.76 (Ca²⁺) | гейт | **~0.28** (stellate Ca-ток 34 pA/pF, N-тип) / до ~0.9 (общий Ca 108 pA/pF) | нужна лит-ра | сома/пресинапс (выброс NA) | J Auton Nerv Syst 1998 (stellate); J Neurosci 2016 36:8562 |
| Postganglionic_Sympathetic | **Kv7 (M-current KCNQ2/3)** | ~16 | PIP₂ | **~0.11** (M-tail 2.8 pA/pF @ −60 мВ, ΔV=25) | нужна лит-ра | сома/дендрит | J Gen Physiol 2008 131:575 (SCG) |
| Postganglionic_Sympathetic | SK2 (mAHP) | 10 | Ca²⁺-зав. | нужна лит-ра | нужна лит-ра | сома | [[SK_103]] |
| Enteric_Myenteric (S) | Nav + Kv1.1 + BK | 20 / 9–14 / 250 | гейт | нужна лит-ра | нужна лит-ра | сома | — |
| Enteric_Myenteric (AH) | **BK + IK/SK** (slow AHP) | 250 / 10 | Ca²⁺-зав. | нужна лит-ра | нужна лит-ра | сома | Furness 2000 (AH-нейроны) |
| IPAN_Enteric (Dogiel II) | **Nav1.9 (persist. Na) + BK/SK** | ~20 / 250 / 10 | persist. | Nav1.9 **~0.02** (1.88 pA/pF @ −50 мВ); BK/SK нужна лит-ра | нужна лит-ра | сома (AH-физиология) | Rugiero 2003 (Nav1.9 AH); J Gen Physiol 2008 131:211 |
| Intracardiac_Neuron | Nav + Kv7 (M) + BK | 20 / ~16 / 250 | гейт | нужна лит-ра | нужна лит-ра | сома (вагальный постсинапс) | — |
| LateralHabenula | **HCN + Cav3 (T)** | 1 / 9.5 | V-зав. / 0.1–0.3 | нужна лит-ра | нужна лит-ра | сома-дендрит | — |
| C_Fiber_Nociceptor | **Nav1.7 (TTX-S)** | ~20 | гейт m³h | **0.45** (сома, 27.2 pA/pF @ −10 мВ); **AIS 500** | нужна лит-ра | сома/аксон | Cox 2006 (SCN9A); PMC4822888 (DRG TTX-S) |
| C_Fiber_Nociceptor | **Nav1.8 (TTX-R)** | **11** | гейт (slow) | **0.95** (сома, 47.5 pA/pF TTX-R) | нужна лит-ра | сома+дистальный аксон (C-волокно) | J Neurosci 2018 (Nav1.8 C-fiber); bioRxiv 2018 (GM-CSF) |
| C_Fiber_Nociceptor | **Nav1.9 (persist., порог ~−60 мВ)** | ~20 | persist. | **0.019** (1.88 pA/pF @ −50 мВ, ΔV≈100) | нужна лит-ра | сома (IB4+ ↑) | Pain 2024 (PMID 38870211); J Gen Physiol 2008 131:211 |
| C_Fiber_Nociceptor | **TRPV1** | 20–46 (нативный) | 0.03–0.7 (капс.-зав.) | **0.42** (29.6 pA/pF капсаицин @ −70 мВ, ΔV≈70) | нужна лит-ра | периф. терминаль + сома | Caterina 1997; Masuoka 2017 Front Physiol 8:272 |
| C_Fiber_Nociceptor | **K2P TREK-1** | 100 | 0.012 | нужна лит-ра | нужна лит-ра | диффузно (leak, задаёт RMP) | [[K2P_107]]; Acosta 2014 (TREK2 C-fiber) |
| C_Fiber_Nociceptor | **Kv7 (M)** | ~16 | PIP₂ | **~0.1** (оценка по SCG M-current; DRG-специф. нужна лит-ра) | нужна лит-ра | сома (подавление возбудимости) | Abd-Elsayed 2019 (Kv7 боль); J Gen Physiol 2008 131:575 |
| A_Delta_Nociceptor | Nav1.7/1.8 | ~20 / 11 | гейт | **0.45 / 0.95** (сома); **AIS 500** | нужна лит-ра | сома+аксон (миелин Aδ) | PMC4822888; bioRxiv 2018 (GM-CSF) |
| TRPM8_ColdSensor | TRPM8 | 22 (натив.) | NPo ↑ при холоде | нужна лит-ра | нужна лит-ра | периф. терминаль | McKemy 2002 |
| TRPV3_WarmSensor | TRPV3/TRPV4 | 172/22; 60–100 | темп.-зав. | нужна лит-ра | нужна лит-ра | кожа (кератиноциты) + терминаль | J Biol Chem 2010 |
| TRPA1_Nociceptor | TRPA1 | 98 | — | нужна лит-ра | нужна лит-ра | периф. терминаль | Story 2003 Cell |
| Itch_C_Fiber | Nav + TRPV1 + H1-R | 20 / 20–46 | гейт | Nav 0.45 (AIS 500) / TRPV1 0.42 | нужна лит-ра | периф. терминаль (MrgprA3+) | — |
| TasteReceptorCell (тип II) | Nav (SCN2A/3A/9A) + KDR (KCNA5) | 20 / 9–14 | гейт | нужна лит-ра | нужна лит-ра | сома (рецепторные) | KEGG map04742 |
| TasteReceptorCell (тип II) | TRPM5 + CALHM1/3 (ATP-пора) | нужна лит-ра | — | нужна лит-ра | нужна лит-ра | апикальная/базальная | KEGG map04742 |
| TasteReceptorCell (тип III) | OTOP1 (H⁺) + **Kir2.1** + Nav | 30 (Kir2.1) | — | нужна лит-ра | нужна лит-ра | апикальная (кислое) + сома | KEGG map04742 |
| TasteReceptorCell (тип I, солёное) | **ENaC** + ROMK | 4–7 / (ROMK) | — | нужна лит-ра | нужна лит-ра | апикальная (Na⁺) | PMC1458778 |
| InnerHairCell | MET (TMC1) | — | градуальный | — | — | стереоцилии | — |
| InnerHairCell | **Cav1.3 (L)** | **18** | Po max 0.23, порог ~−70 мВ | **~0.18** (Ba-ток 181 пА / 8.5 пФ ≈ 21 pA/pF, ΔV≈120) | нужна лит-ра | базальный полюс/ribbon | Brandt & Moser 2005; Zampini 2010 J Physiol 588:3151 |
| InnerHairCell | **BK** | 250 | Ca²⁺/V-зав. | нужна лит-ра | нужна лит-ра | базальный полюс (после начала слуха) | Brandt 2003 J Neurosci |
| InnerHairCell | **KCNQ4 (Kv7.4)** | 2.31 | NPo 0.61 | нужна лит-ра | нужна лит-ра | базальный полюс (задаёт RMP) | Oliver 2003 J Neurosci |
| OuterHairCell | MET + престин (мотор) | — | градуальный | — | — | латеральная стенка/стереоцилии | — |
| OuterHairCell | **KCNQ4 (Kv7.4, I_K,n)** | 2.31 | NPo 0.61 | нужна лит-ра | нужна лит-ра | базальный полюс (I_K,n) | Marburg 2012 (дисс.) |
| VestibularHairCell_TypeI | **KCNQ (gK,L)** | ~2–4 | — | нужна лит-ра | нужна лит-ра | базальный полюс | Rennie 2001 (KCNQ4) |
| VestibularHairCell_TypeII | **BK** | 250 | Ca²⁺/V-зав. | нужна лит-ра | нужна лит-ра | базальный полюс | Rennie 2001 |
| CarotidBody_Glomus | **TASK-1/3 (O₂-сенсор)** | 14/32 (гетеромер ~32) | ~1 (гипоксия ↓ ~80%) | нужна лит-ра | нужна лит-ра | диффузно (leak, задаёт RMP) | Kim 2009; Buckler 2000 |
| CarotidBody_Glomus | **BK (maxi-K)** | 250 | Ca²⁺/O₂-зав. | нужна лит-ра | нужна лит-ра | сома | Peers 1991 |

### Миелин / площади / RMP
| Тип | Миелин | Аксон (d) | Сома (d) | RMP | Порог |
|---|---|---|---|---|---|
| A_Delta_Nociceptor | **тонко миелинизирован** | 1–5 мкм (5–30 м/с) | ~20–35 мкм | ~ −60…−70 мВ | ~ −40 мВ |
| C_Fiber_Nociceptor | **немиелинизирован** | 0.2–1.5 мкм (0.5–2 м/с) | ~15–30 мкм | **−60 мВ** (TREK2 ↓10 мВ у IB4+) | ~ −35 мВ |
| Preganglionic_Autonomic | B-волокно, миелин | 1–3 мкм (3–15 м/с) | ~20–30 мкм | −55 мВ | — |
| Postganglionic_Sympathetic | C-волокно, **немиелин** | 0.3–1 мкм | ~25–35 мкм | **−55 мВ** | — |
| InnerHairCell | — (градуальная) | — | груша ~10 мкм (r 5 мкм, area ~300 мкм²) | **−70 мВ** (мышь P5–10); in vivo −40…−55 | градуальная |
| OuterHairCell | — (градуальная) | — | цилиндр d 7–8 мкм, l 20–90 мкм (2πrl ~ 400–2200 мкм²) | **−70 мВ** | градуальная |
| CarotidBody_Glomus | — | — | ~10 мкм | ~ −50 мВ (нужна лит-ра) | — |
| TasteReceptorCell | — | — | веретено ~10 мкм | ~ −50…−60 мВ | — |

**Площади (2πrl):** C-волокно аксон r≈0.1–0.75 мкм; Aδ аксон r≈0.5–2.5 мкм; терминаль ноцицептора — микрометровые свободные окончания (без миелина даже у Aδ на самом конце).

### Главные расхождения (код vs биология)
1. **Nav1.7/1.8/1.9, TRPV1, Kv7** — в коде ноцицепторов НЕТ (только родовой Nav/K2P). Нужны отдельные модификаторы (Nav1.7 TTX-S, Nav1.8 TTX-R, Nav1.9 persistent, TRPV1 неселективный катионный).
2. **TRPM8/TRPV3/TRPA1** — стоят как `MOD_PHYSICAL_SENSOR` (суррогат); это конкретные неселективные катионные каналы (γ: TRPM8 70/22, TRPA1 98, TRPV3 172/22 пС).
3. **KCNQ4 (γ=2.31 пС)** в OHC/IHC и **TASK-1/3 (γ=14/32 пС)** в гломусе — ключевые leak, задают RMP; в коде замещены родовым K2P (γ=100 пС, Po=0.012 TREK-1) — неверная изоформа для этих клеток.
4. **Cav2.2 (N-type, γ=2.76 пС в физ. Ca²⁺)** в постганглионарных симпатических — в коде Cav-ток родовой.
5. **IHC/O HC/гломус/вкусовые — градуальные или слабо-спайковые**; не спайковые Na-клетки (см. [[NEURON_HAIR_CELL]]).

### Найденные G (агент, 2026-09-05) — этап 6

**Конверсия (код, Cm=1 мкФ/см²):** `G [мС/см²] = I [пА/пФ] / ΔV [мВ]`, где ΔV = V_test − E_rev. Найденные G (реальная литература, whole-cell плотности тока):

| Канал | I (пА/пФ) | ΔV (мВ) | **G (мС/см²)** | Локализация | Источник |
|---|---|---|---|---|---|
| Nav1.7 (TTX-S, DRG) | 27.2 @ −10 мВ | ~60 | **0.45** (сома); **AIS = 500** | сома / AIS | PMC4822888 |
| Nav1.8 (TTX-R, DRG) | 47.5 @ пик | ~50 | **0.95** (сома); AIS = 500 | сома+дист. аксон | bioRxiv 2018 (GM-CSF) |
| Nav1.9 (persist., DRG) | 1.88 @ −50 мВ | ~100 | **0.019** | сома | J Gen Physiol 2008 131:211 |
| TRPV1 (капсаицин, DRG) | 29.6 @ −70 мВ | ~70 | **0.42** | периф. терминаль+сома | Masuoka 2017 Front Physiol 8:272 |
| HCN (Ih, DRG C-волокно) | 5 @ −100 мВ | ~70 | **0.07** | сома-дендрит | Gao 2012 J Physiol 590:4691 |
| HCN (Ih, DRG Aδ) | 14 @ −100 мВ | ~70 | **0.2** | сома-дендрит | Gao 2012 J Physiol 590:4691 |
| HCN (Ih, DRG Aβ) | 21 @ −100 мВ | ~70 | **0.3** | сома-дендрит | Gao 2012 J Physiol 590:4691 |
| Cav2.2 (N, симпат.) | 34 (stellate) / 108 (общ. Ca) | ~120 | **~0.28 / ~0.9** | пресинапс (выброс NA) | J Auton Nerv Syst 1998; J Neurosci 2016 36:8562 |
| Kv7 (M-current, SCG) | 2.8 tail @ −60 мВ | 25 | **~0.11** | сома/дендрит | J Gen Physiol 2008 131:575 |
| Cav1.3 (L, IHC) | 21 (181 пА / 8.5 пФ) | ~120 | **~0.18** | базальный полюс/ribbon | Zampini 2010 J Physiol 588:3151 |

**Правила (этап 6):**
- **Nav на AIS = 500 мС/см²** (канон, Hu 2009 / Kole 2008). Сома-плотность Nav заметно ниже (0.45–0.95).
- **NALCN — НЕ на AIS** (распределён в соме/дендритах), отдельная плотность.
- **НЕ найдено (нужна лит-ра):** SK2 (mAHP) в симпатических; BK (maxi-K) в DRG/гломусе/IHC — только качественные данные (iberiotoxin-чувствит., >90% IB4+); TASK-1/3 в гломусе (γ=14/32 подтверждён, но нет пА/пФ); P2X3 (нет плотности); TRPM8/TRPA1/TRPV3/4 (нет плотности); TREK-1 (γ=112 пС, ~12% фонового K, но нет пА/пФ); Kv4/A-ток преганглионарных (γ/состав подтверждён, плотность нет); preganglionic Nav/Kv1.1 (нет специфич. плотности).

### Новые источники (2026-09-05)
- PMC4822888 (PLoS ONE 2016) — Nav1.7 главный TTX-S в малых DRG; TTX-S плотность 27.2 пА/пФ.
- bioRxiv 2018 (GM-CSF) — Nav1.8-богатая TTX-R фракция 47.5 пА/пФ (контроль), 70.1 (GM-CSF).
- J Gen Physiol 2008 131:211 (Ostman) — Nav1.9/NaN persistent Na 1.88 пА/пФ @ −50 мВ; до 12 пА/пФ после воспал. супа.
- Masuoka 2017 Front Physiol 8:272 — TRPV1 капсаицин-ток 29.6 пА/пФ (TRPA1-негат. DRG).
- Gao 2012 J Physiol 590:4691 — Ih в DRG: C-ноцицептор 5, Aδ 14, Aβ 21 пА/пФ @ −100 мВ.
- J Gen Physiol 2008 131:575 (Jia) — SCG M/KCNQ tail 2.8 пА/пФ @ −60 мВ (phasic).
- J Auton Nerv Syst 1998 70 — stellate Ca-ток ~0.65 нА / 19 пФ (N-тип); J Neurosci 2016 36:8562 — симпат. Ca 108 пА/пФ.
- Zampini 2010 J Physiol 588:3151 (PMC2817446) — Cav1.3 IHC: γ=18 пС (Ba²⁺), Po max 0.23, порог −70 мВ.
- Kang & Kim 2006 — TREK-1 нативный DRG γ≈112 пС (150 мМ KCl), ~12% фонового K.
- Whyment 2011 Neuroscience 178:68 — Kv4.1/4.3 A-ток преганглионарных (4-AP/phrixotoxin-2), плотность не дана.

### Новые источники (2026-09-02)
- PNAS 2000 97:13889 (PMID 11106397) — TRPV1 одиночный канал ~90–100 пС, Po(capsaicin).
- McKemy 2002 Nature 416:52 — TRPM8; J Neurosci 2002 (Reid) — нативный TRPM8 ~22 пС.
- Earley 2012 Br J Pharmacol — TRPA1 γ≈98 пС.
- OATD Toronto (Meir) / Biophys J 2010 99:3114 — Cav2.2 γ=2.76 пС (2 мМ Ca²⁺), N>L>T.
- Kim 2009 J Physiol 587:2963 — TASK-1 ~14 / TASK-3 ~32 пС, гетеромер TASK-1/3 в гломусе.
- Biophys J 2014 (HEK293 Kv7.4) — KCNQ4 γ=2.31 пС, NPo 0.61.
- Oliver 2003 J Neurosci / Brandt 2003 — IHC KCNQ4 задаёт RMP; Cav1.3→BK.
- Rugiero 2003 J Neurosci 23:2715 — Nav1.9 в AH/Dogiel II IPAN.
- J Neurosci 2018 (Hoffmann) — Nav1.8 проводит ПД в C-волокнах.
- KEGG map04742 (Taste transduction) — состав вкусовых клеток тип I/II/III.
