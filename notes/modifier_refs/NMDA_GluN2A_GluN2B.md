# NMDA-рецептор — изоформы GluN2A / GluN2B (головастик vs человек)

**Дата:** 2026-10-14. **Статус:** источник для изоформы GluN2B (d→d/d→c головастика).

## Структура молекулы (это РАЗНЫЕ молекулы, не одна с параметром)

NMDA-рецептор — **гетеротетрамер GluN1 + GluN2** (+ опционально GluN3). GluN2-субъединица
определяет кинетику деактивации и Mg²⁺-блок. Изоформы GluN2: **A, B, C, D**.

| субъединица | τ_deactivation | Mg²⁺-блок | когда/где |
|---|---|---|---|
| **GluN2A** | ~50 мс (рекомб. ~35 мс) | сильный (IC50 низкий) | взрослый (быстрый) |
| **GluN2B** | ~400 мс (рекомб.) / ~100 мс (нативный синапт.) | слабее (~10×) | эмбрион/неонатал (медленный) |
| GluN2C | ~300 мс | слабый | мозжечок |
| GluN2D | ~1–2 с | слабый | эмбрион |

Развитие — **GluN2B→GluN2A switch** (активность/опыт-зависимый): неонатал GluN2B (медленный),
взрослый GluN2A (быстрый). PND4–6 гиппокамп WT: ~93.7 мс (взвешенный), GluN2A-KO: ~109.7 мс.

## Головастик Xenopus laevis

**PMID 19826620** (Schmidt & Hollmann 2009, Front Neuroanat, doi 10.3389/neuro.02.004.2009):
клонированы и экспрессируются в ЦНС головастика **NR1, NR2A, NR2B** (= GluN1, GluN2A, GluN2B).
XenNR2B консервативен между видами; ifenprodil (GluN2B-селективный) действует на Xenopus
с той же аффинностью. NR2D — данных нет.

**Вывод:** эмбриональный dIN головастика = **GluN2B-содержащий NMDA (медленная деактивация)**,
а наш общий NMDA (тип 9, τ_decay 50 мс) = GluN2A-взрослый.

## Длинные NMDA-EPSP → ребаунд (механизм)

- **Roberts & Tunstall 1990** (Eur J Neurosci, PMID 12106099): модель Xenopus — возбуждающий
  (NMDA-type) канал τ_open 1 мс / **τ_close 75 мс**; «короткий IPSP на длинном NMDA-EPSP → rebound».
  (75 мс — параметр упрощённой модели, НЕ физиология GluN2B.)
- **Roberts, Tunstall & Wolf 1995** (J Neurophysiol 73:485): ритм = «long-duration NMDA-dependent
  feedback excitation»; Mg²⁺-блок (voltage-dependency) усиливает postinhibitory rebound.
- **Dale & Roberts 1985** (PMID 2993537): «tonic depolarization that falls away slowly» (background);
  APV (NMDA-антагонист) блокирует.

## Кинетика и Mg²⁺-блок (формулы)

Активация r (в `ionotropic_ghk_current`): r_inf = nt/(nt+EC50), τ = τ_rise (r_inf>r) / τ_decay.

Mg²⁺-блок Woodhull: `B(V) = 1/(1 + ([Mg]/K_0)·exp(−δ·z·F·V/RT))`.
В коде: `mg_block = 1/(1 + (1/K_0)·exp(−coef·V))`, [Mg]=1 мМ, K_0=3.57 мМ, coef=0.062 (GluN2A).

⚠️ **КОРРЕКЦИЯ (2026-10-14):** GluN2A и GluN2B имеют ОДИНАКОВО сильный Mg²⁺-блок (δ~1,
IC50 ~2 µM при −100 мВ). Слабее только GluN2C/D (δ~0.7). Наш K_0=3.57/coef=0.062 корректен
для обоих; GluN2B можно уточнить K₀(0мВ)=4.5 мМ/δ=1.08 (Paoletti 2013), но это НЕ «10× слабее».

✅ **ВЕРИФИЦИРОВАНО против Ferrario 2018 (2026-10-20):** K_0=3.57 мМ корректен, менять НЕ надо.
- Ferrario 2018 (eLife 7:e33281, Appendix 2 + `syn_nmda.mod`): `mgBlock = 1/(1 + 0.05·exp(−0.08·V))`
  ≡ K_0≈20 мМ/δ≈1.0 — «sigmoidal scaling term», добавлен феноменологически **БЕЗ ссылки на измерение**.
  ~5.6× слабее нашего (при −40 мВ: 23% vs 45% пропускание).
- Первичная литература Xenopus: **Soffe & Roberts 1989** (Eur J Neurosci 1:507–515, DOI 10.1111/j.1460-9568.1989.tb00357.x,
  Mg-блок вентральных ритмических нейронов) + **Roberts/Tunstall/Wolf 1995** (J Neurophysiol 73:485) —
  Mg-блок эмбрионального Xenopus **«высоко консервативен»**, близок к неонатальному крысиному гиппокампу;
  блок при 0.5–1 мМ Mg²⁺ voltage-dependent.
- dIN головастика = GluN2B (NR1/NR2A/NR2B, PMID 19826620) → сильный Mg-блок (δ~1, как GluN2A).
- **Следствие:** слабый d→d NMDA-фон на покое = честная биология (сильный Mg-блок при гиперполяризации),
  НЕ баг. Steady-depolarisation (Soffe 2009) даётся нисходящим драйвом/рекуррентным NMDA **ВО ВРЕМЯ
  ребаунда** (когда dIN деполяризован и Mg-блок снят), не тоническим NMDA-фоном на покое.

## CDI (Ca²⁺/кальмодулин-инактивация) — единый для GluN2A/GluN2B

- Механизм: Ca²⁺ → кальмодулин (пре-связан с C0 GluN1) → инактивация. **Общий для 2A/2B** (Iacobucci & Popescu 2019).
- τ_onset ~500 мс, τ_recovery ~9.3 с, max ~80% (20% residual).
- GluN2A CDI 0.42–0.75, GluN2B 0.08 (физиол.) — разница от Po, НЕ от CDI-механизма.
- Реализация (согласована с Cav1.2 Kd=1µM): `cdi_inf = 1 − CDI_MAX·Ca/(Ca+Kd)`, CDI_MAX=0.8, Kd=1 µM,
  state[2], τ = 500 мс (onset) / 9000 мс (recovery). ⚠️ Если окажется ошибкой — вернуться к per-изоформа (0.75/0.67).

## Глицин-коагонист (насыщен)

- D-серин EC50: GluN2A 1.77 µM, GluN2B 0.47 µM; глицин ~1–3 µM.
- В синапсе коагонист НАСЫЩЕН (внеклет. глицин ~10 µM >> EC50) → фактор ≈ 0.91 (константа, не динамика).

## Наша реализация

- **GluN2A** = существующий `MOD_NT_RECEPTOR_NMDA` (тип 9): τ_rise 5 мс, τ_decay 50 мс,
  γ 50 пС, P_Na:P_K:P_Ca = 1:1:10.6, Mg-блок K_0 3.57 / coef 0.062. Используется человеком (49 нейронов).
- **GluN2B** = новая изоформа `MOD_NT_RECEPTOR_NMDA_2B` (тип 194): τ_rise 10 мс, τ_decay 150 мс
  (литературный GluN2B 100–400 мс), тот же GHK Na/K/Ca и Mg-блок. Применяется на d→d и d→c головастика.

## Источники

- Schmidt & Hollmann 2009 — Cloning NR1/NR2A/NR2B in Xenopus laevis tadpoles. Front Neuroanat. PMID 19826620.
- Roberts & Tunstall 1990 — Mutual Re-excitation with Post-Inhibitory Rebound. Eur J Neurosci 2:11. PMID 12106099.
- Roberts, Tunstall & Wolf 1995 — Voltage dependency of NMDA channels. J Neurophysiol 73:485.
- Dale & Roberts 1985 — Synaptic potentials in motoneurons during fictive swimming. J Neurophysiol 54:1. PMID 2993537.
- Soffe & Roberts 1989 — Influence of magnesium ions on NMDA responses of ventral rhythmic neurons in Xenopus embryos. Eur J Neurosci 1:507–515. DOI 10.1111/j.1460-9568.1989.tb00357.x.
- Paoletti, Bellone & Zhou 2013 — NMDA receptor subunit diversity. Nat Rev Neurosci 14:383-400.

Связано: [[DIN_NAK_125]], [[dIN_Ih_rebound]], [[cpg-recent-literature]].
