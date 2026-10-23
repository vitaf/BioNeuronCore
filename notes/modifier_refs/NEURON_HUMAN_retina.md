# NEURON_HUMAN — Ретина (аудит, группа 5)

**Источник:** агент (WebSearch).

- **ConePhotoreceptor** — глутамат. **Cav1.4** (CACNA1F), CNG, CaCC (TMEM16), Kv. RMP −40, градуальный. ⚠️ **код MOD_CAV13 (Cav1.3) — ошибка, нужен Cav1.4**.
- **RodPhotoreceptor** — глутамат. CNG, Cav1.4, CaCC, HCN1, Kv. RMP −40.
- **RetinalBipolar_ON** — глутамат. mGluR6→Gαo→TRPM1, L-type Ca, HCN.
- **RetinalBipolar_OFF** — глутамат. AMPA/KA, L-type Ca.
- **RodBipolarCell** — глутамат. mGluR6/TRPM1, PKCα, CACNA1F.
- **RetinalHorizontal** — GABA. AMPA/KA, HCN1, Kv. Деполяризован в темноте (−20..−30, не −65).
- **RetinalAmacrine_AII** — глицин. Nav (TTX-ПД), Kv, Ca; gap junctions (Cx36). Спайковая.
- **Amacrine_Glycinergic** — глицин (AII-like).
- **StarburstAmacrine** — ACh + GABA (ко-релиз); Nav, Kv, Ca, HCN; направление-селективность.
- **Amacrine_Dopaminergic** — дофамин; Nav, Kv, Ca, HCN.
- **Amacrine_A17** — GABA; L-type Ca, AMPA; локальные Ca-спайки.
- **RetinalGanglion_M/Mtype** — глутамат; Nav1.6, Kv, HCN; спайковая; аксон **миелинизирован** (optic nerve).
- **RetinalGanglion_Bistratified** — глутамат; миелин да.
- **ipRGC** — глутамат + PACAP; меланопсин, TRPC, Nav, Kv, HCN; миелин да.
- **RetinalPigmentEpithelium** — не нейрон (эпителий); Kir7.1, Cav1.3, bestrophin.

## Ключевые расхождения
1. Фоторецепторы: Cav1.4 (не Cav1.3).
2. Фоторецепторы: KCC2 неверен — нужен **NKCC1** (высокий внутрикл. Cl⁻).
3. Horizontal RMP −65 занижен (темнота ~−20..−30).

## Источники
- Barnes & Kelly 2002 — Ca-каналы сетчатки.
- Pflügers Arch 2021 — Cav1.4/CSNB2.
- Veruki & Hartveit 2002 J Neurosci 22:10558 — rod pathway gap junctions.
- Barnes & Grove 2020 Front Cell Neurosci — horizontal feedback.
- Rampino & Nawy 2011 J Neurosci 31:13596 — PKCα/TRPM1 rod bipolar.

---

## Литературные плотности γ/Po/G (дополнено 2026-09-02)

**Формула:** `density = G·10/γ` (G мС/см², γ пС); Po/гейтинг — в open_prob, НЕ в density.

### ConePhotoreceptor (ConePhotoreceptor)
**RMP** −40 мВ (темнота), **градуальный** (без спайков), порог не применим.

| Канал | γ (пС) | Po | G / амплитуда | Локализация | Источник |
|---|---|---|---|---|---|
| Cav1.4 (CACNA1F, #144) | **1–4** (наш 4); 9 в высоком Ba²⁺ | пик **0.05–0.1** (гетерол.), Po<0.002 @ −20 мВ | I_Ca **~45 пА** (мышь, whole-cell); G (мС/см²) — нужна лит-ра | терминаль/pedicle (пресинапс, ribbon) | Doering 2005 Biophys J 89:3042; Grassmeyer & Thoreson 2017 |
| CaBP4 (#145) | — (модулятор, ΔV½ −10 мВ) | — | — | терминаль (IQ-мотив Cav1.4) | Shaltiel 2012 |
| CNG | нужна лит-ра (в низком диваленте ~25 пС, сильно блокируется Ca²⁺/Mg²⁺) | — | тёмный ток | наружный сегмент | — |
| HCN1 | 1 (HCN1 ~0.65 пС) | V-зависимый | **0.23–0.24** (палочка, внутр. сегмент) | внутренний сегмент | Barrow & Wu 2009 J Neurosci 29:5841 (whole-cell 1.4 нС / 586 мкм²; 3.6 кан./мкм²) |
| Kir/KCNJ | 30 (Kir2.1) | V-зависимый | нужна лит-ра | внутренний сегмент | — |
| T-Ca Cav3.2 (компонент) | 9 | V-зависимый | пик ~−40 мВ; G — нужна лит-ра | терминаль | — |

- **Число каналов:** 50–75 каналов/ribbon (саламандра), 13 ribbon/конус → ~650–1000 Cav1.4/конус.
- **V50 Cav1.4 = −38.1 ± 3.05 мВ** (Grassmeyer & Thoreson 2017) — близко к тёмному −40 мВ → тонический Ca²⁺-вход в темноте.

### RodPhotoreceptor
**RMP** −40 мВ; тёмный ток −25…−40 пА. Cav1.4 (саламандра rod L-type γ=**20 пС в Ba²⁺**, Po пик ~0.3), CNG, HCN1 (внутр. сегмент), Kir, CaCC. G — «нужна лит-ра» (см. Cone для γ/Po; density=G·10/γ).

### RetinalBipolar ON/OFF + RodBipolar
- **ON/RodBipolar:** mGluR6→Gαo→**TRPM1** (I_CAN, dendritic tips, неселективный катионный); L-type Ca (Cav1.3/Cav1.4) в терминали; HCN (в части). G (TRPM1/Gαo, L-Ca) — **«нужна лит-ра»** (прямых плотностей не найдено; TRPM1 рекомбинантный ≠ нативный, Koike 2010).
- **OFF:** AMPA/KA + L-type Ca. G — «нужна лит-ра».

### RetinalGanglion (M/Mtype, Bistratified, ipRGC)
**RMP** −65 мВ, спайковая, аксон миелинизирован.

| Канал | γ (пС) | Po | G | Локализация | Источник |
|---|---|---|---|---|---|
| Nav1.6 (#141) | 20.7 | гейт m³h | **50** (single-comp); **80** (сома); **100–150** (аксон-холмик) | AIS/аксон | Fohlmeister & Miller 1997 J Neurophysiol 78:1935/1948 |
| Kv3 (KCNC) | нужна лит-ра | гейт | **12** (single-comp KDR); **18** (сома) | сома/аксон | Fohlmeister & Miller 1997 (KDR) |
| HCN | 1 | V-зависимый | нужна лит-ра | соматодендрит | — |
| KCa/A-type (Kv4.2 γ=5) | 5–250 | — | **g_A=36** (single)/**54** (сома); **g_KCa=0.05**/0.065; g_Ca=2.2 | сома/дендрит | Fohlmeister & Miller 1997 |
| Leak | — | — | **3–8 µS/см²** (0.003–0.008 мС/см²) | вся мембрана | Fohlmeister & Miller 1997 J Neurophysiol 78:1948 |

> Модель Fohlmeister & Miller 1997: 5 каналов (Na, Ca, A-K, KCa, KDR) + leak; молекулярные подтипы (Nav1.6/Kv3) — экстраполяция, G не привязана к молекулам.

### StarburstAmacrine
ACh + GABA ко-релиз; Nav (γ=20), Kv, Ca, HCN (γ=1). γ/Po — из системных справочников; G — «нужна лит-ра».

**Площади (типичные; человеческие — нужна лит-ра):** конус — наружный сегмент r≈1–2 мкм, l≈15–30 мкм (2πrl ~ 90–380 мкм²); внутренний сегмент r≈2–3 мкм, l≈15 мкм; сома r≈3–5 мкм; pedicle/терминаль r≈3 мкм. RGC сома r≈8–12 мкм + дендритное дерево (area сильно варьирует).
