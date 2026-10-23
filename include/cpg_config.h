// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// cpg_config.h — CPG template с Cl⁻ динамикой + aIN/GABA-B + gap junctions
#pragma once
#include "types.h"
#include "bion_format.h"
#include <vector>
#include <utility>
#include <cstdio>

namespace cpg {

// ── Нейрональные плотности ──
constexpr float PUMP     = 1500.0f;  // Na/K-насос α1 (тонический): молекул/µm² (литературная плотность сомы, k_cat=75 с⁻¹)
constexpr float PUMP_A3  = 1500.0f;  // Na/K-насос α3 (динамический usAHP): молекул/µm² — K_Na=25 мМ (Zahler 1997)
constexpr float PUMP_CIN  = 1500.0f; // Na/K-насос: молекул/µm² (литературная плотность сомы, k_cat=75 с⁻¹)
constexpr float K2P      = 4.17f;  // density=G·10/(γ·Po): 0.5 мС/см², γ=100пС, Po=0.012 (TREK-1)
constexpr float KIR_DIN  = 0.033f; // density=G·10/γ: 0.1 мС/см², γ=30пС (Kir2.x)
constexpr float KIR_CIN  = 0.1f;
constexpr float NALCN    = 0.11f;  // density=G·10/γ: 0.3 мС/см², γ=27пС (Na-утечка)
constexpr float MN_K2P   = 3.75f;  // MN: 0.45 мС/см² (72% gLeak, Rekling 2000) — density=G·10/(γ·Po), γ=100, Po=0.012
constexpr float MN_CL_LEAK = 1.2f;   // MN: 0.30 мС/см² — гиперполяризующий Cl-шунт (E_Cl=−87), покой −61
constexpr float MN_NALCN_DENS = 0.06f; // MN: 0.16 мС/см² NALCN Na-утечка — подгон покоя −61 после Cl-leak GHK, density=G·10/γ, γ=27
constexpr float MN_PUMP = 1500.0f;      // Na/K-насос: молекул/µm² (литературная плотность сомы, k_cat=75 с⁻¹)
constexpr float DIN_NALCN = 0.08f; // dIN: V_rest=−51 (Sautois). 0.3→0.22 мС/см² — Na-утечка меньше, K2P-доминирующий покой
constexpr float KCC2     = 0.093f; // Cl⁻ dynamics: KCC2 out = Cl-leak in (точный Cl-баланс), электронейтральный
// cIN leak (Sautois gLeak=4.85 нС, V_leak=−60): K2P 0.975 + NALCN 0.237 мС/см²
// (у cIN gLeak больше, чем у dIN 3.68 нС — свой per-neuron leak, а не общий K2P/NALCN)
constexpr float CIN_K2P   = 8.125f; // density=G·10/(γ·Po): 0.975 мС/см²
constexpr float CIN_NALCN = 0.088f; // density=G·10/γ: 0.237 мС/см²

constexpr float NAP_DIN    = 10.6f;  // чистый Svensson — без плато dL (NaP 22→10.6)
constexpr float CA_DIN     = 0.04f;  // density=G·10/γ: 0.1 мС/см², γ=25пС (Cav1.3) — снижено (Ca-плато)
constexpr float CA_LEAK    = 0.01f;  // density=G·10/γ: 0.05 мС/см², γ=50пС (TRPC7)
constexpr float CA_BUF     = 0.5f;
constexpr float CAV21_DENS = 100.0f;   // пресинаптический Ca²⁺ (Cav2.1, γ=15пС) — триггер выброса
constexpr float PMCA_DENS  = 300.0f;   // PMCA: молекул/µm² (Stauffer 1995 ~300)
constexpr float PIR_TCA    = 0.28f;  // density=G·10/γ: 0.2 мС/см², γ=7.2пС (Cav3.2)
constexpr float PIR_HCN    = 120.0f;  // density=G·10/γ: 12 мС/см², γ=1пС (HCN1 Ih) — Picton 2018: Ih сильный ~12мС/см² (60→120)
constexpr float NMDAR_DENS = 0.250f;    // NMDA-рецептор на соме dIN (bath-NMDA → пейсмейкер ~10 Гц)
constexpr float KV42_DENS  = 0.4f;    // Kv4.2 A-type K (I_A) на соме dIN — де-инактивация → usAHP (Heitler 2025)
constexpr float KV21_DENS  = 2.595f;  // Kv2.1 (I_Ks) медленный DR — single-spike; density = 0.75·10·2.767/8 (Sautois gKs, γ=8)
constexpr float KV11_DENS  = 0.346f;  // Kv1.1 (I_Kf) быстрый DR — реполяризация; density = 0.125·10·2.768/10 (Sautois gKf, γ=10)
constexpr float CIN_KV11_DENS = 20.75f;  // Kv1.1 (I_Kf) cIN — gKfast=7.5 мС/см², γ=10
constexpr float CIN_KV21_DENS = 17.29f;  // Kv2.1 (I_Ks) cIN — gKslow=5 мС/см², γ=8
constexpr float CIN_KV42_DENS = 41.51f;  // Kv4.2 (A-type) cIN — gA=7.5 мС/см², γ=5
constexpr float MN_KV11_DENS  = 5.534f;  // Kv1.1 (I_Kf) MN/aIN — gKfast=2 мС/см², γ=10
constexpr float MN_KV21_DENS  = 0.865f;  // Kv2.1 (I_Ks) MN/aIN — gKslow=0.25 мС/см², γ=8
constexpr float RB_KV11_DENS  = 1.038f;  // Kv1.1 (I_Kf) RB — gKfast=0.375 мС/см², γ=10
constexpr float RB_KV21_DENS  = 6.918f;  // Kv2.1 (I_Ks) RB — gKslow=2 мС/см², γ=8
constexpr float SERCA_DIN  = 700.0f;   // SERCA: молекул/µm² (ER ~700)
constexpr float IP3R_DIN   = 0.15f;
constexpr float RYR_DIN    = 0.4f;

constexpr float SK_CIN     = 6.0f;  // BIOREF g_max 0.5-5 мС/см² (SK/KCa2.x) — AHP cIN, рабочее 6.0 (CPG FINAL)
constexpr float CIN_CA_BUF = 1.0f;  // Ca-буфер cIN: 0.1→1.0 — гасит тонический NMDA-Ca (Ca_i 209→~1мкМ), SK не насыщается
constexpr float CIN_CAV13 = 0.3f;  // соматический Cav1.3 (L-type) → Ca²⁺ для SK-AHP (0.02→0.3: быстрее Ca-вток → быстрее AHP)
constexpr float CIN_PMCA = 300.0f; // PMCA cIN: 3000→300 — release-баг давал тонический NMDA-Ca, теперь эффлюкс не должен пересиливать SK-AHP
constexpr float CIN_NCX  = 30.0f;  // NCX cIN: 300→30 — электрогенный Na-вток пересиливал SK-AHP
constexpr float PIR_TCA_C  = 0.1f;

constexpr float NAK_AIN    = 1.0f;
// Na-плотности по Sautois 2007 Table 2 (gNa нС → мС/см² при C=4пФ/area 400µm²):
// density=G·10/γ, γ=20.8 пС (Nav1.2 developmental). cIN gNa=500нС=125 мС/см², aIN=37.5, MN=27.5, RB=30.
constexpr float CIN_NA_DENS = 60.0f;  // cIN gNa=125 мС/см² (Sautois 500 нС) — было 1.18 (ошибка конверсии INaT)
constexpr float AIN_NA_DENS = 18.0f;  // aIN gNa=37.5 мС/см² (Sautois 150 нС)
constexpr float MN_NA_DENS  = 13.2f;  // MN gNa=27.5 мС/см² (Sautois 110 нС)
constexpr float RB_NA_DENS  = 14.4f;  // RB gNa=30 мС/см² (Sautois 120 нС) — было 3.62
constexpr float K2P_AIN    = 3.75f;  // = MN_K2P (0.45 мС/см², 72% gLeak Rekling 2000) — aIN в группе aIN/MN (Sautois), покой −55, НЕ тонически активен
constexpr float SK_AIN     = 0.5f;   // снижен AHP — aIN быстро firing на excitation
constexpr float PIR_TCA_A  = 0.15f;

constexpr float SK_MN = 0.18f;
constexpr float BK_MN = 0.15f;

// ── RB (Rohon-Beard) тактильный сенсор ──
// Sautois 2007 Table 2: single-spike (RB_NAK: Na m³h + Kslow I_Ks), покой −70, глутаматергический.
constexpr int   RB_PER_SIDE = 10;    // rb_per_hemisegment (как develop: 10-11 био)
constexpr float R_RB  = 8.0f;        // радиус сомы (мкм)
constexpr float V_RB  = -70.0f;      // Sautois RB V_rest −70
constexpr float RB_KCC2 = 0.8f;      // сильный Cl-экструдер → гиперполяр. E_Cl (покой −70)
constexpr float RB_CL_LEAK = 0.1f;   // низкая Cl-утечка (blueprint RB())
constexpr float RB_PHYS_SENSOR = 0.8f; // механотрансдукция касания
constexpr float RB_NALCN = 0.03f;    // малый Na-leak (баланс электро-генного насоса → покой −70)

constexpr float KIR41   = 0.085f; // Kir4.1 (отросток): НЕЗРЕЛАЯ ~8.5× ниже зрелой G~1 мС/см² (захват K, γ=23 пС, Po=0.6)
constexpr float PUMP_GL = 600.0f;    // глиальная α2: молекул/µm² (Stirling & Sarthy 1985: глия ~600/µm²)
constexpr float K2P_GL  = 16.7f;  // TREK-1/TWIK-1 глии: G≈2 мС/см² (Zhou 2009) — канал ВЫБРОСА K, НЕ снижать
constexpr float KIR_GL  = 0.35f;  // Kir сомы (незрелая): зрелый 4.06→незрелый 0.474 нС/пФ (~8.5×, NEURON_GLIA.md)

// Глиальные Na-сопряжённые NT-транспортёры (электрогенный Na-вход, NEURON_GLIA.md):
// GLAST net+2 (3Na/1K), GlyT1/GAT3 net+1 (2Na/1Cl). density × I_max(=2 µA/см²) — калибровка (этап 6, задача #20).
constexpr float GLIA_GLAST_DENS = 1.0f;
constexpr float GLIA_GLYT1_DENS = 1.0f;
constexpr float GLIA_GAT3_DENS  = 1.0f;
constexpr float GLIA_NAP_DENS   = 3.0f;   // NaP (Nav1.6 persistent, Sontheimer 1994) — покоящаяся Na-проводимость незрелой глии (RMP ~−51)
constexpr float GLIA_NKCC1_DENS = 0.1f;   // NKCC1 (SLC12A2) — поддерживает [Cl]i~25 мМ (не накачивает K), малая плотность

// ── Сеть ──
constexpr int GLIA_PER_SIDE = 6;
constexpr int N_SEGMENTS    = 6;       // полный CPG: 6 сегментов
constexpr int N_DIN_PER_SIDE= 3;
constexpr int N_CIN_PER_SIDE= 3;
constexpr int N_AIN_PER_SIDE= 1;
constexpr int N_MN_PER_SIDE = 1;
// Морфология: каждый нейрон = сома + AIS + аксон (типы 1/2/2). Бутон — МИКРОДОМЕН пресинапса
// (Synapse.bouton_Ca), НЕ отдельный V_m-компартмент. AIS — тонкий аксонный холмик (место инициации ПД).
// Локализация: Nav1.2(высок.)+Kv1.1(низ.) → AIS; Cav2.1+Ca → аксон (пресинапс); Na/K/Ih → сома.
// dIN УНИПОЛЯРНЫЙ, БЕЗ ДЕНДРИТА (Li 2001 J Comp Neurol 441:248, PMID 11745648) — Ih (HCN1) на соме.
constexpr int SEGS_PER_NEURON = 3;
constexpr int SOMA = 0, AIS = 1, AXON = 2;  // смещения внутри нейрона

// Раскладка нейронов per-сегмент (в «нейронных слотах», до ×SEGS_PER_NEURON):
//   L: dinL(1) cinL(1) ainL(1) mnL(1) | R: dinR(1) cinR(1) ainR(1) mnR(1) | rbL(10) rbR(10)
constexpr int DINL_BASE = 0;
constexpr int CINL_BASE = DINL_BASE + N_DIN_PER_SIDE;
constexpr int AINL_BASE = CINL_BASE + N_CIN_PER_SIDE;
constexpr int MNL_BASE  = AINL_BASE + N_AIN_PER_SIDE;
constexpr int DINR_BASE = MNL_BASE  + N_MN_PER_SIDE;
constexpr int CINR_BASE = DINR_BASE + N_DIN_PER_SIDE;
constexpr int AINR_BASE = CINR_BASE + N_CIN_PER_SIDE;
constexpr int MNR_BASE  = AINR_BASE + N_AIN_PER_SIDE;
constexpr int RBL_BASE  = MNR_BASE  + N_MN_PER_SIDE;
constexpr int RBR_BASE  = RBL_BASE  + RB_PER_SIDE;
constexpr int TOTAL_NEURONS_PER_SEG = RBR_BASE + RB_PER_SIDE;  // 8 + 10 + 10 = 28
constexpr int STRIDE = TOTAL_NEURONS_PER_SEG * SEGS_PER_NEURON + GLIA_PER_SIDE * 2 * 2;  // 2 стороны × 2 сегмента (сома+отросток) = 24 глиальных сегмента
constexpr int NEURONS_PER_SIDE = N_DIN_PER_SIDE + N_CIN_PER_SIDE + N_AIN_PER_SIDE + N_MN_PER_SIDE + RB_PER_SIDE;

// neuron_seg(seg, base, part): base — базовый нейронный слот, part — сома/AIS/аксон.
inline int neuron_seg(int seg, int base, int part) { return seg*STRIDE + base*SEGS_PER_NEURON + part; }
// Хелперы: индекс СЕГМЕНТА (сома) нейрона
inline int dinL_idx(int seg, int n=0) { return neuron_seg(seg, DINL_BASE, SOMA) + n*SEGS_PER_NEURON; }
inline int cinL_idx(int seg, int n=0) { return neuron_seg(seg, CINL_BASE, SOMA) + n*SEGS_PER_NEURON; }
inline int ainL_idx(int seg)          { return neuron_seg(seg, AINL_BASE, SOMA); }
inline int mnL_idx(int seg, int n=0)  { return neuron_seg(seg, MNL_BASE,  SOMA) + n*SEGS_PER_NEURON; }
inline int dinR_idx(int seg, int n=0) { return neuron_seg(seg, DINR_BASE, SOMA) + n*SEGS_PER_NEURON; }
inline int cinR_idx(int seg, int n=0) { return neuron_seg(seg, CINR_BASE, SOMA) + n*SEGS_PER_NEURON; }
inline int ainR_idx(int seg)          { return neuron_seg(seg, AINR_BASE, SOMA); }
inline int mnR_idx(int seg, int n=0)  { return neuron_seg(seg, MNR_BASE,  SOMA) + n*SEGS_PER_NEURON; }
inline int rbL_idx(int seg, int n=0)  { return neuron_seg(seg, RBL_BASE,  SOMA) + n*SEGS_PER_NEURON; }
inline int rbR_idx(int seg, int n=0)  { return neuron_seg(seg, RBR_BASE,  SOMA) + n*SEGS_PER_NEURON; }
// Хелперы: AIS и аксон (пресинапс) нейрона
inline int ais_of(int soma_idx)  { return soma_idx + AIS; }
inline int axon_of(int soma_idx) { return soma_idx + AXON; }

// ── Синапсы ──
constexpr float GLY_W   = 40.0f;  // 60→40: умеренная глицинергическая ингибиция
constexpr float D2C_W   = 4.0f;   // d→c: оптимум ~4.0 (надёжное переключение cIN)
constexpr float D2D_W   = 3.0f;
constexpr int   D2D_N   = 1;
constexpr float CX36_DENS = 100.0f; // Cx36: ~100 коннексонов/контакт → g ≈ 100×14пС = 1.4 нС (gap junction)
constexpr float CX43_DENS = 30.0f;  // Cx43: глиальный синцитий → g ≈ 30×100пС = 3.0 нС (astrocyte coupling, Veenstra 1995)
constexpr float MN_CX36_DENS = 1.0f; // Cx36 на MN: coupling MN↔MN ~8.5% (Li 2009) — итерации: 100→20.7%, 41→15.5%, 22→14.8%, 13→14.6%, 7→13.6%, 4→13.3%, 2→?
constexpr float D2M_W   = 2.0f;
constexpr int   GLY_N   = 2;      // 4→2: био 1-2 синапса на dIN, умеренный reset
constexpr int   EXC_N   = 1;
constexpr float C2C_W   = 6.0f;
constexpr int   C2C_N   = 2;
constexpr float C2C_GLY_REC = 48.0f;  // γ=60 пС: ×2.18 (c→c глицин)
constexpr float GLY_RP  = 0.7f;
constexpr float GLY_TAU = 10.0f;
constexpr float GLY_REC = 160.0f;  // γ=60 пС: 80→160 даёт 4 switches (пик: 140-160→4, 180+→2)
constexpr float EXC_RP  = 0.6f;
constexpr float EXC_TAU = 5.0f;
constexpr float AMPA_R  = 2000.0f;  // γ=8 пС (аудит #83): ×25 к P_max=1.0 — dIN→cIN; 1000→2000: GHK поднял порог cIN, cL не спайкал
constexpr float NMDA_R  = 300.0f;  // γ=50 пС (аудит #83): ×25 к P_max=1.0
constexpr float D2D_AMPA_R = 2000.0f;  // γ=8 пС: = AMPA_R (Ferrario: din→din ampa = din→cin ampa = 0.593 нС). 76→2000: было 26× слабее d→c
constexpr float D2D_NMDA_R = 79.0f;   // γ=50 пС: Ferrario nmda 0.15 нС ≈ 1/4 ampa: 2000×(0.15/0.593)×(8/50) ≈ 79
constexpr float MN_RP   = 0.7f;
constexpr float MN_TAU  = 10.0f;
constexpr float MN_AMPA = 2.0f;
constexpr float MN_NICOTINIC_DENS = 2.0f;  // nAChR (α3β4/α4β2) на MN — плотность НУЖНА ЛИТЕРАТУРА (γ 39-58 пС, Zhao 1998)
constexpr float RB_DIN_W    = 1.5f;   // RB→dIN AMPA вес (develop tadpole_develop.cpp:3560 «2 контакта, множественные бутоны»)
constexpr float RB_DIN_AMPA = 40.0f;  // RB→dIN AMPA плотность (сильный сенсорный вход — как d→c AMPA_R)
constexpr float PRE_AREA = 1.5f;
constexpr float POST_AREA= 1.5f;
constexpr float CL_LEAK = 0.8f;   // пассивная Cl⁻ утечка: G=0.2 мС/см², density=G·10/(γ·Po), γ=2.5пС CLC-2 (GABA-шунт, НЕ весь gLeak — gLeak=K2P+NALCN)

// ── GABA-B (aIN→цель) — K⁺-торможение ──
constexpr float D2A_W   = 4.0f;   // d→a AMPA (вес = D2C_W; сила задана плотностью ниже)
constexpr int   D2A_N   = 1;
constexpr float AIN_AMPA_DENS = 113.0f; // = 0.17× AMPA_R/N (Ferrario: din→ain 0.1 нС ≈ 0.17× din→cin 0.593)
constexpr float A2C_W   = 2.0f;   // aIN→cIN GABA-B (K⁺-механизм, не Cl⁻)
constexpr int   A2C_N   = 1;
constexpr float A2D_W   = 2.0f;   // aIN→dIN GABA-B (рекуррентное торможение — баланс восходящей d→d)
constexpr int   A2D_N   = 1;
constexpr float GABA_B_TAU = 80.0f;

// ── RRP ──
constexpr float RRP_GLY_MAX = 20.0f;  constexpr float RRP_GLY_REC = 0.2f;
constexpr float RRP_AMPA_MAX = 25.0f; constexpr float RRP_AMPA_REC = 0.06f;
constexpr float RRP_GABA_MAX = 15.0f; constexpr float RRP_GABA_REC = 0.05f;

// ── V_m ──
constexpr float V_DIN  = -52.0f;
constexpr float V_CIN  = -75.0f;   // гиперполяризован — требуется сильный d→c AMPA для firing
constexpr float V_AIN  = -50.0f;  // очень близко к порогу -40mV — должен firing
constexpr float V_MN   = -61.0f;  // Sautois 2007 Table 2: MN V_leak = −61 мВ
constexpr float V_GLIA = -80.0f;
constexpr float V_KICK = -50.0f;
constexpr float V_KICK_DIN = -20.0f;  // kick dIN: выше порога (−28 мВ) → гарантированный одиночный спайк
constexpr float V_HYPER_DIN = -75.0f; // контралатеральный dIN: гиперполяризован → Ih-ребаунд (HCN)

// ── Геометрия ──
constexpr float R_DIN  = 5.0f;
constexpr float R_CIN  = 4.5f;
constexpr float R_AIN  = 4.0f;
constexpr float R_MN   = 6.0f;
constexpr float R_GLIA = 3.0f;
// Радиальная глия (ERG): neuron_id = 0xFFFFFFFE (отличен от астроцита 0xFFFFFFFF).
constexpr uint32_t GLIA_RADIAL_ID = 0xFFFFFFFEu;
constexpr uint32_t CSF_ID = 0xFFFFFFFDu;  // СМЖ (ликвор) — резервуар, не клетка
constexpr float GLIA_PROC_R = 0.5f;   // радиус радиального отростка (мкм)
constexpr float GLIA_PROC_L = 30.0f;  // длина радиального отростка (мкм, к пие)
constexpr float Y_DIN  = 70.0f;
constexpr float Y_CIN  = 45.0f;
constexpr float Y_AIN  = 55.0f;
constexpr float Y_MN   = 20.0f;
constexpr float Y_GLIA = 50.0f;
constexpr float Y_RB   = 90.0f;  // RB дорсальный тактильный сенсор (как develop y=90)
constexpr float Z_SPAC = 150.0f;

// ── Морфология AIS + аксона (биологическая, per-тип-нейрона) ──
// AIS (аксонный холмик): тонкий конический, L 10–60 мкм (Kole & Stuart 2012). Nav1.2 (тадпольный
// спинальный Na) — плотность калибруется этапом 6. См. notes/modifier_refs/AIS.md, NAV12_142.md.
constexpr float AIS_R = 0.5f;     // радиус AIS (мкм) — проксимальный аксон, НЕ тоньше дистального (AXON_R)
constexpr float AIS_L = 20.0f;    // длина AIS (мкм, в диапазоне 10–60)
constexpr float AXON_R = 0.5f;     // радиус аксона (мкм)
constexpr float AXON_L = 50.0f;    // длина аксона (мкм)
// Плотности (калибруются этапом 6): Nav1.2 (GHK) + Kv1.1 (OHM)
constexpr float AIS_NAV12_DENS = 60.0f;  // Nav1.2 в AIS: G≈120 мС/см² (density=G·10/γ, γ=20пС) — как соматический DIN_NAK
constexpr float AIS_K2P = 2.0f;    // K2P в AIS (гиперполяризованный покой — h Nav1.2 доступен); 8→2: меньше шунт, дотягивает до порога
constexpr float AIS_NALCN = 0.04f; // NALCN в AIS (меньше Na-утечки — покой ~-75)
constexpr float AIS_KV_DR_DENS = 6.0f;    // Kv1.1 в AIS (реполяризация): G≈120 мС/см² — вровень с Nav1.2
constexpr float AXON_NAV12_DENS = 50.0f;  // Nav1.2: плотность аксонального Na⁺
constexpr float AXON_KV_DR_DENS = 6.0f;   // Kv1.1: G≈120 мС/см² (реполяризация, вровень с Na)

// ═══════════════════════════════════════════════════════════════
// БИЛДЕРЫ МОДИФИКАТОРОВ
// ═══════════════════════════════════════════════════════════════

inline std::vector<std::pair<uint8_t, float>> dIN_soma_mods() {
    // Сома dIN (Sautois 2007, Picton 2018, Svensson 2017): Na/K-спайк (DIN_NAK),
    // INaP, K-leak (K2P), Ih (HCN1), насос, KCC2, Cl-leak, gap junction (Cx36).
    // NALCN (млекопитающий Na-leak) УБРАН — Na-утечку dIN даёт NaP (Svensson 2017).
    // Ih (HCN1) НА СОМЕ: dIN униполярный, БЕЗ дендрита (Li 2001) — см. DENDRITE_MORPHOLOGY.md.
    // NMDA-рецептор на соме УБРАН (2026-09-18): bath-NMDA-пейсмейкер — экспериментальный
    // fictive-swimming драйв. Физиологический глутамат — только через синапсы (d→d, RB→dIN).
    return {{MOD_NAV12DEV, 37.64f},
            {MOD_NAP_TADPOLE, NAP_DIN},
            {MOD_KV11, KV11_DENS},      // Kv1.1 (I_Kf) быстрый DR — реполяризация спайка
            {MOD_KV21, KV21_DENS},      // Kv2.1 (I_Ks) медленный DR — single-spike
            {MOD_K2P_LEAK, K2P},
            {MOD_HCN1, PIR_HCN},        // Ih (HCN1) — Picton 2018: dIN униполярный, Ih на соме (не на дендрите)
            {MOD_PUMP_NAK, PUMP},
            {MOD_PUMP_NAK_A3, PUMP_A3},  // α3 динамический насос → usAHP (single-spike)
            {MOD_KCC2, KCC2},
            {MOD_IONOTROPIC_CL, CL_LEAK},
            {MOD_CA_BUFFER, CA_BUF},   // буфер Ca²⁺ от NMDA-притока
            {MOD_PMCA, PMCA_DENS},     // удаление Ca²⁺ (NMDA → Ca²⁺)
            {MOD_CONNEXIN_CX36, CX36_DENS},  // gap junction (электрическая связь dIN↔dIN)
            {MOD_NT_RECEPTOR_5HT7, 1.0f},    // 5-HT7 (Gs) → cAMP → сдвиг HCN1 V½ (нейромодуляторный тонус)
            {MOD_CAMP_PKA, 1.0f}};           // пул cAMP/PKA на соме — честная cAMP-модуляция HCN1 (вместо V½-хака)
}
inline std::vector<std::pair<uint8_t, float>> dIN_ais_mods() {
    // AIS dIN: быстрый Na Nav1.2 (тадпольный спинальный, JGP 2022) — инициация ПД.
    // Литературная плотность AIS Na G=0.5 С/см² (Kole & Stuart 2012, AIS.md) → density=500.
    // DIN_NAK (single-spike Na/K) остаётся на СОМЕ, не в AIS.
    return {{MOD_NAV12DEV, 500.0f}, {MOD_K2P_LEAK, AIS_K2P}, {MOD_PUMP_NAK, PUMP}};  // NALCN убран (соматодендритный) + насос
}
inline std::vector<std::pair<uint8_t, float>> dIN_axon_mods() {
    // Аксон dIN: DIN_NAK (Na/K single-spike) + leak + Cav2.1 (пресинапс — триггер выброса).
    return {{MOD_NAV12DEV, 2.03f}, {MOD_KV11, KV11_DENS}, {MOD_KV21, KV21_DENS}, {MOD_K2P_LEAK, K2P}, {MOD_CAV21, CAV21_DENS}, {MOD_CA_BUFFER, CA_BUF}, {MOD_PMCA, PMCA_DENS}};
}

inline std::vector<std::pair<uint8_t, float>> cIN_soma_mods() {
    // Сома cIN (Sautois 2007 Table 2): Na+Kfast+Kslow+A (delayed burst) + SK (Ca²⁺-AHP).
    // SK терминирует бёрст (иначе cIN репетитивно стреляет, глицин не отпускает dIN).
    // Cav1.3 (L-type HVA Ca²⁺) — соматический источник Ca²⁺ для SK-AHP
    // (Cav2.1 пресинаптический — соматический ток вырезан, не годится).
    // Утечка K2P+NALCN. Насос + KCC2 + Cl-leak + Ca-гомеостаз.
    return {{MOD_NAV12DEV, CIN_NA_DENS},
            {MOD_KV11, CIN_KV11_DENS},  // Kv1.1 (I_Kf) — реполяризация
            {MOD_KV21, CIN_KV21_DENS},  // Kv2.1 (I_Ks) — медленный DR
            {MOD_KV42, CIN_KV42_DENS},  // Kv4.2 (A-type) — delayed burst
            {MOD_SK_CHANNEL, SK_CIN},
            {MOD_CAV13, CIN_CAV13},
            {MOD_K2P_LEAK, CIN_K2P},
            {MOD_NALCN, CIN_NALCN},
            {MOD_PUMP_NAK, PUMP_CIN},
            {MOD_KCC2, KCC2},
            {MOD_IONOTROPIC_CL, CL_LEAK},
            {MOD_CA_BUFFER, CIN_CA_BUF},
            {MOD_PMCA, CIN_PMCA},
            {MOD_NCX, CIN_NCX}};
}
inline std::vector<std::pair<uint8_t, float>> cIN_ais_mods() {
    return {{MOD_NAV12DEV, 500.0f}, {MOD_K2P_LEAK, AIS_K2P}, {MOD_NALCN, AIS_NALCN}};
}
inline std::vector<std::pair<uint8_t, float>> cIN_axon_mods() {
    return {{MOD_NAV12DEV, CIN_NA_DENS}, {MOD_KV11, CIN_KV11_DENS}, {MOD_KV21, CIN_KV21_DENS}, {MOD_KV42, CIN_KV42_DENS}, {MOD_K2P_LEAK, CIN_K2P}, {MOD_NALCN, CIN_NALCN}, {MOD_CAV21, CAV21_DENS}, {MOD_CA_BUFFER, CA_BUF}, {MOD_PMCA, PMCA_DENS}};
}

inline std::vector<std::pair<uint8_t, float>> aIN_soma_mods() {
    // Сома aIN (Sautois Table 2, группа aIN/MN): Na+Kfast+Kslow (repetitive).
    return {{MOD_NAV12DEV, AIN_NA_DENS},
            {MOD_KV11, MN_KV11_DENS},
            {MOD_KV21, MN_KV21_DENS},
            {MOD_K2P_LEAK, K2P_AIN},
            {MOD_NALCN, MN_NALCN_DENS},
            {MOD_PUMP_NAK, PUMP},
            {MOD_KCC2, KCC2},
            {MOD_IONOTROPIC_CL, CL_LEAK}};
}
inline std::vector<std::pair<uint8_t, float>> aIN_ais_mods() {
    return {{MOD_NAV12DEV, 500.0f}, {MOD_K2P_LEAK, AIS_K2P}, {MOD_NALCN, AIS_NALCN}};
}
inline std::vector<std::pair<uint8_t, float>> aIN_axon_mods() {
    return {{MOD_NAV12DEV, AIN_NA_DENS}, {MOD_KV11, MN_KV11_DENS}, {MOD_KV21, MN_KV21_DENS}, {MOD_K2P_LEAK, K2P_AIN}, {MOD_NALCN, MN_NALCN_DENS}, {MOD_CAV21, CAV21_DENS}, {MOD_CA_BUFFER, CA_BUF}, {MOD_PMCA, PMCA_DENS}};
}

inline std::vector<std::pair<uint8_t, float>> MN_soma_mods() {
    // Сома MN Xenopus (Sautois 2007 Table 2): Na+Kfast+Kslow (repetitive firing) + delayed rectifier.
    // Утечка по Rekling 2000 (K 72% / Na 9% / Cl 18%): K2P(MN_K2P)+Cl(MN_CL_LEAK),
    // Na-утечка через INaP (window MN_NAK), НЕ NALCN (в MN головастика не обоснован).
    // usAHP = Na⁺/K⁺-насос (PUMP), НЕ SK/BK — у Xenopus головастика нет Ca²⁺-зависимого mAHP
    // (SK/BK — млекопитающая черта; Sun & Dale 1998: IKCa появляется st.41/42 для burst termination).
    // + Cx36 (gap junction MN↔MN, Zhao/Wolf/Roberts 1998 — электротоническое сопряжение).
    return {{MOD_NAV12DEV, MN_NA_DENS},
            {MOD_KV11, MN_KV11_DENS},
            {MOD_KV21, MN_KV21_DENS},
            {MOD_NALCN, MN_NALCN_DENS},  // Na-утечка (V_rest −61)
            {MOD_K2P_LEAK, MN_K2P},
            {MOD_PUMP_NAK, MN_PUMP},     // насос (баланс Na_i)
            {MOD_KCC2, KCC2},
            {MOD_IONOTROPIC_CL, MN_CL_LEAK},
            {MOD_CONNEXIN_CX36, MN_CX36_DENS}};
}
inline std::vector<std::pair<uint8_t, float>> MN_ais_mods() {
    // AIS MN: Nav1.2 (тадпольный спинальный Na, G=0.5 С/см² → density=500) — инициация ПД.
    // Локализация: Na→AIS (как dIN AIS), не соматический MN_NAK.
    return {{MOD_NAV12DEV, 500.0f}, {MOD_K2P_LEAK, AIS_K2P}, {MOD_PUMP_NAK, PUMP}};  // NALCN убран (соматодендритный) + насос
}
inline std::vector<std::pair<uint8_t, float>> MN_axon_mods() {
    return {{MOD_NAV12DEV, MN_NA_DENS}, {MOD_KV11, MN_KV11_DENS}, {MOD_KV21, MN_KV21_DENS}, {MOD_K2P_LEAK, MN_K2P}, {MOD_CAV21, CAV21_DENS}, {MOD_CA_BUFFER, CA_BUF}, {MOD_PMCA, PMCA_DENS}};
}

inline std::vector<std::pair<uint8_t, float>> rb_soma_mods() {
    // Сома RB (Rohon-Beard, Winlove & Roberts 2012 in situ): single-spike Na/K (RB_NAK) +
    // механосенсор (PHYSICAL_SENSOR) + K2P-leak + насос + KCC2 + Cl + Ca-гомеостаз.
    // NALCN УБРАН — RB НЕ имеет персистентного Na⁺ (Winlove 2012: «no persistent Na»).
    return {{MOD_PHYSICAL_SENSOR, RB_PHYS_SENSOR},
            {MOD_NAV12DEV, RB_NA_DENS},
            {MOD_KV11, RB_KV11_DENS},
            {MOD_KV21, RB_KV21_DENS},
            {MOD_K2P_LEAK, K2P},
            {MOD_NALCN, RB_NALCN},
            {MOD_PUMP_NAK, PUMP},
            {MOD_KCC2, RB_KCC2},
            {MOD_IONOTROPIC_CL, RB_CL_LEAK},
            {MOD_CA_BUFFER, CA_BUF},
            {MOD_PMCA, PMCA_DENS}};
}
inline std::vector<std::pair<uint8_t, float>> rb_ais_mods() {
    // AIS RB: RB_NAK (single-spike проведение) + K2P-leak.
    return {{MOD_NAV12DEV, RB_NA_DENS}, {MOD_KV11, RB_KV11_DENS}, {MOD_KV21, RB_KV21_DENS}, {MOD_K2P_LEAK, K2P}};
}
inline std::vector<std::pair<uint8_t, float>> rb_axon_mods() {
    // Аксон RB: RB_NAK + Cav2.1 (пресинапс → выброс глутамата на dIN).
    return {{MOD_NAV12DEV, RB_NA_DENS}, {MOD_KV11, RB_KV11_DENS}, {MOD_KV21, RB_KV21_DENS}, {MOD_K2P_LEAK, K2P},
            {MOD_CAV21, CAV21_DENS}, {MOD_CA_BUFFER, CA_BUF}, {MOD_PMCA, PMCA_DENS}};
}

inline std::vector<std::pair<uint8_t, float>> radial_glia_soma_mods() {
    // Радиальная глия (ERG) сома — НЕЗРЕЛАЯ (головастик) — полный состав NEURON_GLIA.md:
    // K-покой: K2P (TREK-1) + Kir; Na-проводимость (persistent, Sontheimer 1994): NaP (Nav1.6);
    // NKCC1 — Cl-импорт незрелой глии → высокий [Cl]i → деполяризованный E_Cl (~−45) → покой ~−51;
    // Na-вход (электрогенный net+2/+1/+1): GLAST/GlyT/GAT3; гомеостаз: насос α2 + PMCA + Ca-buffer + Cl-leak.
    return {{MOD_K2P_LEAK, K2P_GL}, {MOD_GLIAL_NAK_PUMP, PUMP_GL},
            {MOD_KIR_CHANNEL, KIR_GL},
            {MOD_NKCC1, GLIA_NKCC1_DENS},
            {MOD_NAP_CHANNEL, GLIA_NAP_DENS},
            {MOD_GLAST, GLIA_GLAST_DENS}, {MOD_GLYT1, GLIA_GLYT1_DENS}, {MOD_GAT3, GLIA_GAT3_DENS},
            {MOD_PMCA, PMCA_DENS}, {MOD_CA_BUFFER, CA_BUF},
            {MOD_IONOTROPIC_CL, CL_LEAK},
            {MOD_NB_SOMA_ATP, 0.3f},
            {MOD_CONNEXIN_CX43, CX43_DENS}};  // Cx43 gap junction — синцитий K-буферинга (локальный, не all-to-all)
}
inline std::vector<std::pair<uint8_t, float>> radial_glia_process_mods() {
    // Радиальный отросток: Kir4.1 (K-буферинг, γ=23 пС, G~1 мС/см²) + Cx43 (gap junction).
    return {{MOD_GLIAL_KIR41, KIR41}, {MOD_CONNEXIN_CX43, CX43_DENS}};
}

// ═══════════════════════════════════════════════════════════════
// ХЕЛПЕРЫ
// ═══════════════════════════════════════════════════════════════

inline void init_ions(Segment& s) {
    s.intra_ions[0]=15.0f; s.intra_ions[1]=140.0f; s.intra_ions[2]=0.0001f; // мМ: Na 15, K 140, Ca 100нМ (физиол., было 50µM ×500)
    s.intra_ions[3]=5.0f;  s.intra_ions[4]=0.5f;                            // Cl 5, Mg 0.5
    s.intra_ions[5]=151.0f;                                                  // A⁻ неподвижный макроанион (белки/нуклеиновые кислоты) — электронейтральность
    s.intra_ions[6]=6.3e-5f;                                                 // H⁺ 63 нМ (pH 7.2)
    s.extra_ions[0]=140.0f; s.extra_ions[1]=3.0f; s.extra_ions[2]=1.2f;     // Na 140, K 3, Ca 1.2 — K_o=3 согласовано с bath (BATH_ION), иначе settle тянет ECS 5→3 и K-доминантный cIN тонет
    s.extra_ions[3]=130.0f; s.extra_ions[4]=2.0f;                           // Cl 130, Mg 2
    s.extra_ions[5]=0.0f;                                                    // A⁻ внеклетки нет
    s.extra_ions[6]=4.0e-5f;                                                 // H⁺ 40 нМ (pH 7.4)
    s.ATP_level = 1.0f;
    s.ER_Ca2 = 0.5f;  // v10: ER Ca²⁺ депо (mM)
    s.temperature = 37.0f;  // °C — иначе 0 → PHYSICAL_SENSOR ложный холодочувствительный ток
}

inline void apply_mods(Segment& s, const std::vector<std::pair<uint8_t, float>>& mods) {
    s.modifier_count = (uint8_t)mods.size();
    for (size_t j = 0; j < mods.size() && j < MAX_MODIFIERS_PER_SEGMENT; j++) {
        s.modifiers[j].type = mods[j].first;
        s.modifiers[j].density = mods[j].second;
    }
}

inline void apply_state_params(Segment& s) {
    for (uint8_t j = 0; j < s.modifier_count; j++) {
        if (s.modifiers[j].type == MOD_KV21) {
            // Kv2.1 (I_Ks) медленный DR: m закрыт в покое, h де-инактивирован (U-type).
            s.modifiers[j].state[0] = 0.0044f;  // kv21 m_inf(−51) ≈ 0.0044
            s.modifiers[j].state[1] = 0.993f;   // kv21 h_inf(−51) ≈ 0.993
        }
        if (s.modifiers[j].type == MOD_KV11) {
            // Kv1.1 (I_Kf) быстрый DR: m закрыт в покое, h(C-type) де-инактивирован.
            s.modifiers[j].state[0] = 0.029f;  // m_inf(−51) ≈ 0.029 (V½=−30, k=6)
            s.modifiers[j].state[1] = 1.0f;    // h(C-type) ≈ 1 (медленная, τ~3 с)
        }
        if (s.modifiers[j].type == MOD_NAV12DEV) {
            // Nav1.2 developmental (Scn2a): m³h, V½_акт=−16, V½_инакт=−45 (баланс single-spike + кик).
            // Покой AIS ~-62 мВ: m_inf(-62)≈0.0007 (закрыт), h_inf(-62)≈0.939 (доступен).
            // Двухфазный recovery: h_fast (state[1]) + h_slow (state[3]) — оба на покое де-инактивированы.
            s.modifiers[j].state[0] = 0.0007f;  // m_inf(rest)
            s.modifiers[j].state[1] = 0.939f;   // h_fast(rest)
            s.modifiers[j].state[3] = 0.939f;   // h_slow(rest)
        }
        if (s.modifiers[j].type == MOD_CONNEXIN_CX36) {
            // gap junction: connexon_count (state[0]) = 1.0 (полная экспрессия).
            // P_open (state[4]) вычислится из фосфорилирования/pH (default ~1.0).
            s.modifiers[j].state[0] = 1.0f;   // connexon_count
            s.modifiers[j].state[5] = 7.2f;   // pH (default, не кислый → открыт)
        }
        if (s.modifiers[j].type == MOD_CONNEXIN_CX43) {
            // глиальный gap junction: connexon_count = 1.0, pH 7.2 (открыт).
            s.modifiers[j].state[0] = 1.0f;
            s.modifiers[j].state[5] = 7.2f;
        }
        if (s.modifiers[j].type == MOD_NAP_CHANNEL) {
            s.modifiers[j].state[0] = 0.1f;  // NaP activation at -55mV
        }
        if (s.modifiers[j].type == MOD_NAP_TADPOLE) {
            s.modifiers[j].state[0] = 0.013f;  // nap_tad m_inf(-51), V½=−25
            s.modifiers[j].state[6] = 0.8f;    // slow inactivation s_inf(rest) ≈ 0.8 (V½_s=−50)
        }
        if (s.modifiers[j].type == MOD_HCN2) {
            s.modifiers[j].state[0] = 0.3f;  // начальный m (Ih)
        }
        if (s.modifiers[j].type == MOD_HCN1) {
            s.modifiers[j].state[0] = 0.3f;  // начальный m (Ih)
        }
        if (s.modifiers[j].type == MOD_KV42) {
            // Kv4.2 A-type (I_A): m закрыт в покое, h де-инактивирован. При деполяризации m³h
            // открывается транзиентно (h быстро инактивируется) → задержка спайка/де-инактивация usAHP.
            s.modifiers[j].state[0] = 0.05f;  // m_inf(rest) ≈ 0.05 — закрыт
            s.modifiers[j].state[1] = 0.85f;  // h_inf(rest) ≈ 0.85 — де-инактивирован
        }
        if (s.modifiers[j].type == MOD_CAV21) {
            // Cav2.1 (пресинаптический P/Q Ca²⁺, триггер выброса). m закрыт в покое,
            // h де-инактивирован (иначе h=0 → cav21_open=m²·h=0 → нет Ca²⁺ в бутон → нет выброса).
            s.modifiers[j].state[0] = 0.0f;   // m_inf(rest) ≈ 0.002 — закрыт
            s.modifiers[j].state[1] = 0.5f;   // h_inf(rest −51.5, Ca=0.1µM) ≈ 0.45–0.55 — доступен
        }
        if (s.modifiers[j].type == MOD_PUMP_NAK) {
            // Насос — одномолекулярный: I = N·k_cat·v_norm·e·Δz (k_cat=75 с⁻¹, Peng/Hertz 2010).
            // state[2] больше не читается (легаси-переопределение I_max убрано при переводе).
            s.modifiers[j].state[3] = 6.0f;  // Na_Km (mM): снижен с 10→6 — pump чувствительнее к Na⁺
        }
        if (s.modifiers[j].type == MOD_KNA_CHANNEL) {
            s.modifiers[j].state[1] = 0.041f;  // Na⁺ EC50 = 41 мМ (Slack, Yuan 2003)
        }
        if (s.modifiers[j].type == MOD_CAMP_PKA) {
            // Базальный [cAMP] = 0.1 µM: Ih (HCN1) V½ остаётся −69 (закрыт на покое).
            // Тонический cAMP (2 µM → V½ −57) держал Ih открытым в покое → L/R-асимметрия.
            // Steady-depol теперь даёт сеть (aIN + восходящая d→d), НЕ тонический Ih.
            s.modifiers[j].state[0] = 0.1f;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// БИЛДЕР СЕТИ (dIN + cIN + aIN + MN + glia + gap junctions + GABA-B)
// ═══════════════════════════════════════════════════════════════

struct CpgNetwork {
    std::vector<Segment> segments;
    std::vector<Neuron> neurons;
    std::vector<IntraConnection> connections;
    std::vector<Synapse> synapses;
    int N_neurons = 0, N_glia = 0;
};

inline void add_neuron(CpgNetwork& net, float x, float y, float z, float r, float Vm,
                        const std::vector<std::pair<uint8_t, float>>& soma_mods,
                        const std::vector<std::pair<uint8_t, float>>& ais_mods,
                        const std::vector<std::pair<uint8_t, float>>& axon_mods) {
    uint32_t nid = (uint32_t)net.neurons.size();
    uint32_t soma_id = (uint32_t)net.segments.size();

    // Сома (тип 1): Na/K-спайк + leak + насос + KCC2 + Cl (+ Ih/gap у dIN)
    Segment soma = {};
    soma.id = soma_id;
    soma.neuron_id = nid;
    soma.position = {x, y, z}; soma.direction = {0, 1, 0};
    soma.radius = r; soma.length = r * 2; soma.V_m = Vm; soma.segment_type = 1;
    init_ions(soma);
    apply_mods(soma, soma_mods);
    apply_state_params(soma);
    net.segments.push_back(soma);

    // AIS (тип 2, тонкий): Nav1.2 (высокая плотность — инициация ПД) + Kv1.1 (низкая)
    Segment ais = {};
    ais.id = soma_id + 1;
    ais.neuron_id = nid;
    ais.position = {x, y, z}; ais.direction = {0, 1, 0};
    ais.radius = AIS_R; ais.length = AIS_L; ais.V_m = Vm; ais.segment_type = 2;
    init_ions(ais);
    apply_mods(ais, ais_mods);
    apply_state_params(ais);
    net.segments.push_back(ais);

    // Аксон (тип 2): Nav1.2+Kv1.1 (проведение ПД) + Cav2.1 (пресинапс — триггер выброса)
    Segment axon = {};
    axon.id = soma_id + 2;
    axon.neuron_id = nid;
    axon.position = {x, y, z}; axon.direction = {0, 1, 0};
    axon.radius = AXON_R; axon.length = AXON_L; axon.V_m = Vm; axon.segment_type = 2;
    init_ions(axon);
    apply_mods(axon, axon_mods);
    apply_state_params(axon);
    net.segments.push_back(axon);

    // Осевые связи: сома→AIS→аксон (бутон — микродомен пресинапса, не отдельный сегмент).
    { IntraConnection c = {}; c.segment_a = soma_id; c.segment_b = soma_id + 1;
      c.is_intracellular = 1; net.connections.push_back(c); }
    { IntraConnection c = {}; c.segment_a = soma_id + 1; c.segment_b = soma_id + 2;
      c.is_intracellular = 1; net.connections.push_back(c); }

    // Нейрон (3 сегмента: сома, AIS, аксон)
    Neuron n = {};
    n.id = nid; n.segment_start = soma_id; n.segment_count = SEGS_PER_NEURON;
    n.soma_index = soma_id; n.threshold = -20;
    net.neurons.push_back(n);
}

inline void add_radial_glia(CpgNetwork& net, float x, float y, float z) {
    // Радиальная глия (ERG): сома (type 1) у центрального канала + радиальный отросток (type 5) к пие.
    uint32_t soma_id = (uint32_t)net.segments.size();

    // Сома: TREK-1/K2P (passive, V≈E_K) + Na/K-насос α2 + Kir.
    Segment soma = {};
    soma.id = soma_id;
    soma.neuron_id = GLIA_RADIAL_ID;
    soma.position = {x, y, z}; soma.direction = {0, 1, 0};
    soma.radius = R_GLIA; soma.length = R_GLIA * 2; soma.V_m = V_GLIA; soma.segment_type = SEG_TYPE_SOMA;
    init_ions(soma);
    soma.intra_ions[3] = 25.0f;  // незрелая глия: высокий [Cl⁻]i (NKCC1) → E_Cl ~−44 мВ (деполяризация)
    apply_mods(soma, radial_glia_soma_mods());
    apply_state_params(soma);
    net.segments.push_back(soma);

    // Радиальный отросток: Kir4.1 (K-буферинг), к пие.
    Segment proc = {};
    proc.id = soma_id + 1;
    proc.neuron_id = GLIA_RADIAL_ID;
    proc.position = {x + R_GLIA, y, z}; proc.direction = {1, 0, 0};  // радиально наружу
    proc.radius = GLIA_PROC_R; proc.length = GLIA_PROC_L; proc.V_m = V_GLIA; proc.segment_type = SEG_TYPE_GLIAL_PROCESS;
    init_ions(proc);
    proc.intra_ions[3] = 25.0f;  // незрелая глия: высокий [Cl⁻]i (как сома)
    apply_mods(proc, radial_glia_process_mods());
    apply_state_params(proc);
    net.segments.push_back(proc);

    // Связь сома↔отросток (синцитий, внутриклеточная).
    { IntraConnection c = {}; c.segment_a = soma_id; c.segment_b = soma_id + 1;
      c.is_intracellular = 1; net.connections.push_back(c); }
}

// СМЖ (ликвор) — внеклеточный резервуар: большой объём, ликворный ионный состав.
inline void add_csf(CpgNetwork& net, float x, float y, float z) {
    Segment s = {};
    s.id = (uint32_t)net.segments.size();
    s.neuron_id = CSF_ID;
    s.position = {x, y, z}; s.direction = {0, 1, 0};
    s.radius = 50.0f; s.length = 50.0f;  // большой объём — резервуар (сток K)
    s.V_m = -70.0f; s.segment_type = SEG_TYPE_SOMA;
    // Ликворный состав (внеклеточный: Na 140, K 3, Cl 130, Ca 1.2, Mg 1).
    s.intra_ions[0] = 140.0f; s.intra_ions[1] = 3.0f; s.intra_ions[2] = 1.2f;
    s.intra_ions[3] = 130.0f; s.intra_ions[4] = 1.0f; s.intra_ions[5] = 0.0f;
    s.intra_ions[6] = 4.0e-5f;  // H⁺ (pH 7.4)
    s.extra_ions[0] = 140.0f; s.extra_ions[1] = 3.0f; s.extra_ions[2] = 1.2f;
    s.extra_ions[3] = 130.0f; s.extra_ions[4] = 1.0f; s.extra_ions[5] = 0.0f;
    s.extra_ions[6] = 4.0e-5f;  // H⁺ (pH 7.4)
    s.ATP_level = 1.0f;
    net.segments.push_back(s);
}

inline CpgNetwork build_cpg(int n_seg = 1, int kick_side = 0) {
    CpgNetwork net;

    for (int seg = 0; seg < n_seg; seg++) {
        float z = -seg * Z_SPAC;
        // Все нейроны — на resting V_m. Физиологический триггер ритма — сенсорный вход RB→dIN
        // (касание → RB спайкует → глутамат → AMPA → dIN), а не V_m-кикстарт и не bath-NMDA.
        // kick_side сохранён в сигнатуре для совместимости, но НЕ задаёт V_m.

        for (int n = 0; n < N_DIN_PER_SIDE; n++) {
            add_neuron(net, -35, Y_DIN, z, R_DIN, V_DIN, dIN_soma_mods(), dIN_ais_mods(), dIN_axon_mods());
        }
        for (int n = 0; n < N_CIN_PER_SIDE; n++) {
            add_neuron(net, -35, Y_CIN, z, R_CIN, V_CIN, cIN_soma_mods(), cIN_ais_mods(), cIN_axon_mods());
        }
        add_neuron(net, -35, Y_AIN, z, R_AIN, V_AIN,  aIN_soma_mods(), aIN_ais_mods(), aIN_axon_mods());
        for (int n = 0; n < N_MN_PER_SIDE; n++) {
            add_neuron(net, -35, Y_MN,  z, R_MN,  V_MN,  MN_soma_mods(), MN_ais_mods(), MN_axon_mods());
        }

        for (int n = 0; n < N_DIN_PER_SIDE; n++) {
            add_neuron(net,  35, Y_DIN, z, R_DIN, V_DIN, dIN_soma_mods(), dIN_ais_mods(), dIN_axon_mods());
        }
        for (int n = 0; n < N_CIN_PER_SIDE; n++) {
            add_neuron(net,  35, Y_CIN, z, R_CIN, V_CIN, cIN_soma_mods(), cIN_ais_mods(), cIN_axon_mods());
        }
        add_neuron(net,  35, Y_AIN, z, R_AIN, V_AIN,  aIN_soma_mods(), aIN_ais_mods(), aIN_axon_mods());
        for (int n = 0; n < N_MN_PER_SIDE; n++) {
            add_neuron(net,  35, Y_MN,  z, R_MN,  V_MN,  MN_soma_mods(), MN_ais_mods(), MN_axon_mods());
        }

        // RB (Rohon-Beard) дорсальный тактильный сенсор (RB_PER_SIDE на сторону).
        for (int n = 0; n < RB_PER_SIDE; n++) {
            add_neuron(net, -35, Y_RB, z, R_RB, V_RB, rb_soma_mods(), rb_ais_mods(), rb_axon_mods());
        }
        for (int n = 0; n < RB_PER_SIDE; n++) {
            add_neuron(net,  35, Y_RB, z, R_RB, V_RB, rb_soma_mods(), rb_ais_mods(), rb_axon_mods());
        }

        for (int g = 0; g < GLIA_PER_SIDE; g++) {
            add_radial_glia(net, -5 - g*7, Y_GLIA, z);  // левая глия (симметрично правой: −5..−40)
            add_radial_glia(net,  5 + g*7, Y_GLIA, z);  // правая глия (+5..+40)
        }
    }

    int total = (int)net.segments.size();
    int n_neuron_segs = n_seg * NEURONS_PER_SIDE * 2 * SEGS_PER_NEURON;
    net.N_neurons = n_seg * NEURONS_PER_SIDE * 2;
    net.N_glia = n_seg * GLIA_PER_SIDE * 2;

    // Глия ↔ ЛОКАЛЬНЫЕ нейроны (тот же сегмент ± соседние), НЕ all-to-all.
    // All-to-all глия↔все давал n_glia × total ≈ 114k связей (n_seg=6) → ~14× замедление.
    // Био: астроцит/радиальная глия контактирует с нейронами локально (~50 мкм), K-буферинг
    // пространственно-локальный, не со всей сетью.
    int neur_per_seg  = NEURONS_PER_SIDE * 2 * SEGS_PER_NEURON;
    int glia_per_seg  = GLIA_PER_SIDE * 2 * 2;  // сома + отросток × 2 стороны
    for (int g = n_neuron_segs; g < total; g++) {
        int gseg = (g - n_neuron_segs) / glia_per_seg;
        for (int ds = -1; ds <= 1; ds++) {
            int ns = gseg + ds;
            if (ns < 0 || ns >= n_seg) continue;
            int n0 = ns * STRIDE;
            int n1 = n0 + neur_per_seg;
            for (int n = n0; n < n1; n++) {
                IntraConnection c = {};
                c.segment_a = (uint32_t)g; c.segment_b = (uint32_t)n;
                c.is_intracellular = 0;
                net.connections.push_back(c);
            }
        }
    }

    // Глиальный синцитий мигрировал на gap junctions (SYNAPSE_TYPE_ELECTRICAL + Cx43, локальный) —
    // ниже, в синапс-секции. Осевой Нернст-Планк all-to-all (is_intracellular=1) убран: давал
    // g_ax≈5705 нС (505× C_dt) → Якоби не сходился → 2-тиковый флип V_m на n_seg=6.

    // Gap junctions: теперь через SYNAPSE_TYPE_ELECTRICAL + Cx36 (молекулярно, не осевой ток).
    // IntraConnection оставлен ТОЛЬКО для диффузии (is_intracellular=0 → EXTRA_ONLY).
    // Электрическая связь dIN↔dIN — ниже, в синапс-секции (single-source коннексин).
    // Внеклеточная диффузия dIN/cIN: ЛОКАЛЬНЫЕ соседи (seg±1, тот же n), НЕ all-to-all.
    // All-to-all (n_seg² × N² связей) давал ~14× замедление на n_seg=6 и не биологичен:
    // внеклеточная диффузия ближнедействующая, bath-релаксация (резервуар CSF) доминирует.
    for (int seg = 0; seg < n_seg; seg++) {
        for (int dseg = -1; dseg <= 1; dseg += 2) {  // seg-1, seg+1
            int seg2 = seg + dseg;
            if (seg2 < 0 || seg2 >= n_seg) continue;
            for (int n = 0; n < N_DIN_PER_SIDE; n++) {
                { IntraConnection c = {}; c.segment_a = (uint32_t)dinL_idx(seg,n);
                  c.segment_b = (uint32_t)dinL_idx(seg2,n);
                  c.is_intracellular = 0; net.connections.push_back(c); }
                { IntraConnection c = {}; c.segment_a = (uint32_t)dinR_idx(seg,n);
                  c.segment_b = (uint32_t)dinR_idx(seg2,n);
                  c.is_intracellular = 0; net.connections.push_back(c); }
            }
            for (int n = 0; n < N_CIN_PER_SIDE; n++) {
                { IntraConnection c = {}; c.segment_a = (uint32_t)cinL_idx(seg,n);
                  c.segment_b = (uint32_t)cinL_idx(seg2,n);
                  c.is_intracellular = 0; net.connections.push_back(c); }
                { IntraConnection c = {}; c.segment_a = (uint32_t)cinR_idx(seg,n);
                  c.segment_b = (uint32_t)cinR_idx(seg2,n);
                  c.is_intracellular = 0; net.connections.push_back(c); }
            }
        }
    }

    uint32_t sid = 0;
    // Gap junctions (SYNAPSE_TYPE_ELECTRICAL + Cx36): dIN↔dIN ипсилатерально, кросс-сегментно.
    // Проводимость — из модификатора Cx36 (density × connexon_count × P_open × γ), НЕ weight.
    for (int seg = 0; seg < n_seg; seg++) {
        // Внутри сегмента: соседние dIN (n ↔ n+1) — ЛОКАЛЬНО, не all-to-all (Ferrario 2018)
        // Био (Li 2001 униполярный dIN): Cx36 gap junction — сома↔сома в том же сегменте.
        // Кросс-сегментный dIN↔dIN (аксон↔аксон вдоль оси) УБРАН: на n_seg=6 синхронизировал
        // dIN в ростро-каудальный «кабель» и гасил alternation (5→9 при удалении, [gap-decompose]).
        for (int n = 0; n < N_DIN_PER_SIDE - 1; n++) {
            { Synapse s = {}; s.id = sid++; s.pre_segment = dinL_idx(seg,n);
              s.post_segment = dinL_idx(seg,n+1);
              s.type = SYNAPSE_TYPE_ELECTRICAL; net.synapses.push_back(s); }
            { Synapse s = {}; s.id = sid++; s.pre_segment = dinR_idx(seg,n);
              s.post_segment = dinR_idx(seg,n+1);
              s.type = SYNAPSE_TYPE_ELECTRICAL; net.synapses.push_back(s); }
        }
        // MN↔MN электротоническое сопряжение (Zhao/Wolf/Roberts 1998) — кросс-сегментно (тот же n)
        if (seg < n_seg - 1) {
            for (int n = 0; n < N_MN_PER_SIDE; n++) {
                { Synapse s = {}; s.id = sid++; s.pre_segment = mnL_idx(seg,n);
                  s.post_segment = mnL_idx(seg+1,n);
                  s.type = SYNAPSE_TYPE_ELECTRICAL; net.synapses.push_back(s); }
                { Synapse s = {}; s.id = sid++; s.pre_segment = mnR_idx(seg,n);
                  s.post_segment = mnR_idx(seg+1,n);
                  s.type = SYNAPSE_TYPE_ELECTRICAL; net.synapses.push_back(s); }
            }
        }
    }

    // Глиальный синцитий: gap junctions (Cx43) — ЛОКАЛЬНЫЙ, не all-to-all.
    // Сома↔сома и отросток↔отросток в том же сегменте + соседние (±1).
    // Био: астроциты/радиальная глия связаны щелевыми контактами с СОСЕДЯМИ (не со всеми).
    // Численно: разреженный граф + малая проводимость (3 нС vs 5705 нС) → Якоби сходится.
    {
        const int GLIA_SEGS_PER_SEG = GLIA_PER_SIDE * 2 * 2;  // 24 (6 глий × 2 стороны × 2 сегмента)
        for (int s = 0; s < n_seg; s++) {
            for (int t = s - 1; t <= s + 1; t++) {
                if (t < 0 || t >= n_seg) continue;
                for (int ka = 0; ka < GLIA_SEGS_PER_SEG; ka++) {
                    for (int kb = 0; kb < GLIA_SEGS_PER_SEG; kb++) {
                        if (s == t && ka >= kb) continue;       // избежать дублей внутри сегмента
                        bool a_soma = (ka % 2 == 0);            // чётный offset = сома
                        bool b_soma = (kb % 2 == 0);
                        if (a_soma != b_soma) continue;         // только однотипные (сома↔сома, отросток↔отросток)
                        uint32_t ia = (uint32_t)(n_neuron_segs + s * GLIA_SEGS_PER_SEG + ka);
                        uint32_t ib = (uint32_t)(n_neuron_segs + t * GLIA_SEGS_PER_SEG + kb);
                        Synapse sy = {}; sy.id = sid++; sy.pre_segment = ia; sy.post_segment = ib;
                        sy.type = SYNAPSE_TYPE_ELECTRICAL; net.synapses.push_back(sy);
                    }
                }
            }
        }
    }
    for (int seg = 0; seg < n_seg; seg++) {
        // cIN→dIN cross-inhibition: каждый cIN→каждый dIN противоположной стороны
        for (int i = 0; i < GLY_N; i++) {
            for (int c = 0; c < N_CIN_PER_SIDE; c++) {
                for (int d = 0; d < N_DIN_PER_SIDE; d++) {
                    { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(cinL_idx(seg,c));
                      s.post_segment = dinR_idx(seg,d);
                      s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = GLY_W; s.release_probability = GLY_RP;
                      s.release_amount = 1.2f; s.tau_clearance = GLY_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                      s.RRP = RRP_GLY_MAX; s.RRP_max = RRP_GLY_MAX; s.RRP_recovery = RRP_GLY_REC;
                      s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_GLYCINE;
                      s.post_receptors[0].density = GLY_REC / N_CIN_PER_SIDE; net.synapses.push_back(s); }
                    { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(cinR_idx(seg,c));
                      s.post_segment = dinL_idx(seg,d);
                      s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = GLY_W; s.release_probability = GLY_RP;
                      s.release_amount = 1.2f; s.tau_clearance = GLY_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                      s.RRP = RRP_GLY_MAX; s.RRP_max = RRP_GLY_MAX; s.RRP_recovery = RRP_GLY_REC;
                      s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_GLYCINE;
                      s.post_receptors[0].density = GLY_REC / N_CIN_PER_SIDE; net.synapses.push_back(s); }
                }
            }
        }
        // c↔c reciprocal: каждый cL→каждый cR
        for (int i = 0; i < C2C_N; i++) {
            for (int ca = 0; ca < N_CIN_PER_SIDE; ca++) {
                for (int cb = 0; cb < N_CIN_PER_SIDE; cb++) {
                    { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(cinL_idx(seg,ca));
                      s.post_segment = cinR_idx(seg,cb);
                      s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = C2C_W; s.release_probability = GLY_RP;
                      s.release_amount = 1.0f; s.tau_clearance = GLY_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                      s.RRP = RRP_GLY_MAX; s.RRP_max = RRP_GLY_MAX; s.RRP_recovery = RRP_GLY_REC;
                      s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_GLYCINE;
                      s.post_receptors[0].density = C2C_GLY_REC / N_CIN_PER_SIDE; net.synapses.push_back(s); }
                    { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(cinR_idx(seg,ca));
                      s.post_segment = cinL_idx(seg,cb);
                      s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = C2C_W; s.release_probability = GLY_RP;
                      s.release_amount = 1.0f; s.tau_clearance = GLY_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                      s.RRP = RRP_GLY_MAX; s.RRP_max = RRP_GLY_MAX; s.RRP_recovery = RRP_GLY_REC;
                      s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_GLYCINE;
                      s.post_receptors[0].density = C2C_GLY_REC / N_CIN_PER_SIDE; net.synapses.push_back(s); }
                }
            }
        }
        // d→c excitation: каждый dIN→каждый cIN своей стороны
        for (int i = 0; i < EXC_N; i++) {
            for (int d = 0; d < N_DIN_PER_SIDE; d++) {
                for (int c = 0; c < N_CIN_PER_SIDE; c++) {
                    { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(dinL_idx(seg,d));
                      s.post_segment = cinL_idx(seg,c);
                      s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2C_W; s.release_probability = EXC_RP;
                      s.release_amount = 1.0f; s.tau_clearance = EXC_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                      s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                      s.post_receptor_count = 2; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                      s.post_receptors[0].density = AMPA_R / N_DIN_PER_SIDE; s.post_receptors[1].type = MOD_NT_RECEPTOR_NMDA_2B;
                      s.post_receptors[1].density = NMDA_R / N_DIN_PER_SIDE; net.synapses.push_back(s); }
                    { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(dinR_idx(seg,d));
                      s.post_segment = cinR_idx(seg,c);
                      s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2C_W; s.release_probability = EXC_RP;
                      s.release_amount = 1.0f; s.tau_clearance = EXC_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                      s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                      s.post_receptor_count = 2; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                      s.post_receptors[0].density = AMPA_R / N_DIN_PER_SIDE; s.post_receptors[1].type = MOD_NT_RECEPTOR_NMDA_2B;
                      s.post_receptors[1].density = NMDA_R / N_DIN_PER_SIDE; net.synapses.push_back(s); }
                }
            }
        }

        // d→a, a→c, a→d (сохранены, выключены через D2A_N=0, A2C_N=0, A2D_N=0)
        for (int i = 0; i < D2A_N; i++) {
            for (int d = 0; d < N_DIN_PER_SIDE; d++) {
                { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(dinL_idx(seg,d)); s.post_segment = ainL_idx(seg);
                  s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2A_W; s.release_probability = EXC_RP;
                  s.release_amount = 1.0f; s.tau_clearance = EXC_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                  s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                  s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                  s.post_receptors[0].density = AIN_AMPA_DENS; net.synapses.push_back(s); }
                { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(dinR_idx(seg,d)); s.post_segment = ainR_idx(seg);
                  s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2A_W; s.release_probability = EXC_RP;
                  s.release_amount = 1.0f; s.tau_clearance = EXC_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                  s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                  s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                  s.post_receptors[0].density = AIN_AMPA_DENS; net.synapses.push_back(s); }
            }
        }
        for (int i = 0; i < A2C_N; i++) {
            { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(ainL_idx(seg)); s.post_segment = cinR_idx(seg,0);
              s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = A2C_W; s.release_probability = 0.5f;
              s.release_amount = 1.0f; s.tau_clearance = GABA_B_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
              s.RRP = RRP_GABA_MAX; s.RRP_max = RRP_GABA_MAX; s.RRP_recovery = RRP_GABA_REC;
              s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_GABAB;
              s.post_receptors[0].density = 0.5f; net.synapses.push_back(s); }
            { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(ainR_idx(seg)); s.post_segment = cinL_idx(seg,0);
              s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = A2C_W; s.release_probability = 0.5f;
              s.release_amount = 1.0f; s.tau_clearance = GABA_B_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
              s.RRP = RRP_GABA_MAX; s.RRP_max = RRP_GABA_MAX; s.RRP_recovery = RRP_GABA_REC;
              s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_GABAB;
              s.post_receptors[0].density = 0.5f; net.synapses.push_back(s); }
        }
        for (int i = 0; i < A2D_N; i++) {
            { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(ainL_idx(seg)); s.post_segment = dinL_idx(seg,0);
              s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = A2D_W; s.release_probability = 0.5f;
              s.release_amount = 1.0f; s.tau_clearance = GABA_B_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
              s.RRP = RRP_GABA_MAX; s.RRP_max = RRP_GABA_MAX; s.RRP_recovery = RRP_GABA_REC;
              s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_GABAB;
              s.post_receptors[0].density = 0.4f; net.synapses.push_back(s); }
            { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(ainR_idx(seg)); s.post_segment = dinR_idx(seg,0);
              s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = A2D_W; s.release_probability = 0.5f;
              s.release_amount = 1.0f; s.tau_clearance = GABA_B_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
              s.RRP = RRP_GABA_MAX; s.RRP_max = RRP_GABA_MAX; s.RRP_recovery = RRP_GABA_REC;
              s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_GABAB;
              s.post_receptors[0].density = 0.4f; net.synapses.push_back(s); }
        }
        // d→m: каждый dIN→каждый MN своей стороны (дивергентные проекции)
        for (int d = 0; d < N_DIN_PER_SIDE; d++) {
            for (int m = 0; m < N_MN_PER_SIDE; m++) {
                { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(dinL_idx(seg,d)); s.post_segment = mnL_idx(seg,m);
                  s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2M_W; s.release_probability = MN_RP;
                  s.release_amount = 1.2f; s.tau_clearance = MN_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                  s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                  s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                  s.post_receptors[0].density = MN_AMPA / N_DIN_PER_SIDE; net.synapses.push_back(s); }
                { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(dinR_idx(seg,d)); s.post_segment = mnR_idx(seg,m);
                  s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2M_W; s.release_probability = MN_RP;
                  s.release_amount = 1.2f; s.tau_clearance = MN_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                  s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                  s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                  s.post_receptors[0].density = MN_AMPA / N_DIN_PER_SIDE; net.synapses.push_back(s); }
            }
        }
        // RB→dIN: сенсорный вход (ипсилатерально, AMPA) — касание → RB → глутамат → dIN.
        for (int rb = 0; rb < RB_PER_SIDE; rb++) {
            for (int d = 0; d < N_DIN_PER_SIDE; d++) {
                { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(rbL_idx(seg,rb));
                  s.post_segment = dinL_idx(seg,d);
                  s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = RB_DIN_W; s.release_probability = EXC_RP;
                  s.release_amount = 1.0f; s.tau_clearance = EXC_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                  s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                  s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                  s.post_receptors[0].density = RB_DIN_AMPA; net.synapses.push_back(s); }
                { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(rbR_idx(seg,rb));
                  s.post_segment = dinR_idx(seg,d);
                  s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = RB_DIN_W; s.release_probability = EXC_RP;
                  s.release_amount = 1.0f; s.tau_clearance = EXC_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                  s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                  s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                  s.post_receptors[0].density = RB_DIN_AMPA; net.synapses.push_back(s); }
            }
        }
    }

    // dIN→dIN химическое возбуждение (AMPA+NMDA), ипсилатерально.
    // Ferrario 2018 (eLife 7:e33281): primary аксон НИСХОДЯЩИЙ (все dIN), secondary аксон
    // ВОСХОДЯЩИЙ ТОЛЬКО у ростральных HdIN (Roberts 2010: "some dINs have both descending and
    // ascending axons"; восходящие связи "play a key role in swimming", без них ростральные dIN гаснут).
    // AMPA-доминирующий (ampa 0.593 / nmda 0.15 ≈ 4:1). НЕ all-to-all.
    // Ребаунд dIN — Na-де-инактивация + фоновая деполяризация, НЕ T-Ca (Dale 1995: T-type в dIN нет).
    // density per-synapse = популяционная / N_DIN_PER_SIDE (конвергенция входа).
    for (int i = 0; i < D2D_N; i++) {
        for (int seg_a = 0; seg_a < n_seg; seg_a++) {
            for (int seg_b = seg_a + 1; seg_b < n_seg; seg_b++) {  // НИСХОДЯЩИЙ (ростральный→каудальный)
                for (int da = 0; da < N_DIN_PER_SIDE; da++) {
                    for (int db = 0; db < N_DIN_PER_SIDE; db++) {
                        { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(dinL_idx(seg_a,da));
                          s.post_segment = dinL_idx(seg_b,db);
                          s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2D_W; s.release_probability = EXC_RP;
                          s.release_amount = 1.0f; s.tau_clearance = EXC_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                          s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                          s.post_receptor_count = 2; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                          s.post_receptors[0].density = D2D_AMPA_R / (seg_b * N_DIN_PER_SIDE); s.post_receptors[1].type = MOD_NT_RECEPTOR_NMDA_2B;
                          s.post_receptors[1].density = D2D_NMDA_R / (seg_b * N_DIN_PER_SIDE); net.synapses.push_back(s); }
                        { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(dinR_idx(seg_a,da));
                          s.post_segment = dinR_idx(seg_b,db);
                          s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2D_W; s.release_probability = EXC_RP;
                          s.release_amount = 1.0f; s.tau_clearance = EXC_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                          s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                          s.post_receptor_count = 2; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                          s.post_receptors[0].density = D2D_AMPA_R / (seg_b * N_DIN_PER_SIDE); s.post_receptors[1].type = MOD_NT_RECEPTOR_NMDA_2B;
                          s.post_receptors[1].density = D2D_NMDA_R / (seg_b * N_DIN_PER_SIDE); net.synapses.push_back(s); }
                    }
                }
            }
        }
    }

    // ВОСХОДЯЩИЙ d→d (secondary аксон, только ростральный HdIN = seg0): восходящая ветвь HdIN
    // связывает ростральный пул сам с собой → seg0↔seg0 (da≠db). Без него ростральные dIN
    // не получают d→d-возбуждения (ничего ростральнее нет) → плавание гаснет (Ferrario 2018).
    // Балансируется рекуррентным торможением aIN (a→d) — без него runaway (проверено 2026-10-21).
    if (n_seg >= 2) {
        for (int da = 0; da < N_DIN_PER_SIDE; da++) {
            for (int db = 0; db < N_DIN_PER_SIDE; db++) {
                if (da == db) continue;
                { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(dinL_idx(0,da));
                  s.post_segment = dinL_idx(0,db);
                  s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2D_W; s.release_probability = EXC_RP;
                  s.release_amount = 1.0f; s.tau_clearance = EXC_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                  s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                  s.post_receptor_count = 2; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                  s.post_receptors[0].density = D2D_AMPA_R / (N_DIN_PER_SIDE - 1); s.post_receptors[1].type = MOD_NT_RECEPTOR_NMDA_2B;
                  s.post_receptors[1].density = D2D_NMDA_R / (N_DIN_PER_SIDE - 1); net.synapses.push_back(s); }
                { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(dinR_idx(0,da));
                  s.post_segment = dinR_idx(0,db);
                  s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2D_W; s.release_probability = EXC_RP;
                  s.release_amount = 1.0f; s.tau_clearance = EXC_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                  s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                  s.post_receptor_count = 2; s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                  s.post_receptors[0].density = D2D_AMPA_R / (N_DIN_PER_SIDE - 1); s.post_receptors[1].type = MOD_NT_RECEPTOR_NMDA_2B;
                  s.post_receptors[1].density = D2D_NMDA_R / (N_DIN_PER_SIDE - 1); net.synapses.push_back(s); }
            }
        }
    }

    // MN→MN никотиновый ACh (Zhao/Wolf/Roberts 1998): ростральный MN → каудальный MN.
    // Холинергический компонент возбуждения MN (глутамат + ACh + электротонический).
    for (int seg_a = 0; seg_a < n_seg; seg_a++) {
        for (int seg_b = seg_a + 1; seg_b < n_seg; seg_b++) {
            for (int m = 0; m < N_MN_PER_SIDE; m++) {
                { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(mnL_idx(seg_a,m));
                  s.post_segment = mnL_idx(seg_b,m);
                  s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2M_W; s.release_probability = MN_RP;
                  s.release_amount = 1.0f; s.tau_clearance = MN_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                  s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                  s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_NICOTINIC;
                  s.post_receptors[0].density = MN_NICOTINIC_DENS; net.synapses.push_back(s); }
                { Synapse s = {}; s.id = sid++; s.pre_segment = axon_of(mnR_idx(seg_a,m));
                  s.post_segment = mnR_idx(seg_b,m);
                  s.type = SYNAPSE_TYPE_CHEMICAL; s.weight = D2M_W; s.release_probability = MN_RP;
                  s.release_amount = 1.0f; s.tau_clearance = MN_TAU; s.pre_area = PRE_AREA; s.post_area = POST_AREA;
                  s.RRP = RRP_AMPA_MAX; s.RRP_max = RRP_AMPA_MAX; s.RRP_recovery = RRP_AMPA_REC;
                  s.post_receptor_count = 1; s.post_receptors[0].type = MOD_NT_RECEPTOR_NICOTINIC;
                  s.post_receptors[0].density = MN_NICOTINIC_DENS; net.synapses.push_back(s); }
            }
        }
    }

    // Инициализация микродомена Ca²⁺ бутона (иначе ca_in=0 → гигантский Ca-градиент → efflux)
    for (auto& sy : net.synapses) sy.bouton_Ca = 0.0001f;  // 100 нМ (BOUTON_CA_BASELINE_MM)

    // СМЖ (ликвор) — резервуар + связь с глиальными сомами (EXTRA_ONLY, сток K).
    add_csf(net, 0.0f, 0.0f, 0.0f);
    uint32_t csf_id = (uint32_t)(net.segments.size() - 1);
    for (uint32_t i = 0; i < net.segments.size(); i++) {
        if (net.segments[i].neuron_id == GLIA_RADIAL_ID && net.segments[i].segment_type == SEG_TYPE_SOMA) {
            IntraConnection c = {};
            c.segment_a = i; c.segment_b = csf_id;
            c.is_intracellular = 0;  // EXTRA_ONLY: обмен внеклеточного K между глией и ликвором
            net.connections.push_back(c);
        }
    }

    return net;
}

inline int save_bion(const CpgNetwork& net, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    NetworkHeader h = {};
    h.magic = BION_MAGIC; h.version = BION_VERSION; h.mode = 0;
    h.num_neurons = (uint32_t)net.neurons.size();
    h.num_segments = (uint32_t)net.segments.size();
    h.num_intra_connections = (uint32_t)net.connections.size();
    h.num_synapses = (uint32_t)net.synapses.size();
    fwrite(&h, sizeof(h), 1, f);
    for (auto& n : net.neurons) fwrite(&n, sizeof(n), 1, f);
    for (auto& s : net.segments) fwrite(&s, sizeof(s), 1, f);
    for (auto& c : net.connections) fwrite(&c, sizeof(c), 1, f);
    for (auto& s : net.synapses) fwrite(&s, sizeof(s), 1, f);
    fclose(f);
    return 0;
}

} // namespace cpg
