# NEURON_HUMAN — Мозжечок (аудит, группа 2)

**Источник:** агент (WebSearch).

- **Purkinje** — GABA; пейсмекер 20–50 Гц. Каналы: resurgent Na (Nav1.6), **P/Q-type Ca (Cav2.1)**, T-type (Cav3.1–3.3), Kv1/Kv3/Kv4, BK, SK2, HCN1. ⚠️ **код CAV13 подписан «P-type» — неверно, P-type=Cav2.1** (Cav1.3 не доминирует).
- **Granule_cerebellum** — глутамат; утечка K2P TASK-1/3 (IK(SO)); тонический GABAA (α6δ). ⚠️ Kv и тонический GABAA не выражены.
- **Golgi_cerebellum** — GABA (+глицин); feedback-ингибирование гранул. ⚠️ чистый GABA (глицин упущен, для человека допустимо).
- **Stellate_Cerebellum** — GABA; T-type Ca. ⚠️ пропущен Kv3 (быстрый спайк PV+).
- **Basket_Cerebellum** — GABA, PV+, fast-spiking (**Kv3.x**). ⚠️ **код Kv4.2 вместо Kv3.1**.
- **Lugaro** — смешанный GABA/глицин (≈2/3). ⚠️ чистый GABA, глицин упущен.
- **Dentate/Interpositus/Fastigial nuclei** — глутамат; 50–100 Гц, Kv3.1, BK, **rebound через T-type (Cav3.1) + HCN + persistent Na**. ⚠️ нет Kv3.1, нет T-type/HCN → rebound не воспроизводится.
- **InferiorOlive** — глутамат (climbing fiber); **Cav3.1 (T-type) + Cav2.1 (P/Q)** → осцилляции 4–10 Гц; Cx36. ⚠️ дендрит CAV13 (Cav1.3) вместо Cav2.1.
- **PontineNuclei** — глутамат (mossy fiber). Код адекватен.

## Главные расхождения
(1) Purkinje/IO «P-type» = Cav2.1, не CAV13; (2) Basket/DCN нужен Kv3.1; (3) DCN — T-type Cav3.1 + HCN для rebound; (4) Lugaro — глицин.

## Источники
- Masoli 2015 Front Cell Neurosci 9:47 — Purkinje.
- Swensen & Bean 2003 J Neurosci — resurgent Na Nav1.6.
- Aller 2005 J Neurosci — TASK гранулы.
- Brickley 2001 Nature 409:88 — тонический GABAA.
- Steuber 2010 ModelDB 136175 — DCN.
- Park 2010 PNAS 107:10731 — Cav3.1 IO.

---

## Дополнение: литературные плотности γ/Po/G (агент, 2026-09-02; G заполнены 2026-09-05)

**Формула пересчёта (как в коде):** `density [мкм⁻²] = G [мСм/см²] · 10 / (γ [пСм] · Po)`.

### Одноканальная проводимость γ и Po (литература)
| Канал | γ (пСм) | Po | Примечание / источник |
|---|---|---|---|
| Nav1.6 | 20.7 ± 0.7 | пик 0.45 (0 мВ); persistent 0.016 | Chatelier 2010 Pflügers Arch |
| Cav2.1 (P/Q) | 9 / 14 / 19 (подсостояния) | NPo 0.13 (patch), Po_max 0.46 (челов. α1A HEK) | нативный Purkinje |
| Cav3.1 (T) | 9.5 (Ca²⁺), 10.3 (Ba²⁺) | 0.1–0.3 | Biophys J 2008 (PMID 18375519) |
| HCN1 | нативный ~9.7; рекомб. mHCN1 0.84 | V₁/₂ ≈ −81 мВ (Po) | Simeone 2005 J Physiol; bioRxiv 2024 |
| Kv3.1 | 27 | — | Grissmer 1992 |
| Kv4.2 (A) | ~4 (голая), ~7.5 (с DPP6, нативный) | — | J Neurosci 2009 (PMID 19279261) |
| BK | 250–300 | Ca²⁺-зависимый | — |
| SK2 | 4–14 (≈11) | Ca²⁺-зависимый | — |
| TASK-1 / TASK-3 | 14–16 / 36–38 | ~1 (leak) | Kang 2004; Han 2002 (гранулы) |

### Таблица «тип → канал → γ/Po/G → density → локализация → источник G»
| Тип | Канал | γ (пСм) | Po | G (мСм/см²) | density (мкм⁻²) | Локализация | Источник G |
|---|---|---|---|---|---|---|---|
| Purkinje | Nav1.6 (transient+resurgent) | 20.7 | 0.45 | сома 214; AIS **500**; дендр 16; узел Ранвье 30 | сома 230; AIS 537; дендр 17; узел 32 | AIS/узлы (высокая), сома, дендриты | Masoli 2015 (ModelDB 229585) |
| Purkinje | Nav1.6 persistent (NaP) | 20.7 | 0.016 | ≈1.6–5% от transient (сома ~3–10) | калибровка | AIS/сома | Kay 1998; Chatelier 2010 |
| Purkinje | Cav2.1 (P/Q) | 9/14/19 | NPo 0.13 | сома 0.22; дендр 1.0; AIS 0.22 | калибровка | соматодендрит (дендр ↑ ~4.5×) | Masoli 2015 |
| Purkinje | Cav3.1 (T, LVA) | 9.5 | 0.1–0.3 | сома 0.007; дендр 0.005; AIS 0.008 | ~0.04 | дендриты | Masoli 2015 |
| Purkinje | Cav3.2 / Cav3.3 | 9.5 | 0.1–0.3 | Cav3.2 сома 0.8/дендр 1.2; Cav3.3 0.1 | калибровка | дендриты | Masoli 2015 |
| Purkinje | Kv3.4 / Kv3.3 | 27 | — | Kv3.4 сома 50/AIS 10/узел 20; Kv3.3 дендр 10 | калибровка | сома | Masoli 2015 |
| Purkinje | Kv1.1 / Kv4.3 (A) | ~7.5 | — | Kv1.1 сома 2/дендр 1.2/AIS 10; Kv4.3 дендр 1.0 | калибровка | дендриты | Masoli 2015 |
| Purkinje | BK (KCa1.1) | 250 | Ca²⁺-зав. | сома 10; дендр 35 | Ca²⁺-зав. | сома | Masoli 2015 |
| Purkinje | SK2 (KCa2.2) | ~11 | Ca²⁺-зав. | сома 1.0; дендр 1.0 | Ca²⁺-зав. | сома | Masoli 2015 |
| Purkinje | HCN1 | ~0.84–9.7 | V-зав. | сома 0.4; дендр 0.004 | низкая | дендриты | Masoli 2015 |
| Basket_Cerebellum | Kv3.4 (PV+, fast-spiking) ⚠️ НЕ Kv3.1 | 27 | ~0.7 | сома 97; AIS 2 | калибровка | сома/AIS | Masoli 2025 (ModelDB 2018018); γ/Po Alle 2011 |
| DCN (Dentate/Interpos./Fastigial) | Nav1.6 fast | 20.7 | 0.45 | сома 25; AIS 50 | сома 27; AIS 54 | сома/AIS | Steuber 2010 (ModelDB 136175) |
| DCN | NaP (rebound-вход) | 20.7 | 0.016 | 0.2–0.8 | калибровка | сома | Steuber 2010 |
| DCN | Cav3.1 (T, rebound) | 9.5 | 0.1–0.3 | сома 0.15–0.45; дендр 0.3–0.9 | 0.8–2.4 | сома-дендрит | Steuber 2010 |
| DCN | HCN1 (Ih) | ~0.84–9.7 | V-зав. | сома 0.2; прокс. 0.4; дист. 0.6 | калибровка | дендриты | Steuber 2010 |
| DCN | Kv3.1 (fKdr) | 27 | — | сома 15; AIS 30 | калибровка | сома/AIS | Steuber 2010 |
| DCN | Kv2 (sKdr) | — | — | сома 12.5; AIS 25 | калибровка | сома | Steuber 2010 |
| DCN | SK | ~11 | Ca²⁺-зав. | 0.22 | Ca²⁺-зав. | сома | Steuber 2010 |
| DCN | NALCN/TNC (tonic cation) | — | ~1 | сома 0.03 | низкая | сома/дендр (**НЕ AIS**) | Steuber 2010 (GTNC) |
| InferiorOlive | Cav3.1 (T) | 9.5 | 0.1–0.3 | 1.0 (диап. 0.95–2.5) | ~5.3 | сома-дендрит | Schweighofer 1999 |
| InferiorOlive | Cav2.1 (P/Q, HVA) | 9/14/19 | ~0.05–0.1 | **4.0–4.5** | ~2–4 | дендриты | Schweighofer 1999 (gCa_h, PMID 10444678); Chaudhuri 2007 |
| InferiorOlive | HCN (Ih) | ~1 | V-зав. | 0.5–0.7 | калибровка | сома | Schweighofer 1999 |
| Granule_cerebellum | TASK-1/TASK-3 (leak IK(SO)) | 14–16 / 36–38 | ~1 | 0.057 (gL, E=−59) | ~0.04 | диффузно | D'Angelo 2001 |
| Granule_cerebellum | тонический GABAA α6δ | (рецептор) | — | 0.022 (28% входа, E=−65) | — | экстрасинаптич. дендриты | D'Angelo 2001 |
| Golgi_cerebellum | Nav1.6 (Na) | 20.7 | 0.45 | 48 | 52 | сома | Solinas 2007 (ModelDB 112685) |
| Golgi_cerebellum | NaP | 20.7 | 0.016 | 0.19 | калибровка | сома | Solinas 2007 |
| Golgi_cerebellum | Kv (delayed rectifier) | — | — | 32 | калибровка | сома | Solinas 2007 |
| Golgi_cerebellum | Kv4 (A-type) | ~7.5 | — | 8 | калибровка | дендриты | Solinas 2007 |
| Golgi_cerebellum | BK (KCa1.1) | 250 | Ca²⁺-зав. | 3 | Ca²⁺-зав. | сома | Solinas 2007 |
| Golgi_cerebellum | SK2 | ~11 | Ca²⁺-зав. | 38 (модель) | Ca²⁺-зав. | сома | Solinas 2007 |
| Golgi_cerebellum | HCN1 / HCN2 | ~0.84–9.7 | V-зав. | HCN1 0.05; HCN2 0.08 | калибровка | дендриты | Solinas 2007 |
| Golgi_cerebellum | Cav (HVA) | — | — | 0.46 | калибровка | сома-дендрит | Solinas 2007 |
| Golgi_cerebellum | Cav3 (LVA/T) | 9.5 | 0.1–0.3 | 0.25 | калибровка | дендриты | Solinas 2007 |
| Lugaro | (GABA/глицин, HCN?) | — | — | нужна лит. | — | — | — |
| UnipolarBrush | Na/Cav/HCN/TRP | — | — | Na 192; KDR 0.675; KA 7; BK 5.2; HCN 0.572; CaHVA 0.215 | калибровка | — | Subramaniyam 2014 (Table 1, PMID 25191224) |

### Площади сегментов (2πrl) и пассивные параметры — типовые, нужна точная морфометрия
| Тип | Сома (d, area) | AIS/аксон | RMP | Порог | R_in |
|---|---|---|---|---|---|
| Purkinje | d 20–25 мкм → ~1250–2000 мкм² | AIS r~1 мкм, l 15–30 мкм → ~100–190 мкм² | −60…−65 мВ | ~−50 мВ | 30–100 МОм |
| Basket | d 10–15 мкм → ~300–700 мкм² | — | −60…−65 мВ | ~−50 мВ | 50–100 МОм |
| DCN | d 25–35 мкм → ~2000–3800 мкм² | — | −55…−60 мВ | ~−50 мВ | ~100 МОм |
| InferiorOlive | d 20–30 мкм → ~1250–2800 мкм² | — | −55…−65 мВ | ~−50 мВ | 50–100 МОм |
| Granule | d 5–8 мкм → ~100–200 мкм² | — | −60…−70 мВ | ~−40 мВ | 1–3 ГОм |

**Локализация (градиенты):** Nav1.6 → AIS (≈2.3× сомы у Purkinje) + узлы Ранвье; Cav2.1/Cav3 → сома-дендрит (дендритный градиент ↑, у IO и Purkinje); Kv1.1/Kv4.3 (A) → дистальные дендриты; HCN1 → дендриты; Kv3.1 → сома/AIS (fast-spiking); BK/SK2 → сома; TASK → диффузно (gradient=0); тонический GABAA α6δ → экстрасинаптический (не синаптический); NALCN/TNC → сома/дендрит (**НЕ AIS** — проектная конвенция).

## НЕ НАЙДЕНО / нужна лит. (для этапа 6 — калибровка)
- **InferiorOlive Cav2.1 (P/Q HVA)** — G в Schweighofer 1999 (gCa_h, дендритный HVA) не подтверждён из доступных сниппетов → нужна лит.
- **Lugaro** — количественной модели/плотностей каналов не найдено (только морфология/нейрохимия) → нужна лит.
- **UnipolarBrush** — G найдены (Subramaniyam 2014 Table 1, PMID 25191224): Na=192, KDR=0.675, KA=7, BK=5.2, HCN=0.572, CaHVA=0.215 мС/см² (CaLVA — пермеабельность 6.3e-5 см/с).
- **Basket Kv3** — в мозжечковой корзинчатой клетке быстрый Kv3-ток = **Kv3.4, НЕ Kv3.1** (Masoli 2025, ModelDB 2018018: сома 97 мС/см², AIS 2). «Ozaita 2004 (1.3–1.7 мС/см²)» — про starburst amacrine сетчатки (Kv3.1/3.2), НЕ мозжечок. γ=27/Po≈0.7 (Alle 2011) не подтверждены независимо.

**Новые источники:** Chatelier 2010 Pflügers Arch (Nav1.6 single-channel); Indriati 2013 (Cav2.1 соматодендр. градиент Purkinje); Simeone 2005 J Physiol 568 (HCN single-channel); Grissmer 1992 (Kv3.1 γ=27 пСм); Kang 2004 J Physiol 554 (TASK-1/3 гранулы); Biophys J 2008 (Cav3.1 γ); Masoli 2015 (ModelDB 229585, PC_param.py — полный набор Purkinje); Steuber 2010 (ModelDB 136175, cn_const.g — DCN); Schweighofer 1999 J Neurophysiol 82:804 (IO); D'Angelo 2001 J Neurosci 21:759 (гранулы, gL/tonic GABA); Solinas 2007 (ModelDB 112685 — Golgi); Ozaita 2004 (Kv3.1/3.2 FS-интерн.).
