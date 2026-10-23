// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
#include <cstdint>
#include <cmath>

// ==================== КОНСТАНТЫ ====================
#define MODE_BIOLOGICAL  0
#define MODE_ARBITRARY    1

// Удельная ёмкость мембраны: 1 µF/cm² = 0.01 pF/µm²
// Используется в C_m = area_µm² × C_SPECIFIC_PF_PER_UM2 → результат в pF
#define C_SPECIFIC_PF_PER_UM2  0.01f

#define DIFFUSION_OFF     0
#define DIFFUSION_VOXEL   1
#define DIFFUSION_DIRECT  2

// === БАЗОВЫЕ МОДИФИКАТОРЫ ===
#define MOD_RESERVED_0                 0   // бывш. MOD_IONOTROPIC_NAK (HH squid) — удалён (Фаза 4)
#define MOD_CAV13                     1   // L-type Ca²⁺ (Cav1.3/CACNA1D, пейсмейкерный)
#define MOD_IONOTROPIC_CL              2
#define MOD_METABOTROPIC               3
#define MOD_MYELIN                     4
#define MOD_SYNAPTIC_RELEASE           5
#define MOD_PHYSICAL_SENSOR            6
#define MOD_PUMP_NAK                   7

// === ИОНОТРОПНЫЕ РЕЦЕПТОРЫ ===
#define MOD_NT_RECEPTOR_AMPA           8
#define MOD_NT_RECEPTOR_NMDA           9   // GluN1/GluN2A-содержащий NMDA (взрослый, быстрая деактивация τ_decay ~50 мс) — см. NMDA_GluN2A_GluN2B.md
#define MOD_NT_RECEPTOR_KAINATE        10
#define MOD_NT_RECEPTOR_GABAA          11
#define MOD_NT_RECEPTOR_GLYCINE        12
#define MOD_NT_RECEPTOR_NICOTINIC      13
#define MOD_NT_RECEPTOR_5HT3           14
#define MOD_NT_RECEPTOR_P2X            15
#define MOD_NT_RECEPTOR_TRPV1          16

// === МЕТАБОТРОПНЫЕ Gi/o (ингибиторные) ===
#define MOD_NT_RECEPTOR_GABAB          17
#define MOD_NT_RECEPTOR_5HT1A          18
#define MOD_NT_RECEPTOR_5HT1B          19
#define MOD_NT_RECEPTOR_5HT1D          20
#define MOD_NT_RECEPTOR_5HT1F          21
#define MOD_NT_RECEPTOR_D2             22
#define MOD_NT_RECEPTOR_D3             23
#define MOD_NT_RECEPTOR_D4             24
#define MOD_NT_RECEPTOR_M2             25
#define MOD_NT_RECEPTOR_M4             26
#define MOD_NT_RECEPTOR_A1             27
#define MOD_NT_RECEPTOR_A3             28
#define MOD_NT_RECEPTOR_MGLUR2         29
#define MOD_NT_RECEPTOR_MGLUR3         30
#define MOD_NT_RECEPTOR_MGLUR4         31
#define MOD_NT_RECEPTOR_MGLUR6         32
#define MOD_NT_RECEPTOR_MGLUR7         33
#define MOD_NT_RECEPTOR_MGLUR8         34
#define MOD_NT_RECEPTOR_ALPHA2         35
#define MOD_NT_RECEPTOR_CB1            36
#define MOD_NT_RECEPTOR_CB2            37
#define MOD_NT_RECEPTOR_H3             38
#define MOD_NT_RECEPTOR_MT1            39
#define MOD_NT_RECEPTOR_MT2            40

// === МЕТАБОТРОПНЫЕ Gs (возбуждающие через cAMP) ===
#define MOD_NT_RECEPTOR_5HT4           41
#define MOD_NT_RECEPTOR_5HT6           42
#define MOD_NT_RECEPTOR_5HT7           43
#define MOD_NT_RECEPTOR_D1             44
#define MOD_NT_RECEPTOR_D5             45
#define MOD_NT_RECEPTOR_A2A            46
#define MOD_NT_RECEPTOR_A2B            47
#define MOD_NT_RECEPTOR_BETA1          48
#define MOD_NT_RECEPTOR_BETA2          49
#define MOD_NT_RECEPTOR_H2             50
#define MOD_NT_RECEPTOR_TAAR1          51

// === МЕТАБОТРОПНЫЕ Gq/11 (возбуждающие через IP3/DAG) ===
#define MOD_NT_RECEPTOR_5HT2A          52
#define MOD_NT_RECEPTOR_5HT2B          53
#define MOD_NT_RECEPTOR_5HT2C          54
#define MOD_NT_RECEPTOR_ALPHA1A        55
#define MOD_NT_RECEPTOR_ALPHA1B        56
#define MOD_NT_RECEPTOR_M1             57
#define MOD_NT_RECEPTOR_M3             58
#define MOD_NT_RECEPTOR_M5             59
#define MOD_NT_RECEPTOR_MGLUR1         60
#define MOD_NT_RECEPTOR_MGLUR5         61
#define MOD_NT_RECEPTOR_H1             62

// === NB_-РЕЦЕПТОРЫ (Neuroblox-совместимые, IDs 63+) ===
// Детальные кинетические модели, соответствующие Neuroblox.jl
#define MOD_NB_NMDA_MORADI_FULL       63   // MoradiFullNMDAR: Q10, voltage-dep tau, Mg²⁺
#define MOD_NB_AMPA_STA               64   // Glu_AMPA_STA_Synapse: кратковременная аугментация
#define MOD_NB_D1_MODULATOR           65   // MsnD1Receptor: Hill-occupancy DA→D1, M_NMDA1
#define MOD_NB_D2_MODULATOR           66   // MsnD2Receptor: Hill-occupancy DA→D2, M_AMPA2
#define MOD_NB_ALPHA7_ER_NACHR        67   // Alpha7ERnAChR: Марков-гейтинг + ER Ca-store
#define MOD_NB_BETA2_NACHR            68   // Beta2nAChR: десенситизация никотином
#define MOD_NB_CA_TRPM4R              69   // CaTRPM4R: I_CAN с нанодоменовой Ca-динамикой
#define MOD_NB_MUSCARINIC_R           70   // MuscarinicR: I_NCM с Na/K-аппорцией
#define MOD_NB_AMPA_DESTEXHE          71   // Glu_AMPA_Synapse: damped oscillator (Destexhe 1998)
#define MOD_NB_NMDA_DESTEXHE          72   // NMDA_Synapse: damped oscillator (Destexhe 1998)
#define MOD_NB_GABAA_DESTEXHE         73   // GABA_A_Synapse: damped oscillator
#define MOD_NB_GABAB_DESTEXHE         74   // GABA_B_Synapse: damped oscillator (τ₁=200.1, τ₂=200)
#define MOD_NB_MSN_NMDAR              75   // MsnNMDAR: NMDA с D1-модуляцией
#define MOD_NB_MSN_AMPAR              76   // MsnAMPAR: AMPA с D2-модуляцией

// === СОМА-МОДИФИКАТОРЫ (IDs 77+) ===
#define MOD_NB_SOMA_ATP               77   // митохондриальный синтез ATP в соме
#define MOD_NB_SOMA_THERMAL           78   // термальная регуляция клетки (сома)
#define MOD_NB_CREB_TRANSCRIPTION     79   // ядерный CREB: Ca²⁺→pCREB→транскрипция

// === GENERIC-ТИПЫ (IDs 80+) — параметрическая кинетика без .so ===
#define MOD_RESERVED_80                 80   // бывш. MOD_GENERIC_VOLTAGE_GATED (Ohm-суррогат) — удалён
#define MOD_RESERVED_81                 81   // бывш. MOD_GENERIC_LIGAND_GATED (Ohm-суррогат) — удалён

// === МОЛЕКУЛЯРНЫЙ КАСКАД STDP (IDs 82+) ===
#define MOD_CAMKII                      82   // Ca²⁺/calmodulin-dependent kinase II: state[]=activity,autonomous
#define MOD_CALCINEURIN                 83   // Ca²⁺/calmodulin-dependent phosphatase (PP2B): state[]=activity

// === ПОЛНЫЙ МОЛЕКУЛЯРНЫЙ КАСКАД STDP (IDs 84+) ===
#define MOD_CA_L_TYPE                   84   // L-type Ca²⁺ канал (Cav1.x, HVA, дигидропиридин-чувств.)
#define MOD_KV42                        85   // Kv4.2 A-type K⁺ канал с CaMKII-фосфорилированием
#define MOD_ECB_SYNTHESIS               86   // эндоканнабиноидный синтез (2-AG) из Ca²⁺

// === КОНСОЛИДАЦИЯ/РЕКОНСОЛИДАЦИЯ (IDs 87-92) ===
#define MOD_MAPK_ERK                   87   // MAPK/ERK: Ca²⁺→Ras→Raf→MEK→ERK→pCREB (консолидация)
#define MOD_IKK_NFKB                   88   // IKK/NF-κB: Ca²⁺→IKK→IkB→NF-κB→Zif268 (реконсолидация)
#define MOD_MTORC1                     89   // mTORC1: двухключевая (AA + PI3K/AKT)→Rheb→mTORC1
#define MOD_PROTEIN_SYNTH              90   // Синтез белка: mTORC1→S6K1/4E-BP→трансляция
#define MOD_COFILIN                    91   // Актиновое ремоделирование: Cofilin/LIMK/F-actin
#define MOD_MMP9                       92   // Деградация ECM: MMP-9/TIMP/интегрины

// === СЕНСОРЫ И CPG (IDs 93-95) ===
#define MOD_RESERVED_93                93   // бывш. MOD_POST_INHIBITORY_REBOUND (PIR) — удалён (PIR → HCN + T-Ca²⁺)
#define MOD_HUNGER_SENSOR              94   // Сенсор голода: ATP-монитор → пищевое поведение
#define MOD_LIGHT_SENSOR               95   // Фоторецептор: pineal/eye spot

// === PIR-ПОДТИПЫ (IDs 96-97) — детальная биофизика ===
#define MOD_PIR_TTYPE_CALCIUM          96   // T-type Ca²⁺ (Cav3.x): GHK-поток, m³h-гейтинг, фазический отскок
#define MOD_NICOTINIC_ALPHA7           97   // α7 nAChR: быстрый, высокий Ca²⁺ (P_Ca/P_Na≈6-10). Бывш. PIR_HCN (deprecated)

// === Ca²⁺/K⁺/Cl⁻ ГОМЕОСТАЗ (IDs 98-102) — транспортёры и буферы ===
#define MOD_PMCA                       98   // PMCA: Ca²⁺-АТФаза плазмалеммы, ATP-зависимый выброс 1 Ca²⁺
#define MOD_NCX                        99   // NCX: Na⁺/Ca²⁺ обменник, 3Na⁺ in : 1Ca²⁺ out, V-зависимый
#define MOD_KCC2                      100   // KCC2: K⁺-Cl⁻ котранспортёр, электронейтральный 1K⁺:1Cl⁻ out
#define MOD_CA_LEAK                   101   // Ca²⁺ утечка: пассивный GHK-поток Ca²⁺ через неселективные каналы
#define MOD_CA_BUFFER                 102   // Ca²⁺ буфер: парвальбумин-подобный, быстрая кинетика on/off

// === ИОННЫЕ КАНАЛЫ РИТМОГЕНЕЗА (IDs 103-106) ===
#define MOD_SK_CHANNEL                103   // SK (KCa2.x): Ca²⁺-активируемый K⁺, AHP, Hill n=4, GHK
#define MOD_BK_CHANNEL                104   // BK (KCa1.1): Ca²⁺+V-активируемый K⁺, быстрая реполяризация, GHK
#define MOD_NAP_CHANNEL               105   // NaP (Nav1.6): персистентный Na⁺, без инактивации, подпороговый, GHK
#define MOD_KIR_CHANNEL               106   // Kir (Kir2.x): входящий выпрямитель K⁺, Больцман×rectification, GHK
#define MOD_K2P_LEAK                  107   // K2P (TREK/TRAAK): пассивная K⁺ утечка, всегда открыт, GHK
#define MOD_OLFACTORY_RECEPTOR        108   // Обонятельный рецептор (GPCR Gs): лиганд=глутамат → cAMP↑ → деполяризация
#define MOD_GLIAL_KIR41               109   // Kir4.1 глиальный: inward rectifier K⁺, [K]o-чувствительный, spatial buffering
#define MOD_GLIAL_NAK_PUMP            110   // Глиальная Na/K-АТФаза (α2): низкая скорость, высокое сродство к K⁺_o
#define MOD_THERMAL_NOISE             111   // Тепловой шум (Johnson-Nyquist): σ≈0.05mV RMS, xorshift32 RNG
#define MOD_KV7_MCURRENT              112   // Kv7 (KCNQ2/3) M-ток: медленный K⁺, V-зав., без инактивации, PIP2-модуляция
#define MOD_PP1_INHIBITOR_DARPP32      113   // PP1/Inhibitor-1/DARPP-32: D1→cAMP→PKA→p-DARPP32→inhibit PP1→facilitated LTP
#define MOD_CONNEXIN_CX36              114   // Коннексин-36: синтез/сборка/фосфорилирование/деградация gap junction
#define MOD_VIBRATION_SENSOR           115   // Вибрационный/акустический сенсор: детекция колебаний воды (аудиотаксис)
#define MOD_OTOLITH_HAIR_CELL          116   // Волосковая клетка отолитового органа: MET-каналы, Ca²⁺-адаптация, гравитация/ускорение
#define MOD_SEMICIRCULAR_CUPULA        117   // Купула полукружного канала: торсионный маятник, угловое ускорение, push-pull
#define MOD_ELECTRORECEPTOR_AMPULLARY  118   // Ампулярный электрорецептор: канал-гель, low-pass фильтр, детекция E-полей
#define MOD_MAGNETITE_MECHANORECEPTOR  119   // Магнетитовый механорецептор: Fe₃O₄-цепочки → крутящий момент → MET-каналы
#define MOD_CRYPTOCHROME_MAGNETIC      120   // Криптохромный магнитный сенсор: радикальные пары Cry4, свето-зависимый
#define MOD_KNA_CHANNEL                121   // Slack/Slick KNa (KCNT1/KCNT2): Na⁺-активируемый K⁺ канал, slow AHP
#define MOD_NKCC1                      122   // NKCC1 (SLC12A2): Na-K-2Cl котранспортер, импорт Cl⁻, деполяризующее торможение
#define MOD_KV_DELAYED_RECTIFIER        123   // Kv1.1 (KCNA1) delayed rectifier K⁺: быстрая реполяризация спайка
#define MOD_RESERVED_124                124   // зарезервированный слот (никогда не использовался — пропуск 123→125)
#define MOD_RESERVED_125                125   // бывш. MOD_DIN_NAK (Sautois-суррогат) — удалён (Фаза 4)
#define MOD_SERCA                       126   // SERCA: Ca²⁺-АТФаза ЭПР, закачивает Ca²⁺ в ER
#define MOD_IP3R                        127   // IP3R: IP3-управляемый Ca²⁺-канал ЭПР
#define MOD_RYR                         128   // RyR: рианодиновый рецептор, Ca²⁺-индуцированный Ca²⁺-выброс (CICR)
#define MOD_HCN1                        129   // HCN1: быстрый Ih (τ~84ms) — головастиковые dIN
#define MOD_HCN2                        130   // HCN2: средний Ih (τ~557ms) — сердце, мозг
#define MOD_HCN3                        131   // HCN3: медленный Ih (τ~1408ms)
#define MOD_HCN4                        132   // HCN4: самый медленный Ih (τ~3586ms) — пейсмейкерные клетки
#define MOD_NALCN                       133   // NALCN: Na⁺-leak канал (независимый от напряжения), покоящаяся Na⁺-проводимость
#define MOD_P2X2                        134   // P2X2: ATP-зависимый катионный (медленная десенситизация ~5с)
#define MOD_CAMP_PKA                    135   // cAMP/PKA каскад: Gs/Gi→AC→cAMP→PKA (общий пул на сегмент)
#define MOD_PLC_IP3_DAG                 136   // Gq→PLC→IP3+DAG: общий пул IP3/DAG на сегмент
#define MOD_PKC                         137   // PKC киназа: DAG+Ca²⁺ → активность → фосфорилирует мишени
#define MOD_RESERVED_138                138   // бывш. MOD_CIN_NAK (Sautois-суррогат) — удалён (Фаза 4)
#define MOD_RESERVED_139                139   // бывш. MOD_MN_NAK (Sautois-суррогат) — удалён (Фаза 4)
#define MOD_CAV21                       140   // Cav2.1 (P/Q-type, CACNA1A): пресинаптический Ca²⁺-канал, триггер выброса
#define MOD_AXON_NAV16                  141   // Nav1.6 (SCN8A) transient Na⁺: аксональный Na m³h (PNAS 2018), repetitive firing
#define MOD_AXON_NAV12                  142   // Nav1.2 (SCN2A) transient Na⁺: аксональный Na m³h, repetitive — тадпольный спинальный изоформ (JGP 2022)
#define MOD_RESERVED_143                143   // бывш. MOD_RB_NAK (Sautois-суррогат) — удалён (Фаза 4)
#define MOD_CAV14                       144   // Cav1.4 (CACNA1F) L-type Ca²⁺: γ=1–4 пС, БЕЗ CDI, ограниченная VDI — градуальный sustained-выброс фоторецептора
#define MOD_CABP4                       145   // CaBP4 (calcium-binding protein 4): белок-модулятор Cav1.4 — снимает ингибицию ICDI/CTM, ΔV½=−10 мВ
#define MOD_CACC                        146   // CaCC/ANO2 (TMEM16B): Ca²⁺-активируемый Cl⁻-канал (обоняние/фоторецептор), Hill n=2 EC50=4µM
#define MOD_GLAST                       147   // GLAST/GLT-1 (EAAT1/2): глиальный глутаматный транспортер — 3Na⁺+Glu внутрь/1K⁺ наружу, заменяет tau_clearance
#define MOD_GLYT1                       148   // GlyT1 (SLC6A9): глиальный транспортер глицина — 2Na⁺+1Cl⁻+Gly внутрь, net +1
#define MOD_GAT3                        149   // GAT3 (SLC6A11): глиальный транспортер GABA — 2Na⁺+1Cl⁻+GABA внутрь, net +1
#define MOD_KV31                        150   // Kv3.1 (KCNC1) fast-spiking K⁺: γ=27 пС, активация >−10 мВ, БЕЗ инактивации, fast deactivation (Grissmer 1992; Rudy & McBain 2001)
#define MOD_TASK                        151   // TASK (K2P3.1/9.1, KCNK3/9) фоновая K⁺-утечка: γ≈14–38 пС, voltage-independent, Po~0.3 (carotid body/COS-7)
#define MOD_CAN                         152   // CAN (TRPM4/5) Ca²⁺-активируемый неселективный Na⁺/K⁺ (не Ca-проводящий): γ=25–35 пС, Ca²⁺-зависимый Po (Kd 0.5 µM)
#define MOD_PKD2L1                      153   // PKD2L1 (TRPP3) Ca²⁺-предпочитающий механосенсор CSF: γ=120–200 пС, PCa/PNa≈4–6, Po=state[4] (поток)
#define MOD_KV13                        154   // Kv1.3 (KCNA3) delayed-rectifier K⁺: γ=14–24 пС, V½≈−30 мВ (типовое Kv1.x — точная Kv1.3 лит-ра не найдена), m⁴, GHK K⁺ (Fadool 2011)
#define MOD_CNG                         155   // CNG (CNGA2/A4/B1b) циклический нуклеотид-зависимый: γ=29(гомомер)/13(гетеромер) пС, лиганд cAMP (Hill 1.5), Na⁺/Ca²⁺ (JBC, Qu 2000)
#define MOD_KV11                        156   // Kv1.1 (KCNA1) delayed-rectifier K⁺: γ=9–14 пС, V½ −30 мВ, m⁴+C-type (τ~3 с), α-DTX/TEA (Grissmer 1994)
#define MOD_KV12                        157   // Kv1.2 (KCNA2) delayed-rectifier K⁺: γ=14–18 пС, V½ −27 мВ, m⁴+C-type (τ~0.4 с), α-DTX/TEA + PKC (mGluR1→Gq)
#define MOD_CAV23                       158   // Cav2.3 (CACNA1E) R-type Ca²⁺: γ=14 пС, HVA m² V½ −25, слабая CDI, SNX-482 (Randall & Tsien 1995)
#define MOD_KV34                        159   // Kv3.4 (KCNC4) A-type K⁺: V½_акт +19, V½_инакт −15, N-ball, PKC dis-inhibition, 4-AP/TEA
#define MOD_GIRK                        160   // GIRK (Kir3.x, KCNJ3/5/6/9) Gβγ-активируемый inward-rectifier K⁺: γ=30–40 пС (не V-зав.)
#define MOD_KATP                        161   // KATP (Kir6.2/SUR1) ATP-чувствительный K⁺: γ=70–80 пС, ATP-зависимый (IC50 15 µМ)
#define MOD_TRPC4                       162   // TRPC4 неселективный катионный: γ=30–42 пС, Gq/PLC-зависимый, PCa/PNa~4
#define MOD_TRPC5                       163   // TRPC5 неселективный катионный: γ=41–66 пС, Gq+Ca²⁺-зависимый, PCa/PNa~5
#define MOD_TRPM4                       164   // TRPM4 Ca²⁺-активируемый неселективный: γ=25 пС, Ca²⁺-зависимый (EC50 0.4 µМ), Na⁺/K⁺ (не Ca)
#define MOD_TRPM5                       165   // TRPM5 Ca²⁺-активируемый неселективный: γ=25 пС, Ca²⁺-зависимый (EC50 0.7 µМ, Hill 2.4), Na⁺/K⁺
#define MOD_THIK1                       166   // THIK-1 (K2P13.1/KCNK13) фоновая K⁺-утечка: γ≈5 пС, voltage-independent
#define MOD_THIK2                       167   // THIK-2 (K2P12.1/KCNK12): гомомерно НЕфункционален (ЭПР), γ≈5 пС placeholder (Rajan 2001)
#define MOD_CAV22                       168   // Cav2.2 (CACNA1B) N-type Ca²⁺: γ=2.76 пС (Ca²⁺), HVA m² V½ −20, ω-GVIA (Kirchner 2018)
#define MOD_NAV17                       169   // Nav1.7 (SCN9A) TTX-S ноцицепторный Na⁺: V½_акт −24, V½_инакт −71, m³h (Cummins 1998)
#define MOD_NAV18                       170   // Nav1.8 (SCN10A) TTX-R ноцицепторный Na⁺: V½_акт −20, V½_инакт −30, m³h медленный (Akopian 1996)
#define MOD_NAV19                       171   // Nav1.9 (SCN11A) персистентный TTX-R Na⁺: V½_акт −47, окно-ток, почти без инактивации (Cummins 1999)
#define MOD_TRPV1_CHANNEL               172   // TRPV1 тепло/капсаицин канал: γ 50–100 пС, P_Ca/P_Na≈10, EC50 капсаицин 0.7 µM (Caterina 1997)
#define MOD_TRPM8                       173   // TRPM8 холод/ментол канал: γ≈63 пС, V½ +162→+53 (ментол), EC50 4–85 µM (McKemy 2002)
#define MOD_TRPA1                       174   // TRPA1 ирритант канал: γ 87–100 пС, AITC EC50 11–64 µM (Story 2003)
#define MOD_TRPV3                       175   // TRPV3 тепло канал: γ 185–206 пС, 2-APB 30–100 µM, сенситизация (Xu 2002)
#define MOD_KCNQ4                       176   // KCNQ4 (Kv7.4) покойный K⁺ волосковых клеток: γ=2.31 пС, V½_натив −78, τ_акт 212 мс (Kharkovets 2000)
#define MOD_NAV13                       177   // Nav1.3 (SCN3A) TTX-S Na⁺: V½_акт −15.8/инакт −51.8, τ_инакт 0.65 мс (Cummins 2001)
#define MOD_KIR71                       178   // Kir7.1 (KCNJ13) фоновый K⁺ хориоидного сплетения: γ=0.05 пС, слабое выпрямление (Krapivinsky 1998)
#define MOD_TRPC67                      179   // TRPC6/7 канал фотопроводимости ipRGC: γ≈35 пС, P_Ca/Na≈5, DAG-гейт (Perez-Leighton 2011)
#define MOD_CALHM13                     180   // CALHM1/3 пора высвобождения АТФ (вкус тип II): V½ +30, потенциал-зависимая (Taruno 2013; Ma 2018)
#define MOD_OTOP1                       181   // OTOP1 протонный канал (кислый вкус тип III): H⁺-селективный, pH<6, Zn²⁺-блок (Tu 2018)
#define MOD_ENAC                        182   // ENaC эпителиальный Na⁺ (солёный вкус тип I): γ 4–5 пС, амилорид 100 нМ (Kellenberger 2002)
#define MOD_EAAT3                       183   // EAAT3/EAAC1 (SLC1A1): нейрональный глутаматный транспортер — 3Na⁺+Glu внутрь/1K⁺ наружу, net +2
#define MOD_GLYT2                       184   // GlyT2 (SLC6A5): нейрональный транспортер глицина — 3Na⁺+1Cl⁻+Gly внутрь, net +2 (Roux & Supplisson 2000)
#define MOD_GAT1                        185   // GAT1 (SLC6A1): нейрональный транспортер GABA — 3Na⁺+1Cl⁻+GABA внутрь, net +2 (Willford 2015)
#define MOD_AQP4                        186   // AQP4 аквапорин-4: водный канал (не ионный), осмотический транспорт, P_f=24e-14 см³/с (Yang & Verkman 1997)
#define MOD_CILIA                       187   // Цилиарный генератор CSF-потока (эпендима): биение ресничек 5–20 Гц → скалярный поток 0..1 (#32+#40, минимальная версия)
#define MOD_MRGPR                       188   // Mrgpr (Mas-related GPCR): метаботропный рецептор зуда, Gαq/Gβγ → Ca²⁺ → TRPA1 (Liu 2009; Wilson 2011)
#define MOD_PIEZO2                      189   // PIEZO2 механочувствительный катионный: γ 16 пС, λ50 2 мН/м, P_Ca>P_K>P_Na (Murthy 2023)
#define MOD_TMC1                        190   // TMC1/TMC2 MET канал стереоцилий: γ 50–110 пС, P_Ca/P_Na≈3.8, адаптация Ca²⁺/myosin (Pan 2013)
#define MOD_CAV33                       191   // Cav3.3 (CACNA1I) T-type Ca²⁺: γ 11 пС, V½_m=−43.8, самая медленная кинетика (Perez-Reyes 2003)
#define MOD_ROD_PHOTOTRANSDUCTION      192   // Родопсин фотокаскад (палочка): свет→R*→Gt→PDE→cGMP↓→CNG закрытие (гиперполяр.)
#define MOD_NAV12DEV                   193   // Nav1.2 developmental (Scn2a) — тадпольный аксональный Na⁺: V½_акт −23/инакт −35, m³h (Ribera & Spitzer)
#define MOD_NT_RECEPTOR_NMDA_2B         194   // GluN1/GluN2B-содержащий NMDA (эмбриональный, медленная деактивация τ_decay ~150 мс) — Xenopus dIN, см. NMDA_GluN2A_GluN2B.md
#define MOD_NAP_TADPOLE                195   // NaP-tadpole (Nav1.2-persistent): персистентный Na⁺ dIN, активация V½=−25 (Svensson 2017), без h, GHK
#define MOD_KV21                       196   // Kv2.1 (KCNB1) медленный delayed rectifier K⁺ (I_Ks): V½_акт=0, U-type инактивация, TEA IC50=10 мМ, γ=8 пС — single-spike dIN
#define MOD_PUMP_NAK_A3                197   // Na/K-АТФаза α3 (динамический насос): низкое сродство к Na⁺_i (K_m≈25 мМ), генерирует usAHP при залпе (Zahler 1997, Blanco 2005)
#define MOD_CONNEXIN_CX43              198   // Коннексин-43 (GJA1): глиальный gap junction (астроциты/радиальная глия), γ=100 пС — синцитий K-буферинга
#define MOD_NAV11                      199   // Nav1.1 (SCN1A) transient Na⁺: γ=17 пС, V½_акт −30/инакт −65, m³h, быстрый recovery (repetitive) — PV-интернейроны/проприоцепторы/таламус
#define MOD_CAV31                      200   // Cav3.1 (CACNA1G/α1G) T-type Ca²⁺: γ=7.5 пС, V½_акт −45/инакт −77, Ni²⁺ 250 µM — таламический burst (TRN/релейные)
#define MOD_CAV32                      201   // Cav3.2 (CACNA1H/α1H) T-type Ca²⁺: γ=9 пС, V½_акт −46/инакт −72, Ni²⁺ 13 µM — самая Ni-чувствительная из T-type
#define MOD_KV43                       202   // Kv4.3 (KCND3) A-type K⁺: γ=4.4 пС, V½_акт −31.7/инакт −47.4, быстрая N-type инактивация
#define MOD_KV32                       203   // Kv3.2 (KCNC2) high-threshold K⁺: γ=15 пС, V½_акт +10, non-inactivating (нет h)
#define MOD_KV33                       204   // Kv3.3 (KCNC3) high-threshold K⁺: γ=35 пС, V½_акт +1.5/инакт −27.9, A-type (медленная инакт ~200 мс)
#define MOD_KV22                       205   // Kv2.2 (KCNB2) медленный delayed rectifier K⁺: γ=14.5 пС, V½_акт +13.2/инакт −16.6, медленнее Kv2.1
#define MOD_KV14                       206   // Kv1.4 (KCNA4) A-type K⁺: γ=9.3 пС, V½_акт −21.7/инакт −51.8, N-type инактивация (N-ball)
#define MOD_KV16                       207   // Kv1.6 (KCNA6) delayed rectifier K⁺: γ=8.2 пС, V½_акт −8.1, non-inactivating (NIP-домен)
#define MOD_KIR22                      208   // Kir2.2 (KCNJ12) inward rectifier K⁺: γ=36 пС, конститутивный, Ba²⁺ 2.8 µM — чувствительнее Kir2.1
#define MOD_KCA31                      209   // KCa3.1 (KCNN4/SK4) Ca²⁺-активируемый K⁺: γ=25 пС, EC50 188 нМ, voltage-independent, TRAM-34 20 нМ
#define MOD_TRPC3                      210   // TRPC3 неселективный катионный: γ=60 пС, P_Ca:P_Na=1.62, DAG-активируемый, Pyr3 0.7 µM
#define MOD_TRPM7                      211   // TRPM7 chanzyme: γ=40 пС, P_Ca/P_Na=0.34, Mg²⁺-ингибируемый (0.7 мМ), NS8593 1.6 µM
#define MOD_TYPE_COUNT                 212

// Ионы
#define ION_NA   0
#define ION_K    1
#define ION_CA   2
#define ION_CL   3
#define ION_MG   4    // Mg²⁺ — для NMDA Mg²⁺-блока
#define ION_A    5    // A⁻ — неподвижный внутриклеточный макроанион (белки/нуклеиновые кислоты/фосфаты), z=−1, D=0
#define ION_H    6    // H⁺ — протон (pH-зависимые каналы: OTOP1), z=+1
#define ION_COUNT 7   // активное число ионов (Na, K, Ca, Cl, Mg, A, H)
#define ION_MAX   8   // физический максимум слотов (запас на будущее)

// Метаданные иона (универсальный механизм: заряд-флюкс/диффузия берут z, D отсюда, а не из хардкода)
typedef struct {
    float z;   // валентность (заряд)
    float D;   // коэффициент диффузии (мкм²/мс)
} IonInfo;

static const IonInfo ION_INFO[ION_MAX] = {
    { 1.0f, 1.33f},  // Na⁺
    { 1.0f, 1.96f},  // K⁺
    { 2.0f, 0.22f},  // Ca²⁺
    {-1.0f, 2.03f},  // Cl⁻
    { 2.0f, 0.10f},  // Mg²⁺
    {-1.0f, 0.0f },  // A⁻ (неподвижный макроанион — не диффундирует)
    { 1.0f, 9.31f},  // H⁺ (протон — самый быстрый ион, D≈9.3 мкм²/мс)
    { 0.0f, 0.0f },  // резерв
};

// Нейромедиаторы
#define NT_GLUTAMATE       0
#define NT_GABA            1
#define NT_DOPAMINE        2
#define NT_SEROTONIN       3
#define NT_ACETYLCHOLINE   4
#define NT_ADENOSINE       5
#define NT_NORADRENALINE   6
#define NT_GLYCINE         7
#define NT_HISTAMINE       8
#define NT_ENDOCANNABINOID 9
#define NT_ATP             10
#define NT_MELATONIN       11
#define NT_MAGNESIUM       12   // Mg²⁺ в синаптической щели
#define NT_NICOTINE        13   // никотин для β2-nAChR
#define NT_CAPSAICIN       14   // капсаицин — экзогенный агонист TRPV1
#define NT_COUNT           15

// Гены (ядро клетки) — пластичность + ТФ + ионные каналы + везикулярные белки
#define GENE_COUNT 57
// Пластичность (0-19, без изменений)
#define GENE_CREB        0
#define GENE_CFOS        1
#define GENE_ZIF268      2
#define GENE_ARC         3
#define GENE_BDNF        4
#define GENE_COFILIN     5
#define GENE_MMP9        6
#define GENE_TIMP        7
#define GENE_GLUR1       8
#define GENE_GLUR2       9
#define GENE_PSD95       10
#define GENE_NEUREXIN    11
#define GENE_NEUROLIGIN  12
#define GENE_DYNAMIN     13
#define GENE_CLATHRIN    14
#define GENE_INTEGRINB3  15
#define GENE_TRKB        16
#define GENE_MTOR        17
#define GENE_S6K1        18
#define GENE_4EBP        19
// Транскрипционные факторы (20-25)
#define GENE_TF_LHX3      20   // LIM-homeobox 3: dIN/motor fate, activates Nav+Kv
#define GENE_TF_ISLET1    21   // ISL1: motor/interneuron fate, activates Cav3+HCN
#define GENE_TF_PAX2      22   // Paired box 2: GABAergic/glycinergic fate
#define GENE_TF_BRN3A     23   // POU4F1: sensory neuron fate
#define GENE_TF_MNX1      24   // Motor neuron and pancreas homeobox 1: MN fate
#define GENE_TF_LMX1B     25   // LIM homeobox 1b: diencephalic/serotonergic fate
// Ионные каналы (26-35)
#define GENE_SCN1A        26   // Nav1.1: быстрый Na⁺ (HH-type)
#define GENE_KCND2        27   // Kv4.2: A-type K⁺
#define GENE_KCNQ2        28   // Kv7.2: M-current K⁺
#define GENE_KCNQ3        29   // Kv7.3: M-current K⁺
#define GENE_CACNA1G      30   // Cav3.1: T-type Ca²⁺ (PIR)
#define GENE_HCN1         31   // HCN1: Ih (fast, weak cAMP)
#define GENE_HCN2         32   // HCN2: Ih (slow, strong cAMP)
#define GENE_KCNJ2        33   // Kir2.1: inward rectifier K⁺
#define GENE_KCNK2        34   // TREK-1/K2P: leak K⁺
#define GENE_KCNMA1       35   // BK (KCa1.1): Ca²⁺+V-activated K⁺
// Везикулярные гены (36-45) — synaptic vesicle machinery
#define GENE_SYN1         36   // Synapsin I: якорь резервного пула к актину
#define GENE_VAMP2        37   // Synaptobrevin/VAMP2: SNARE-комплекс, слияние везикул
#define GENE_SNAP25       38   // SNAP-25: SNARE-комплекс
#define GENE_STX1A        39   // Syntaxin-1A: SNARE-комплекс
#define GENE_SYT1         40   // Synaptotagmin-1: Ca²⁺ сенсор (EC50≈10µM, быстрый release)
#define GENE_SYT7         41   // Synaptotagmin-7: Ca²⁺ сенсор (EC50≈1µM, asynchronous release)
#define GENE_UNC13A       42   // Munc13-1: priming factor, определяет размер RRP
#define GENE_RIMS1        43   // RIM1: active zone scaffold, docking
#define GENE_CPLX1        44   // Complexin: clamp спонтанного release
#define GENE_KCNT1        45   // Slack/Slick KNa (KCNT1/KCNT2): Na⁺-активируемый K⁺ канал
// Изоформы #8 (46-56) — новые ионные каналы (2026-10-22)
#define GENE_CACNA1H      46   // Cav3.2: T-type Ca²⁺ (самая Ni-чувствительная)
#define GENE_KCND3        47   // Kv4.3: A-type K⁺ (N-type инактивация)
#define GENE_KCNC2        48   // Kv3.2: high-threshold K⁺ (non-inactivating)
#define GENE_KCNC3        49   // Kv3.3: high-threshold K⁺ (A-type ~200 мс)
#define GENE_KCNB2        50   // Kv2.2: delayed rectifier K⁺ (медленнее Kv2.1)
#define GENE_KCNA4        51   // Kv1.4: A-type K⁺ (N-type N-ball)
#define GENE_KCNA6        52   // Kv1.6: delayed rectifier K⁺ (NIP-домен)
#define GENE_KCNJ12       53   // Kir2.2: inward rectifier K⁺
#define GENE_KCNN4        54   // KCa3.1 (SK4/IK): Ca²⁺-активируемый K⁺
#define GENE_TRPC3        55   // TRPC3: неселективный катионный (DAG)
#define GENE_TRPM7        56   // TRPM7: chanzyme (Mg²⁺/Ca²⁺)

// Типы синапсов
#define SYNAPSE_TYPE_CHEMICAL          0
#define SYNAPSE_TYPE_ELECTRICAL        1
#define SYNAPSE_TYPE_DENDRITIC_BRIDGE  2
#define SYNAPSE_TYPE_COUNT             3

// Лимиты
#define MAX_MODIFIERS_PER_SEGMENT        32
#define MAX_STATE_VARS                    8
#define MAX_PRE_AUTORECEPTORS             8
#define MAX_POST_RECEPTORS                8
#define MAX_INTRACONNECTIONS_PER_SEGMENT  16
#define MAX_SYNAPSES_PER_SEGMENT          64
#define MAX_CONSOL_EVENTS               1024
#define MAX_NEURONS                     4096

// Типы событий консолидации
#define CONSOL_EVENT_NEW_SYNAPSE          0
#define CONSOL_EVENT_DELETE_SYNAPSE       1
#define CONSOL_EVENT_DIAMETER_CHANGE      2
#define CONSOL_EVENT_RECEPTOR_CHANGE      3
#define CONSOL_EVENT_MODIFIER_DENSITY     4   // генная экспрессия → плотность канала

// Физические константы
#define FARADAY_CONST   96485.0
#define GAS_CONST       8.314f
#define CELSIUS_37      310.15f
#define PI              3.14159265f

// Коэффициенты внутриклеточной диффузии ионов (мкм²/мс)
// Источник: Kushmerick & Podolsky (1969), скорректировано для 37°C
#define DIFF_NA  1.33f     // Na⁺
#define DIFF_K   1.96f     // K⁺
#define DIFF_CA  0.22f     // Ca²⁺ (медленнее из-за буферизации)
#define DIFF_CL  2.03f     // Cl⁻
#define DIFF_MG  0.10f     // Mg²⁺ (самый медленный)
#define DIFF_ATP 0.14f     // ATP (крупная молекула)
#define DIFF_HEAT 0.0014f  // теплопроводность цитоплазмы (мм²/мс)

// Молекулярные массы нейромедиаторов (Да) для расчёта диффузии по Стоксу-Эйнштейну
// D ∝ M^(-1/3), эталон Na⁺ (M=23 Да, D=1.33 мкм²/мс)
static const float NT_MOLECULAR_MASS[NT_COUNT] = {
    147.0f,  // 0  Glutamate
    103.0f,  // 1  GABA
    153.0f,  // 2  Dopamine
    176.0f,  // 3  Serotonin
    146.0f,  // 4  Acetylcholine
    267.0f,  // 5  Adenosine
    169.0f,  // 6  Noradrenaline
    75.0f,   // 7  Glycine
    111.0f,  // 8  Histamine
    348.0f,  // 9  Endocannabinoid
    507.0f,  // 10 ATP
    232.0f,  // 11 Melatonin
    24.0f,   // 12 Mg²⁺
    162.0f   // 13 Nicotine
};

// Коэффициент диффузии нейромедиатора по Стоксу-Эйнштейну (относительно Na⁺)
#if defined(__CUDACC__)
__host__ __device__
#endif
inline float nt_diffusion_coeff(int nt_idx) {
    if (nt_idx < 0 || nt_idx >= NT_COUNT) return DIFF_NA;
    float mass = NT_MOLECULAR_MASS[nt_idx];
    if (mass < 1.0f) mass = 1.0f;
    return DIFF_NA * powf(23.0f / mass, 1.0f / 3.0f);
}

// ==================== СТРУКТУРЫ ====================
#pragma pack(push, 1)

typedef struct { float x, y, z; } Vec3;

// Модификатор — данные, загружаемые из .bion (88 байт, BION v13: density/state → double)
typedef struct {
    uint8_t  type;
    uint8_t  _pad[7];         // выравнивание density (double) до offset 8
    double   density;
    double   state[MAX_STATE_VARS];
    uint8_t  active_vars;
    uint8_t  flags;
    uint8_t  _reserved[6];    // хвост до 88 (кратность 8 для double-выравнивания массива)
} Modifier;

// Типы сегмента (segment_type, Studio-расширение):
//   1 = сома, 2 = аксон/дендрит/AIS, 3 = бутон, 4 = шипик,
//   5 = глиальный отросток (радиальная глия, ERG) — цилиндр.
#define SEG_TYPE_SOMA        1
#define SEG_TYPE_NEURITE     2
#define SEG_TYPE_BOUTON      3
#define SEG_TYPE_SPINE       4
#define SEG_TYPE_GLIAL_PROCESS 5

// Сегмент (1692 байта, BION v8, MAX_MODIFIERS_PER_SEGMENT=32)
typedef struct {
    uint32_t id;
    uint32_t neuron_id;
    Vec3     position;
    Vec3     direction;          // v4: ориентация сегмента {0,1,0} по умолчанию
    float    length;
    float    radius;
    double   V_m;                  // double (этап 1: V_m/I_buffer → double для точности малых токов)
    double   intra_ions[ION_MAX];
    double   extra_ions[ION_MAX];
    float    intra_nt[NT_COUNT];
    float    neurotransmitter[NT_COUNT];
    Modifier modifiers[MAX_MODIFIERS_PER_SEGMENT];
    uint8_t  modifier_count;
    uint8_t  segment_type;       // Studio-расширение: тип сегмента
    uint8_t  reserved1[6];       // v14: +4 байта — выравнивание I_buffer (double) на 8 (CUDA err=716)
    float    ATP_level;
    float    temperature;
    double   I_buffer;              // double (этап 1)
    float    transcription_signal;  // v5: ядерный CREB/транскрипционный сигнал (0..1)
    uint32_t intra_connection_ids[MAX_INTRACONNECTIONS_PER_SEGMENT];
    uint8_t  intra_connection_count;
    uint8_t  reserved2[7];          // v14: +4 байта — sizeof(Segment) кратен 8 (массивы Segment double-выровнены)
    float    ER_Ca2;                // v10: [Ca²⁺] в эндоплазматическом ретикулуме (mM)
} Segment;

// Нейрон
typedef struct {
    uint32_t id;
    uint32_t segment_start;
    uint32_t segment_count;
    uint32_t soma_index;
    float    threshold;
    uint8_t  type;
    uint8_t  reserved[3];
} Neuron;

// Ядро клетки — генная система консолидации/реконсолидации (412 байт, GENE_COUNT=46)
typedef struct {
    uint32_t neuron_id;                     // offset 0 — привязка к нейрону
    uint8_t  enabled;                       // offset 4 — 0=консолидация выключена
    uint8_t  consolidation_mode;            // offset 5 — 0=OFF,1=консол,2=реконсол,3=оба
    uint8_t  _pad0[2];                      // offset 6
    uint32_t consolidation_interval_ticks;  // offset 8 — 0=глобальный default
    uint32_t last_consolidation_tick;       // offset 12
    float    gene_activity[GENE_COUNT];     // offset 16 — мРНК (46×4=184 байта)
    float    protein_level[GENE_COUNT];     // offset 200 — белки (46×4=184 байта)
    float    protein_synthesis_rate;        // offset 384
    float    pcreb_level;                   // offset 388 — кэш pCREB из CREB(79)
    float    erk_level;                     // offset 392 — кэш pERK из MAPK_ERK(87)
    float    nfkb_level;                    // offset 396 — кэш NF-κB из IKK_NFKB(88)
    float    mtorc1_level;                  // offset 400 — кэш mTORC1 из MTORC1(89)
    float    amino_acid_level;              // offset 404 — доступность аминокислот
    uint8_t  _reserved[4];                  // offset 408 → total 412
} Nucleus;

// Внутриклеточная связь (осевой ток — Нернст-Планк, без Ом-резистора)
typedef struct {
    uint32_t segment_a;
    uint32_t segment_b;
    uint8_t  is_intracellular;  // 1 = внутриклеточная связь (сома↔аксон↔дендрит), 0 = EXTRA_ONLY (глия↔нейрон)
    uint8_t  reserved[3];
    uint32_t _pad;  // v9: 16-byte alignment to prevent XMM load overflow
} IntraConnection;

// Синапс
typedef struct {
    uint32_t id;
    uint32_t pre_segment;
    uint32_t post_segment;
    uint8_t  type;
    uint8_t  reserved[3];
    float    weight;
    float    delay;
    float    pre_area;
    float    post_area;
    Modifier pre_autoreceptors[MAX_PRE_AUTORECEPTORS];
    uint8_t  pre_autoreceptor_count;
    uint8_t  reserved2[7];   // v14: +4 байта — выравнивание post_receptors на 8 (double density/state, CUDA err=716)
    Modifier post_receptors[MAX_POST_RECEPTORS];
    uint8_t  post_receptor_count;
    uint8_t  reserved3[3];
    float    release_probability;
    float    release_amount;
    float    cleft_neurotransmitter[NT_COUNT];
    float    facilitation;
    float    depression;
    float    tau_clearance;
    uint8_t  refractory;
    uint8_t  reserved4[7];   // v14: +4 байта — sizeof(Synapse) кратен 8 (массивы double-выровнены)
    // ── Vesicle dynamics (BION v7) ───────────────────────────────────────
    float    RRP;              // readily-releasable pool (везикулы, штуки)
    float    RRP_max;          // максимальный RRP (Munc13-1-зависимый baseline)
    float    RRP_recovery;     // priming rate (ms⁻¹, Ca²⁺/CaMKII-зависимая)
    float    bouton_Ca;        // v11: [Ca²⁺] микродомена пресинаптического бутона (mM) — триггер выброса
    float    spine_Ca;         // v12: [Ca²⁺] микродомена постсинаптического шипика (mM) — локальная пластичность
    float    spine_camkii;     // v12: CaMKII-активность в шипике (0..1) — локальная LTP (Hill n=4, Kd=1µM)
    float    spine_calcineurin;// v12: кальциневрин-активность в шипике (0..1) — локальная LTD (Hill n=2, Kd=0.05µM)
} Synapse;

// Воксель
typedef struct {
    float ions[ION_MAX];
    float neurotransmitters[NT_COUNT];
} Voxel;

// Заголовок .bion
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t  mode;
    uint8_t  diffusion_type;
    uint16_t reserved;
    uint32_t num_neurons;
    uint32_t num_segments;
    uint32_t num_intra_connections;
    uint32_t num_synapses;
    uint32_t grid_dim_x;
    uint32_t grid_dim_y;
    uint32_t grid_dim_z;
    float    voxel_size;
    uint64_t timestamp;
    char     build_id[16];
    uint32_t num_nuclei;          // v5: количество ядер (0 если нет данных консолидации)
    uint32_t nucleus_version;     // v5: версия формата ядра (1)
    // v10: size-based self-describing format — forward/backward compatible
    uint32_t file_sz_segment;     // sizeof(Segment) в этом файле
    uint32_t file_sz_synapse;     // sizeof(Synapse)
    uint32_t file_sz_nucleus;     // sizeof(Nucleus)
    uint32_t file_sz_intraconnection; // sizeof(IntraConnection)
    uint32_t file_sz_neuron;      // sizeof(Neuron)
    uint32_t file_sz_modifier;    // sizeof(Modifier)
} NetworkHeader;

// Структуры стимуляции (Studio → Core API)
#define MAX_PENDING_STIMULI 256

typedef struct {
    uint32_t segment_id;
    float    amplitude_pA;
    float    duration_ms;
    uint64_t start_tick;
} CurrentStimulus;

typedef struct {
    uint32_t segment_id;
    float    target_mV;
    uint64_t start_tick;
    uint64_t end_tick;
} VoltageClamp;

typedef struct {
    uint32_t segment_id;
    float    start_pA;
    float    end_pA;
    uint64_t start_tick;
    uint64_t end_tick;
} RampStimulus;

typedef struct {
    uint32_t segment_id;
    int32_t  ion_idx;
    float    amount_mM;
    uint8_t  extracellular;
    uint8_t  _pad[3];
} IonInjection;

typedef struct {
    uint32_t segment_id;
    int32_t  nt_idx;
    float    amount;
    uint8_t  intracellular;
    uint8_t  _pad2[3];
} LigandInjection;

// Механо-стимул (task #46): отдельный канал стимула — прямая запись
// значения в state[state_idx] модификатора mod_type на сегменте segment_id.
// Применяется детерминированно в начале тика (шаг 1), устраняя гонку
// GUI↔simThread варианта 1 (прямая запись state[] из GUI-потока).
typedef struct {
    uint32_t segment_id;
    uint8_t  mod_type;
    int32_t  state_idx;
    float    value;
} MechanoStimulus;

// ── GPU-буфер стимулов (USM, передаётся в кернел) ─────────────────────
// Содержит все 5 типов стимулов + счётчики + текущий тик.
// Размещается через malloc_shared / cudaMallocManaged.
// Кернел читает и применяет стимулы параллельно с токами модификаторов.
#define GPU_MAX_STIMULI 256  // совпадает с MAX_PENDING_STIMULI
typedef struct {
    uint32_t current_stim_count;
    uint32_t vclamp_count;
    uint32_t ramp_count;
    uint32_t ion_inj_count;
    uint32_t lig_inj_count;
    uint64_t current_tick;
    CurrentStimulus current_stim[GPU_MAX_STIMULI];
    VoltageClamp    vclamps[GPU_MAX_STIMULI];
    RampStimulus    ramps[GPU_MAX_STIMULI];
    IonInjection    ion_inj[GPU_MAX_STIMULI];
    LigandInjection lig_inj[GPU_MAX_STIMULI];
} GpuStimulusData;

// Событие структурной пластичности от потока консолидации
typedef struct {
    uint8_t  event_type;       // CONSOL_EVENT_*
    uint8_t  _pad[3];
    uint32_t neuron_id;        // какой нейрон
    uint32_t segment_a;        // pre-сегмент (для синапса) или сегмент (для диаметра)
    uint32_t segment_b;        // post-сегмент (0 для диаметра)
    float    param1;           // новый диаметр / вес синапса / плотность рецептора
    float    param2;           // post_area / тип рецептора
    float    param3;           // pre_area / тип нейромедиатора
    uint64_t apply_tick;       // когда применить
} ConsolidationEvent;

// ── Информация о GPU-устройстве (для MSG_LIST_DEVICES) ──
struct GpuDeviceInfo {
    char     name[128];
    uint32_t vendor_id;
    uint8_t  is_cpu;             // 1 = CPU-устройство (SYCL host)
    uint64_t global_mem_bytes;
    uint32_t max_compute_units;
    int32_t  device_index;       // индекс для gpu_backend_create_on_device()
    char     backend_type[16];   // "intel" или "cuda" — из какого .so
};

#pragma pack(pop)

// Проверки размеров для бинарной совместимости с BioNeuronStudio
static_assert(sizeof(Modifier)            == 88,  "Modifier must be 88 bytes (v13: density/state double)");
static_assert(sizeof(Segment)             == 3216, "Segment must be 3216 bytes (v14: I_buffer 8-aligned)");
static_assert(sizeof(Synapse)             == 1568, "Synapse must be 1568 bytes (v14: post_receptors 8-aligned)");
static_assert(sizeof(IntraConnection)     == 16,  "IntraConnection must be 16 bytes (v9: +_pad)");
static_assert(sizeof(Neuron)              == 24,  "Neuron must be 24 bytes");
static_assert(sizeof(Nucleus)             == 500, "Nucleus must be 500 bytes (GENE_COUNT=57)");
static_assert(sizeof(ConsolidationEvent)  == 36,  "ConsolidationEvent must be 36 bytes");
static_assert(sizeof(NetworkHeader)       == 100, "NetworkHeader must be 100 bytes (v10: +file_sz fields)");
