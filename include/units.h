// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// units.h — ЕДИНЫЙ слой именованных переводов физических единиц.
// Цель SI-рефакторинга: каждый множитель в коде — либо точный перевод единиц
// (здесь), либо физическая константа (F/R/T в types.h), либо литературное
// значение (P в см/с, g в С/см²). Никаких «магических» подгоночных чисел.
//
// Рабочие единицы хранения/интерфейса (НЕ меняются): мВ, мс, µm, мМ, пА, пФ.
// Физические единицы внутри формул: В, с, м, моль/м³, А, Ф.

#pragma once

// ── Напряжение: мВ → В ────────────────────────────────────────────────
#define MV_TO_V   1e-3f

// ── Время: мс → с ─────────────────────────────────────────────────────
#define MS_TO_S   1e-3f

// ── Длина: µm → см / м ────────────────────────────────────────────────
#define UM_TO_CM  1e-4f   // 1 µm = 1e-4 cm
#define UM_TO_M   1e-6f   // 1 µm = 1e-6 m
#define CM_TO_M   1e-2f   // 1 cm = 1e-2 m

// ── Площадь: µm² → см² / м² ───────────────────────────────────────────
#define UM2_TO_CM2 1e-8f  // 1 µm² = 1e-8 cm²
#define UM2_TO_M2  1e-12f // 1 µm² = 1e-12 m²

// ── Объём: µm³ → м³ / Л ───────────────────────────────────────────────
#define UM3_TO_M3 1e-18f  // 1 µm³ = 1e-18 m³
#define UM3_TO_L  1e-15f  // 1 µm³ = 1e-15 L

// ── Ток: пА ↔ А ───────────────────────────────────────────────────────
#define NA_TO_A   1e-9f   // 1 nA = 1e-9 A (обратный, не используется в токовых путях)
#define A_TO_PA   1e12f   // 1 A  = 1e12 pA

// ── Ёмкость: пФ → Ф ───────────────────────────────────────────────────
#define PF_TO_F   1e-12f  // 1 pF = 1e-12 F

// ── Концентрация: мМ ≡ моль/м³ (1 мМ = 1 ммоль/Л = 1 моль/м³) ─────────
// Массив intra_ions[]/extra_ions[] хранится в мМ и УЖЕ совместим с SI.
#define MM_IS_MOL_M3 1.0f

// ── Концентрация: мМ → µM (для Ca²⁺-зависимых реакций, Kd в µM) ─────────
#define MM_TO_UM 1e3f   // 1 мМ = 1000 µM

// ── Внеклеточное пространство (ECS): ~20% объёма мозга (Syková & Nicholson 2008,
// Physiol Rev 88:1277-1340). V_extra/V_intra = α/(1−α) = 0.25.
// ЕДИНЫЙ ИСТОЧНИК для переноса ионов через мембрану (каналы/насос: extra Δc = intra Δc / RATIO)
// и внеклеточной диффузии (diffusion_kernel.h: extra Δc = flux/(vol × RATIO)).
// Ранее хардкод /10 предполагал V_extra = 10×V_intra — неверно (внеклетка МЕНЬШЕ внутриклетки).
#define ECS_VOLUME_FRACTION 0.2f
#define EXTRA_VOLUME_RATIO  (ECS_VOLUME_FRACTION / (1.0f - ECS_VOLUME_FRACTION))  // = 0.25

// ── Насос Na/K-АТФаза: цикл Поста–Альберса (Smith & Crampin 2004, Läuger 1991) ──
// Стехиометрия 3 Na⁺ наружу : 2 K⁺ внутрь : 1 ATP (электрогенный, нетто +1 заряд наружу).
// ЕДИНИЦА — молекула: I = N·k_cat·v_norm·e·Δz, где N = density[молекул/µm²]·area[µm²],
// k_cat — оборот молекулы [с⁻¹], e·Δz = +1.602e-19 Кл за цикл (нетто +1 наружу).
#define ELEMENTARY_CHARGE_C 1.602176634e-19f  // элементарный заряд [Кл]
#define NA_K_PUMP_TURNOVER_MAX_S 75.0f  // k_cat α3 (Peng/Hertz 2010: α2/α3 = 70-80 с⁻¹)
#define UA_PER_CM2_TO_PA  (UM2_TO_CM2 * 1e6f)   // µA/см² · µm² → пА (=1e-2)

// Константы скоростей цикла (s⁻¹, 37°C):
#define NA_K_PUMP_K1_PHOSPH   200.0f  // E1·Na3·ATP → E1P(Na3), фосфорилирование
#define NA_K_PUMP_K2_E1PE2P   60.0f   // E1P → E2P при V=0 (Heyse 22 s⁻¹ @20°C, Q10≈2.7)
#define NA_K_PUMP_K2M_E2PE1P  6.0f    // E2P → E1P при V=0 (обратный, ~10× медленнее)
#define NA_K_PUMP_K3_DEPHOSPH 200.0f  // E2P·2K → E2(2K), дефосфорилирование
#define NA_K_PUMP_K4_E2E1     60.0f   // E2 → E1, конформационный переход + высвобождение K⁺
#define NA_K_PUMP_Z_NET       0.8f    // кажущаяся валентность электрогенного шага (z_q 0.74-0.91)
#define NA_K_PUMP_DELTA       0.5f    // партиция заряда (симметричная)
#define NA_K_PUMP_K_NA_MM     10.0f   // K_m Na⁺_i, мМ (Hill-3, три сайта связывания)
#define NA_K_PUMP_K_K_MM      1.5f    // K_m K⁺_o, мМ (Hill-2, два сайта связывания)
#define NA_K_PUMP_K_ATP_MM    0.5f    // K_m ATP, мМ (сродство Na/K-АТФазы к ATP)
#define NA_K_PUMP_ATP_RATE_PA 1e-8f   // ATP_level -= I[пА]·dt[мс]·1e-8 (1 ATP/цикл, норм. пула ~1 нмоль)
// Глиальная α2-изоформа (MOD_GLIAL_NAK_PUMP 110): тот же k_cat (α2=α3 по Peng/Hertz 2010),
// выше сродство к K⁺_o. Различие α2 vs α3 — в плотности экспрессии (клеточный уровень), НЕ в обороте.
#define NA_K_PUMP_GLIAL_K_K_MM          1.0f   // K_m K⁺_o α2 (vs α3 1.5 мМ)
// α3-изоформа (MOD_PUMP_NAK_A3 197): НИЗКОЕ сродство к Na⁺_i (K_m≈25 мМ) — ДИНАМИЧЕСКИЙ насос.
// usAHP: активируется только при сильном росте [Na⁺]_i во время залпа (Zahler 1997, Blanco 2005).
// Сродство к Na⁺: α1(≈10 мМ) > α2(≈15-20) > α3(≈25-50) — α3 тонически почти неактивна на покое.
#define NA_K_PUMP_A3_K_NA_MM        25.0f   // K_m Na⁺_i α3 (низкое сродство vs α1 10 мМ)
#define NA_K_PUMP_OUABAIN_IC50_NM   20.0f   // уабаин IC50 α3 (грызуны Blanco 1993; Xenopus — уточнить)

// ── PMCA (98): Ca²⁺-АТФаза плазмалеммы (Carafoli 1991, Strehler & Zacharias 2001) ──
// Высокоаффинный, низкоёмкостный: 1 Ca²⁺ наружу : 1 ATP, электрогенный (+2 наружу).
// Hill-2 (два Ca²⁺-связывающих сайта), K_m ≈ 300 нМ. Вольт-НЕзависимый (в отличие от Na/K-насоса).
#define PMCA_TURNOVER_MAX_S  100.0f   // k_cat PMCA (BRENDA 9.5-149 с⁻¹, физиол. 30-200)
#define PMCA_K_M_CA_M_M       0.0003f  // K_m Ca²⁺ = 300 нМ (мМ)
#define PMCA_HILL_N           2.0f     // Hill-2
#define PMCA_K_ATP_MM         0.2f     // K_m ATP, мМ (PMCA — выше сродство чем у Na/K-насоса)

// ── NCX (99): Na⁺/Ca²⁺ обменник (Weber et al. 2001, Shannon et al. 2004) ──
// Стехиометрия 3 Na⁺ : 1 Ca²⁺ (Hinata 2002, Bers & Weber 2002). Электрогенный (+1 в направлении Na),
// РЕВЕРСИВНЫЙ: знак меняется при E_NaCa = 3·E_Na − 2·E_Ca (термодинамически согласованный, без клампа).
#define NCX_TURNOVER_MAX_S  500.0f   // k_cat (физиол. turnover, Hilgemann 1991: max 5000, физиол. 300-500 с⁻¹)
#define NCX_ETA              0.35f     // вольт-зависимость (партиция заряда, Shannon 2004)
#define NCX_K_SAT            0.27f     // фактор насыщения (Shannon 2004)
#define NCX_K_M_CA_ACT_MM    0.000125f // K_mCaAct = 125 нМ (мМ), Hill-2 аллостерич. (Bers & Weber 2002)
#define NCX_REF_FLUX_MM4     530.2f    // якорь I_max: F_flux (мМ⁴) при V=0, Na_i10/Na_o140/Ca_i0.1µM/Ca_o2mM

// ── KCC2 (100): K⁺-Cl⁻ котранспортёр (Payne 1997, Rivera 1999) ──
// Электронейтральный 1 K⁺ : 1 Cl⁻ наружу, Cl⁻-экструдер (задаёт гиперполяризующее GABA).
// Транспорт — ПОТОК (моль/с), а не ток (электронейтрален).
#define KCC2_VMAX_NMOL_PER_CM2_S  5.0f    // V_max (литература 1-10 нмоль/см²/с; Staley & Proctor 1999 ~5-6 мМ/с)
#define KCC2_K_M_K_I_MM          6.5f    // K_m K⁺_i (Payne/Rivera: 5.2-11.7 мМ)
#define KCC2_K_M_CL_I_MM         6.5f    // K_m Cl⁻_i (6.8-7.2 мМ)
#define KCC2_K_M_K_O_MM          0.5f    // K_m K⁺_o (транс-ингибирование)
// 1 нмоль/(см²·с) = 1e-9 моль / (1e8 µm² · 1e3 мс) = 1e-20 моль/(µm²·мс)
#define NMOL_CM2_S_TO_MOL_UM2_MS 1e-20f

// ── CA_BUFFER (102): Ca²⁺-буфер (парвальбумин-подобный, Schwaller 2010) ──
// Обратимое связывание Ca²⁺ + B ⇌ CaB. Электронейтрален (нет тока).
// density масштабирует ЁМКОСТЬ B_total (количество белка), не кинетику k_on/k_off.
#define CA_BUFFER_B_TOTAL_MM  0.15f     // B_total 150 µM
#define CA_BUFFER_K_D_MM      0.0005f   // K_d = 500 нМ
#define CA_BUFFER_K_ON_MM_MS  0.1f      // k_on = 0.1 мМ⁻¹мс⁻¹ (= 0.1 µM⁻¹с⁻¹)
#define CA_BUFFER_K_OFF_MS    0.00005f  // k_off = K_d·k_on = 5e-5 мс⁻¹

// ── NKCC1 (122): Na⁺-K⁺-2Cl⁻ котранспортёр (Delpire & Gagnon 2011) ──
// Электронейтральный 1 Na⁺ : 1 K⁺ : 2 Cl⁻ ВНУТРЬ, Cl⁻-загрузчик (антагонист KCC2).
// Упорядоченное связывание: Cl⁻ первый, Na⁺, второй Cl⁻, K⁺. Поток (не ток).
#define NKCC1_VMAX_NMOL_PER_CM2_S  7.0f    // V_max (высокоёмкостный, чуть быстрее KCC2 5)
#define NKCC1_K_M_NA_I_MM         10.0f    // K_m Na⁺_i
#define NKCC1_K_M_K_I_MM          15.0f    // K_m K⁺_i
#define NKCC1_K_M_CL_I_MM         50.0f    // K_m Cl⁻_i (ниже сродство чем KCC2 6.5)
#define NKCC1_K_M_NA_O_MM          0.5f    // K_m Na⁺_o (транс-стимуляция)
#define NKCC1_K_M_K_O_MM           0.2f    // K_m K⁺_o (транс-ингибирование)

// ── SERCA (126): Ca²⁺-АТФаза эндоплазматического ретикулума (Verkhratsky 2005, Lytton 1992) ──
// Закачивает 2 Ca²⁺ из цитозоля в ЭПР за 1 ATP. Внутриклеточная мембрана → нет тока плазмалеммы.
#define SERCA_TURNOVER_MAX_S  100.0f   // k_cat SERCA (оборот ~100 с⁻¹, Verkhratsky 2005)
#define AVOGADRO  6.02214076e23f      // число Авогадро [молекул/моль]
#define SERCA_K_M_CA_M_M           0.00027f // K_m Ca²⁺ = 0.27 µM (Lytton 1992)
#define SERCA_K_ATP_MM             0.1f     // K_m ATP
#define SERCA_ATP_NORM_PER_MOL_CA  1e6f     // ATP_level -= flux_mol·1e6 (1 ATP на 2 Ca, норм. пула)

// ── IP3R (127): IP3-рецептор, канал выброса Ca²⁺ из ЭПР (Li & Rinzel 1994) ──
// Редукция De Young–Keizer: J = v·m∞³·n∞³·h³·([Ca]ER−[Ca]). Динамическая инактивация h.
// IP3 — локальный второй мессенджер (state[]), продукция честным каскадом Gq→PLC→IP3.
#define IP3R_RELEASE_RATE_S       5.0f     // v_rel выброса Ca²⁺ (с⁻¹)
#define IP3R_K_IP3_M_M            0.00013f // K_IP3 = 0.13 µM (Li-Rinzel d1)
#define IP3R_K_ACT_M_M            0.0002f  // K_act (Ca²⁺-активация) = 0.2 µM
#define IP3R_K_INH_M_M            0.001f   // K_inh (Ca²⁺-инактивация) = 1 µM (Li-Rinzel d2=1.049)
#define IP3R_TAU_H_MS             100.0f   // τ_h инактивации
#define IP3R_TAU_IP3_MS           200.0f   // τ_IP3 распада (PLC-деградация)
#define IP3R_K_IP3_PROD_MS        0.01f    // скорость продукции IP3 (PLC, мс⁻¹)
#define IP3R_EC50_MGLUR_MM        0.01f    // EC50 mGluR (глутамат) = 10 µM

// ── RyR (128): рианодиновый рецептор, CICR (Keizer & Levine 1996) ──
// J = v·w·(1+(c/K_b)³)/(1+(K_a/c)⁴+(c/K_b)³)·([Ca]ER−[Ca]). Динамическая адаптация w.
#define RYR_RELEASE_RATE_S        20.0f    // v_rel выброса Ca²⁺ (с⁻¹, быстрее IP3R — CICR)
#define RYR_K_A_M_M               0.000372f // K_a = 0.372 µM (Hill-4 активация)
#define RYR_K_B_M_M               0.000636f // K_b = 0.636 µM (Hill-3 ингибирование)
#define RYR_K_C                   0.057f   // K_c (адаптация)
#define RYR_TAU_W_MS              10000.0f // τ_w базовая (адаптация, ~10 с)

// ── ИОНОТРОПНЫЕ РЕЦЕПТОРЫ (8-16): единый источник параметров для плагина И fallback ──
// P-соотношения нормированы (P_Na+P_K+P_Ca = 1). EC50 — относительные единицы выброса
// (физические µM — задача #20 «шкала выброса»). Кинетика tau — мс.

// ── γ (одноканальная проводимость, пС) ионотропных рецепторов (аудит #83) ──
// Источники: Traynelis 2010 (AMPA/NMDA/kainate), Moss & Smart 2001 (GABAA),
// Lynch 2004 (глицин), Dani & Bertrand 2007 (никотин), Barnes 2009 (5HT3), North 2002 (P2X).
#define AMPA_GAMMA_PS       8.0f
#define NMDA_GAMMA_PS       50.0f
#define KAINATE_GAMMA_PS    3.0f
#define GABAA_GAMMA_PS      28.0f
#define GLYCINE_GAMMA_PS    60.0f
#define NICOTINIC_A4B2_GAMMA_PS 30.0f
#define NICOTINIC_A7_GAMMA_PS   40.0f
#define HT3_GAMMA_PS        2.0f
#define P2X_GAMMA_PS        15.0f

// ── γ (пС) сенсорных каналов (аудит #83, сенсорная партия) ──
// MET (механотрансдукция, волосковая клетка): 100–300 пС — Corey & Hudspeth 1979, Nature 281:675.
// CNG (cGMP-gated, фототрансдукция палочки): ~25 пС — Kaupp & Seifert 2002, Physiol Rev 82:769.
// TRP-подобный (physical/hunger/light, неселективный катионный): ~50 пС.
#define MET_GAMMA_PS        150.0f
#define CNG_GAMMA_PS        25.0f
#define TRP_SENSOR_GAMMA_PS 50.0f

// AMPA (8): GluA2-содержащий, P_Na:P_K≈1:1 (E_rev≈0 мВ), Ca²⁺-непроницаем (Traynelis 2010).
#define AMPA_P_NA_RATIO   0.5f
#define AMPA_P_K_RATIO    0.5f
#define AMPA_P_CA_RATIO   0.0f
#define AMPA_EC50_REL     0.5f    // относительные единицы (задача #20)
#define AMPA_TAU_RISE_MS  0.5f
#define AMPA_TAU_DECAY_MS 3.0f

// NMDA (9) — GluN1/GluN2A-содержащий (ВЗРОСЛЫЙ, быстрая деактивация): P_Ca/P_Na ≈ 10.6,
// P_K/P_Na ≈ 1 (Mayer & Westbrook 1987). Mg²⁺-блок (Woodhull). Изоформа GluN2B — ниже (тип 194).
#define NMDA_P_NA_RATIO   0.08f   // норм. 1/12.6
#define NMDA_P_K_RATIO    0.08f   // норм. 1/12.6
#define NMDA_P_CA_RATIO   0.84f   // норм. 10.6/12.6
#define NMDA_EC50_REL     1.0f    // относительные единицы (задача #20)
// bath-NMDA: постоянный фоновый глутамат (изолированный спинной мозг, fictive swimming).
// Активирует NMDA-рецепторы на соме dIN → NMDAR-пейсмейкер (~10 Гц) (Soffe & Roberts 1989).
#define BATH_NMDA_GLUTAMATE 1.0f  // = EC50 (полуактивация NMDA)
#define BATH_5HT_SEROTONIN  0.0f  // 5-HT тонический ВЫКЛЮЧЕН: steady-depol даёт сеть (aIN+восходящая d→d), НЕ тонический Ih (иначе L/R-асимметрия)
#define NMDA_TAU_RISE_MS  5.0f    // GluN2A: быстрый rise
#define NMDA_TAU_DECAY_MS 50.0f   // GluN2A: быстрая деактивация (взрослый)
#define NMDA_MG_K0_MM     3.57f   // K_0 при 0 мВ (Mg²⁺-блок GluN2A, [Mg]=1 мМ)
#define NMDA_MG_COEF_PER_MV 0.062f // δ·z·F/(RT·1000) per мВ, δ≈0.83

// NMDA GluN2B (194) — эмбриональный/неонатальный (Xenopus dIN): медленная деактивация
// τ_decay ~100-400 мс (рекомб. GluN2B ~400 мс, нативный синапт. ~100 мс) → длинные NMDA-EPSP
// (Roberts & Tunstall 1990). Тот же GHK Na/K/Ca (P_Ca/P_Na 10.6). Mg-блок = GluN2A (оба δ~1).
#define NMDA2B_TAU_RISE_MS  10.0f   // GluN2B: медленнее rise
#define NMDA2B_TAU_DECAY_MS 150.0f  // GluN2B: медленная деактивация (в лит. диапазоне 100-400 мс)

// NMDA CDI (Ca²⁺/кальмодулин-инактивация) — единая для GluN2A/GluN2B (Iacobucci & Popescu 2019).
// cdi_inf = 1 − CDI_MAX·Ca/(Ca+Kd). Kd согласован с Cav1.2 (CAV12_CDI_KD_UM=1 µM).
#define NMDA_CDI_MAX             0.8f      // max инактивация 80% (20% residual)
#define NMDA_CDI_KD_UM           1.0f      // Ca²⁺/CaM Kd (µM)
#define NMDA_CDI_TAU_ONSET_MS    500.0f    // onset CDI (Ca²⁺ растёт)
#define NMDA_CDI_TAU_RECOVERY_MS 9000.0f   // recovery CDI (Ca²⁺ падает)

// Глицин-коагонист NMDA (насыщен в синапсе): coagonist = gly/(gly+EC50) ≈ 0.91.
#define NMDA_COAG_GLYCINE_UM    10.0f      // внеклеточный глицин/D-серин (насыщение)
#define NMDA_COAG_EC50_UM       1.0f       // EC50 коагониста (0.5–2 µM)

// Kainate (10): P_Na:P_K ≈ 1:1, P_Ca=0 (edited-форма, GluK2(R)).
#define KAINATE_P_NA_RATIO  0.5f
#define KAINATE_P_K_RATIO   0.5f
#define KAINATE_P_CA_RATIO  0.0f
#define KAINATE_EC50_REL    0.5f    // относительные (задача #20)
#define KAINATE_TAU_RISE_MS 1.0f
#define KAINATE_TAU_DECAY_MS 10.0f

// GABAA (11): Cl⁻-анионный канал (тормозной, E_rev=E_Cl).
#define GABAA_EC50_REL     1.0f    // относительные (задача #20)
#define GABAA_TAU_RISE_MS  1.0f
#define GABAA_TAU_DECAY_MS 5.0f

// Glycine (12): Cl⁻-анионный канал (очень быстрый, тормозной).
#define GLYCINE_EC50_REL    0.5f   // относительные (задача #20)
#define GLYCINE_TAU_RISE_MS 0.3f   // GlyR очень быстрый (0.2-0.4 мс)
#define GLYCINE_TAU_DECAY_MS 5.0f

// Nicotinic α4β2 (13): высокоаффинный, низкий Ca²⁺ (P_Ca/P_Na≈0.1), медленная десенситизация.
#define NICOTINIC_A4B2_P_NA_RATIO   0.48f
#define NICOTINIC_A4B2_P_K_RATIO    0.48f
#define NICOTINIC_A4B2_P_CA_RATIO   0.04f
#define NICOTINIC_A4B2_EC50_REL     0.5f    // относительные (задача #20)
#define NICOTINIC_A4B2_TAU_RISE_MS  1.0f
#define NICOTINIC_A4B2_TAU_DECAY_MS 10.0f

// Nicotinic α7 (97): низкоаффинный, высокий Ca²⁺ (P_Ca/P_Na≈6-10), быстрая десенситизация.
#define NICOTINIC_A7_P_NA_RATIO   0.12f
#define NICOTINIC_A7_P_K_RATIO    0.17f
#define NICOTINIC_A7_P_CA_RATIO   0.71f
#define NICOTINIC_A7_EC50_REL     0.5f    // относительные (задача #20)
#define NICOTINIC_A7_TAU_RISE_MS  0.5f
#define NICOTINIC_A7_TAU_DECAY_MS 3.0f

// ── Десенситизация ионотропных (τ_desens, мс) — двухстадийная модель ──
// K_desens по умолчанию = EC50 (в хелпере). 0 = десенситизация не моделируется.
#define AMPA_TAU_DESENS_MS        3.0f    // AMPA: быстрая (~2-5мс)
#define NMDA_TAU_DESENS_MS        0.0f    // NMDA: синаптически незначима (одностадийная)
#define KAINATE_TAU_DESENS_MS     5.0f    // Kainate: быстрая
#define GABAA_TAU_DESENS_MS       2000.0f // GABAA: медленная (~1-5с)
#define GLYCINE_TAU_DESENS_MS     2000.0f // Glycine: медленная
#define NICOTINIC_A4B2_TAU_DESENS_MS 2000.0f // α4β2: медленная (~1-5с)
#define NICOTINIC_A7_TAU_DESENS_MS    50.0f  // α7: быстрая (~10-100мс)
#define HT3_TAU_DESENS_MS         800.0f  // 5HT3: медленная (~0.8с)

// 5HT3 (14): неселективный катионный (P_K ≈ P_Na, P_Ca/P_Na ≈ 0.5).
#define HT3_P_NA_RATIO   0.40f
#define HT3_P_K_RATIO    0.40f
#define HT3_P_CA_RATIO   0.20f
#define HT3_EC50_REL     0.3f    // относительные (задача #20)
#define HT3_TAU_RISE_MS  1.0f
#define HT3_TAU_DECAY_MS 10.0f
#define P2X_TAU_DESENS_MS         20.0f   // P2X: умеренная

// P2X (15): ATP-зависимый катионный (P_K≈P_Na, P_Ca/P_Na≈2.5, P2X2).
#define P2X_P_NA_RATIO   0.22f
#define P2X_P_K_RATIO    0.22f
#define P2X_P_CA_RATIO   0.56f
#define P2X_EC50_REL     0.5f    // относительные (задача #20)
#define P2X_TAU_RISE_MS  1.0f
#define P2X_TAU_DECAY_MS 20.0f   // P2X3 быстрый (~18-32мс)

// P2X2 (134): ATP-зависимый катионный, МЕДЛЕННАЯ десенситизация (~3-35с).
#define P2X2_P_NA_RATIO     0.22f
#define P2X2_P_K_RATIO      0.22f
#define P2X2_P_CA_RATIO     0.56f
#define P2X2_EC50_REL       0.5f    // относительные (задача #20)
#define P2X2_TAU_RISE_MS    1.0f
#define P2X2_TAU_DECAY_MS   20.0f   // EPSC (быстрее десенситизации)
#define P2X2_TAU_DESENS_MS  5000.0f // P2X2 медленная (~5с)
#define TRPV1_TAU_DESENS_MS       5000.0f // TRPV1: медленная (~2-7с)

// TRPV1 (16): температурный (>43°C) + капсаициновый сенсор. Кастомная активация (не NT-шаблон).
#define TRPV1_GAMMA_PS      90.0f   // γ одноканальная (пС, 50–100; ~90 @ +60 мВ, наружу-выпрямляющий)
#define TRPV1_T_THRESHOLD_C 43.0f   // температурный порог (>43°C, ноцицепция)
#define TRPV1_T_SLOPE_C     0.5f    // крутизна (крутой порог: f_T(37°C)≈6e-6, закрыт ниже 43°C)
#define TRPV1_EC50_CAP_REL  0.5f    // капсаицин EC50 (относительные, задача #20)
#define TRPV1_P_NA_RATIO    0.125f  // P_Na (норм., P_Ca/P_Na≈6)
#define TRPV1_P_K_RATIO     0.125f  // P_K (норм., P_Na:P_K≈1:1)
#define TRPV1_P_CA_RATIO    0.75f   // P_Ca (норм., P_Ca/P_Na≈6)
#define TRPV1_TAU_ACT_MS    200.0f  // медленная активация (~сотни мс)

// TRPM4 (69): Ca²⁺-активируемый НЕселективный катионный канал (Ullrich 2005, Launay 2002).
// Моновалент-селективный (Na⁺/K⁺), НЕ Ca²⁺-проницаем. Гейт: Hill([Ca²⁺]ᵢ).
#define TRPM4_GAMMA_PS      25.0f    // γ одноканальная (пС, Launay 2002: 20.9±0.5 SAN)
#define TRPM4_P_NA_RATIO    0.5f     // P_Na (P_Na:P_K≈1:1, E_rev≈0)
#define TRPM4_P_K_RATIO     0.5f     // P_K
#define TRPM4_EC50_CA_UM    0.5f     // EC50(Ca²⁺) ≈ 0.4-1 µM (Launay 2002)
#define TRPM4_HILL          2.0f     // Hill-коэффициент (кооперативность Ca²⁺-связывания)
#define TRPM4_TAU_MS        30.0f    // время активации (десятки мс)

// ── Метаботропный Gi/o → GIRK (Kir3.x) — инвард-ректификатор ──
#define GIRK_GAMMA_PS      35.0f     // Kir3.1/3.2 гетеромер γ (пС)
#define GIRK_TAU_ACT_MS    70.0f     // G-белковая активация (~50-100мс, с RGS)
#define GIRK_TAU_DEACT_MS  1500.0f   // деактивация (GTP-гидролиз Gα, ~1-2с)
#define GIRK_TAU_DESENS_MS 40000.0f  // десенситизация (RGS-зависимая, ~50%/60с)
#define GIRK_DESENS_DEPTH  0.55f     // глубина десенситизации (~50-60%)

// ── Метаботропный Gs → AC → cAMP → PKA (общий пул на сегмент) ──
// Молекулярные константы (литература, одинаковы для любой клетки). V_max — калибровка
// клетки через density (сколько AC/PDE экспрессирует клетка), НЕ молекулярная константа.
#define CAMP_BASAL_UM            0.1f    // базальный [cAMP] µM (Alberts 2002) — default без каскада
#define CAMP_CEILING_UM          10.0f   // потолок [cAMP] µM (clamp)
#define AC_KM_ATP_MM             0.1f    // AC K_m(ATP) мМ (Dessauer & Gilman 1996)
#define AC_BASAL                 0.05f   // конститутивная AC-активность (поддерживает basal cAMP)
#define AC_VMAX_UM_PER_S         1.0f    // AC V_max µM/с (калибровка × density)
#define PDE_KM_CAMP_UM           2.0f    // PDE4 K_m(cAMP) µM (low-Km, Arch & Newsholme 1976)
#define PDE_VMAX_UM_PER_S        1.0f    // PDE V_max µM/с (калибровка × density)
#define PKA_EC50_CAMP_UM         0.15f   // PKA EC50(cAMP) µM (Ka 40-188 nM type I)
#define PKA_HILL                 1.7f    // PKA Hill (2 cAMP/рег. субъед., кооперативность)
#define GS_TAU_ACT_MS            70.0f   // Gαs активация (GDP→GTP-обмен, ~70мс)
#define GS_TAU_DEACT_MS          1500.0f // Gαs деактивация (GTP-гидролиз, ~1-2с)
#define GS_TAU_DESENS_MS         70000.0f// GRK/β-аррестин десенситизация t½~70с
#define GS_DESENS_DEPTH          0.8f    // глубина десенситизации Gs (~80%)
#define HCN_CAMP_VHALF_SHIFT_MV  15.0f   // HCN ΔV½ на насыщ. cAMP (мВ, +14-20)
#define HCN_CAMP_EC50_UM         0.5f    // HCN cAMP EC50 (µM, 0.1-1.7)
#define CAV12_PKA_BOOST          1.0f    // PKA-буст Cav1.2 Ser1928 (доп. +100% → 2×)

// ── Метаботропный Gq → PLC → IP3 + DAG → IP3R/PKC (общий пул на сегмент) ──
#define PLC_BASAL            0.02f   // конститутивная PLC-активность (базальный IP3/DAG)
#define IP3_PLC_MM_PER_S     0.0005f // PLC продукция IP3 (мМ/с → ~0.5 µM/с на полный Gq)
#define IP3_TAU_MS           1000.0f // IP3-5-фосфатаза: деградация IP3 τ~1с
#define DAG_PLC_PER_S        0.5f    // PLC продукция DAG (норм./с, 1:1 с IP3)
#define DAG_TAU_MS           2000.0f // DAG-киназа: деградация DAG τ~2с
#define GQ_TAU_ACT_MS        70.0f   // Gαq активация (GDP→GTP, ~70мс)
#define GQ_TAU_DEACT_MS      1500.0f // Gαq деактивация (GTP-гидролиз, ~1-2с)
#define GQ_TAU_DESENS_MS     70000.0f// GRK/β-аррестин десенситизация t½~70с
#define GQ_DESENS_DEPTH      0.8f    // глубина десенситизации Gq (~80%)
#define PKC_K_DAG            0.1f    // PKC K_d DAG (норм.)
#define PKC_K_CA_MM          0.0005f // PKC K_d Ca²⁺ (мМ = 0.5 µM, C2-домен классической PKC)
#define PKC_KV7_INHIBIT      0.8f    // PKC подавляет M-ток (до −80%, фосфорилирование KCNQ)
#define PKC_KV12_INHIBIT     0.8f    // PKC подавляет Kv1.2 (mGluR1→Gq→PKC→тирозин-фосфорилирование→эндоцитоз)
#define DTX_IC50_KV11        2.0f    // α-дендротоксин IC50 Kv1.1 (нМ, GtoPdb)
#define DTX_IC50_KV12        5.0f    // α-дендротоксин IC50 Kv1.2 (нМ)
#define TEA_IC50_KV1X        170.0f  // TEA IC50 (µМ, высокочувствительная компонента)
#define CDI_CAV23            0.3f    // Cav2.3 слабая Ca²⁺/CaM-зависимая инактивация (R-type << L-type)
#define SNX482_IC50          30.0f   // SNX-482 IC50 Cav2.3 (нМ)
#define SNX482_VHALF_SHIFT   70.0f   // SNX-482 сдвигает V½ активации +70 мВ (не поровая окклюзия)
#define PKC_KV34_DISINHIBIT  0.9f    // PKC убирает N-ball инактивацию Kv3.4 (dis-inhibition)
#define AP4_IC50_KV34        500.0f  // 4-AP IC50 Kv3.4 (µМ)
#define TEA_IC50_KV34        1000.0f // TEA IC50 Kv3.4 (µМ)
#define SHK_IC50_KV13        11.0f   // ShK-186 IC50 Kv1.3 (пМ, селективный блокатор)
#define TTX_IC50_NAV16       5.0f    // TTX IC50 Nav1.6 (нМ)
#define TTX_IC50_NAV12       10.0f   // TTX IC50 Nav1.2 (нМ)
#define PKA_PKC_INHIBIT_NAV16 0.2f   // PKA/PKC ингибиция Nav1.6 (слабая — нет Ser573)
#define PKA_PKC_INHIBIT_NAV12 0.7f   // PKA/PKC ингибиция Nav1.2 (сильная — Ser573)
#define TERTIAPINQ_IC50_GIRK     8.0f    // tertiapin-Q IC50 GIRK (нМ)
#define GLIBENCLAMIDE_IC50_KATP  3.0f    // глибенкламид IC50 KATP (нМ)
#define ML204_IC50_TRPC4         2.0f    // ML204 IC50 TRPC4 (µМ)
#define HC070_IC50_TRPC5         9.3f    // HC-070 IC50 TRPC5 (нМ)
#define PHENANTHROL_IC50_TRPM4   18.0f   // 9-phenanthrol IC50 TRPM4 (µМ)
#define HALOTHANE_IC50_THIK      2.8f    // галотан IC50 THIK-1 (мМ)
#define GVIA_IC50_CAV22          0.5f    // ω-conotoxin GVIA IC50 Cav2.2 (нМ)

// ── Устаревшие (используются глиальным насосом 110 и PMCA 98, убрать при их аудите) ──
#define NA_PUMP_PMOL_TO_NA  1e-3f   // legacy: I_max = p_rate·density·area·1e-3
#define NA_PUMP_ATP_RATE    1e-7f   // legacy: ATP_level -= I·dt·1e-7

// ═══ ИОН-СПЕЦИФИЧНЫЕ логарифмические средние (chord-conductance, Hille) ═══
// Честный одномолекулярный перевод: P_single = γ·RT/(z²F²·c̄), c̄ = (c_i−c_o)/ln(c_i/c_o).
// Покойные концентрации (мМ ≡ моль/м³). Вводится этапом 1 плана одномолекулярной схемы
// (notes/PLAN_single_molecule_ghk.md) — заменит суррогатный c_ref=150 мМ.
#define GHK_CBAR_NA_MM 55.964f   // (15−140)/ln(15/140)
#define GHK_CBAR_K_MM  40.514f   // (140−5)/ln(140/5)
#define GHK_CBAR_CA_MM 0.12775f  // (0.0001−1.2)/ln(0.0001/1.2)
#define GHK_CBAR_CL_MM 36.20f    // (5−120)/ln(5/120)
#define GHK_CBAR_H_MM  5.06e-5f  // (6.3e-5−4.0e-5)/ln(6.3e-5/4.0e-5), pH 7.2/7.4

// pS → S (одноканальная проводимость, для P_single в SI)
#define PS_TO_S 1e-12f
