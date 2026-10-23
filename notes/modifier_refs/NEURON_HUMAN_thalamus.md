# NEURON_HUMAN — Таламус (аудит, группа 4)

**Источник:** агент (WebSearch).

- **Thalamocortical (relay)** — глутамат. T-type **Cav3.1/α1G** (Talley 1999), **Ih/HCN** (McCormick & Pape 1990), K2P **TASK1/TASK3** (~40% стоячего тока, Meuth 2003). Burst (V<−65) / tonic (V>−60). Аксон миелинизирован. ⚠️ **T-type добавлен ДВАЖДЫ (0.6+0.8 дубль)** + **MOD_CAV13 подписан «T-type» — это Cav1.3 (L-type)**.
- **ThalamicReticular (nRt/TRN)** — GABA. T-type **Cav3.2/α1H + Cav3.3/α1I**, медленная I_Ts (Huguenard & Prince 1992), **SK2** (Cueni 2008). **Ih/HCN слабый/отсутствует**. ⚠️ код TRN даны HCN2 (0.15) — противоречит.
- **ThalamicRelay_LGN** — глутамат; P-/M-клетки; T-type + Ih + K2P.
- **ThalamicRelay_MGN** — глутамат; T-type + Kv4 (A-type ↑дист.).
- **ThalamicRelay_VPL/VPM** — глутамат; lemniscal vs paralemniscal; T-type + Ih.
- **ThalamicRelay_Pulvinar** — глутамат; higher-order; T-type + Ih.
- **ThalamicRelay_MD** — глутамат; dlPFC; T-type + Ih.
- **ThalamicInterneuron** — GABA. **Дендритный GABA-релиз (F2), триадные синапсы** (Ohara 1989); **T-type (I_T) + I_CAN** → burst (Zhu 1999). ⚠️ **код «голый» — нет T-type/SK/дендритного GABA**.

## Главные расхождения
(1) Thalamocortical дубль T-type + неверная подпись Cav1.3; (2) TRN HCN должен быть слабым/отсутствовать; (3) интернейрон без T-type/SK.

## Источники
- Llinás & Jahnsen 1982 Nature 297:406 (PMID 7078650).
- McCormick & Pape 1990 J Physiol 431:291.
- Talley 1999 J Neurosci 19:1895 (PMID 10066243).
- Huguenard & Prince 1992 J Neurosci 12:3804 (PMID 1403085).
- Cueni 2008 Nat Neurosci 11:683 (PMID 18488023).
- Meuth 2003 J Neurosci 23:6460 (PMID 12878686).
- Zhu 1999 Neuroscience 91:1445 (PMID 10391450).
- Ohara 1989 J Comp Neurol 283:541 (PMID 2745753).

---

## Литературные плотности γ/Po/G (дополнено 2026-09-02)

**Формула калибровки** (система): `density = G·10/γ` (G мС/см², γ пС). Po/гейтинг передаётся
в `open_prob` модификатора и НЕ входит в density (иначе двойной учёт Po → runaway). Для
leak/persistent (K2P/NaP) в части конфигов density=G·10/(γ·Po) — сверять с реализацией модификатора.

### Thalamocortical (relay)

| Канал | γ (пС) | Po | G (мС/см²) | density (=G·10/γ) | Локализация (градиент) | Источник |
|---|---|---|---|---|---|---|
| Nav fast (Nav1.1/1.6) | 20 (Nav1.x) | гейт m³h | **100** (сома) | 50 | сома + AIS/аксон | Destexhe 1998 J Neurosci 18:3574 (Traub-Miles кинетика) |
| KDR (Kv1.x) | 12 (9–14) | гейт | **100** (сома) | ~83 | сома/аксон | Destexhe 1998 |
| T-Ca Cav3.1 (α1G) | **7.5** | V-зависимый (де-инакт. <−65) | ~2 (сома); дендрит **4.5–7.6×** | ~27 | соматодендрит ↑дист. | Talley 1999; Destexhe 1998 (P_T дендр. 0.25 µm/s) |
| Ih HCN2/3 | 1 (HCN2 ~1.5) | V-зависимый | **0.02** (сома); ~0.084 дендр. (5×) | ~0.2 (=G·10/γ) | соматодендрит | Huguenard & McCormick 1992 J Neurophysiol 68:1373 (g_h=0.02); Destexhe 1998 (дендр. 5×) |
| Kv7/M (KCNQ2/3) | **9.0±0.3** | V-зависимый | **0.14** (модельн.) | ~0.16 (=G·10/γ) | сома/прокс. дендрит | Selyanko 2001 J Physiol 534:15 (γ); Destexhe 1998 J Neurophysiol 79:999 (gM=0.14) |
| K2P TASK1/TASK3 | TASK1 ~14, TASK3 ~40 ⚠️ | ~0.012 (leak) | 0.5 (~40% стоячего тока) | — | сома | Meuth 2003 |
| Leak | — | — | gL=0.0379 (Destexhe 1998); g_KL=0.012 + g_NaL=0.00022 (Huguenard & McCormick 1992) | — | вся мембрана | Destexhe 1998 J Neurosci 18:3574; Huguenard & McCormick 1992 J Neurophysiol 68:1384 |

**RMP** −65…−70 мВ; **R_in** ~138 МОм (крыса LGNd, McCormick & Huguenard 1992). **Burst** требует V<−65 (T-де-инактивация), **tonic** при V>−60. Порог ПД ~ −55…−50 (нужна лит-ра).

> ⚠️ **TASK1/TASK3 ≠ MOD_K2P (107=TREK-1, γ=100 пС).** Таламический K2P — другой подтип с
> меньшей γ (TASK1 ~14 пС, TASK3 ~40 пС). При переносе на таламус нужен отдельный γ, НЕ переиспользовать TREK-1.

### ThalamicReticular (TRN/nRt)

| Канал | γ (пС) | Po | G (мС/см²) | Локализация | Источник |
|---|---|---|---|---|---|
| T-Ca Cav3.2 (α1H) | **9** | V-зависимый | **2.0–2.2** (суммарный I_T, сома) | соматодендрит | Huguenard & McCormick 1992 (RE g_T=2.0); Destexhe 1996 J Neurosci 16:169 (g_T=2.2); PMC4673464 Table 1 |
| T-Ca Cav3.3 (α1I) | **11** | V-зависимый | **2.0–2.2** (суммарный I_T; split Cav3.2/Cav3.3 — нужна лит-ра) | соматодендрит | Talley 1999; Huguenard & Prince 1992; PMC4673464 |
| SK2 (KCa2.2) | **10** | Ca²⁺-зависимый (Hill 4, EC50 0.4 µM) | нужна лит-ра (роль pacemaker/AHP, мС/см² не дано) | сома (терминирует burst, AHP) | Cueni 2008 Nat Neurosci 11:683; Synapse 2023 (PMID 37837643) |
| Kv3.1/3.3 ⚠️ | нужна лит-ра | гейт | нужна лит-ра | сома/аксон (fast-spiking) | Espinosa 2008; Rudy & McBain 2001 |
| Kv1.2 | **14–18** | гейт | нужна лит-ра | аксон/сома | (если оставить Kv1.2) |

**TRN прочие (Huguenard & McCormick 1992, RE-клетка):** g_Na=**100**, g_K(dr)=10, g_leakK=**0.005** мС/см² (E_Na=50, E_K=−95 мВ). g_K=10 — подозрительно низко для fast-spiking, сверять с Destexhe 1996 (g_Kd=200) — нужна лит-ра.

> ⚠️ **Расхождение:** задача даёт TRN «Kv1.2», но литература (TRN-модели, Espinosa 2008;
> Rudy & McBain 2001) — высокая экспрессия **Kv3.1/Kv3.3** (fast-spiking), Kv1.2 минорный.
> Уточнить состав TRN.

### Прочие relay-типы + интернейрон
- **LGN/MGN/VPL/VPM/Pulvinar/MD** — relay-семейство: T-Ca Cav3.1 (γ=7.5) + Ih (γ=1) + K2P TASK.
  MGN дополнительно Kv4.2 (A-type, γ=5 пС, ↑дистально). G — «нужна лит-ра» для каждого ядра.
- **ThalamicInterneuron** — T-Ca (γ=7.5) + I_CAN/TRPM4 + SK; G — «нужна лит-ра» (Zhu 1999, Ohara 1989).

**Площади (типичные; человеческие размеры — нужна лит-ра):** сома r≈10–15 мкм (area 4πr² ~ 1250–2800 мкм²);
дендриты 4–6 ветвей r=1 мкм, l=100–200 мкм (area 2πrl ~ 630–1260 мкм²/ветвь).
