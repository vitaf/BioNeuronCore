// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
#include "types.h"
#include "modifier_interface.h"

// SYCL/CUDA-совместимость: при компиляции SYCL/CUDA помечаем функции
// как доступные для межъюнитового device-линкинга
#if defined(__INTEL_LLVM_COMPILER)
#define BIOCORE_DEVICE_CALLABLE __attribute__((sycl_device))
#elif defined(__CUDACC__)
#define BIOCORE_DEVICE_CALLABLE __host__ __device__
#else
#define BIOCORE_DEVICE_CALLABLE
#endif

// Для static inline: только __host__ __device__ (без sycl_device — Intel не разрешает)
#if defined(__CUDACC__)
#define BIOCORE_HOST_DEVICE __host__ __device__
#else
#define BIOCORE_HOST_DEVICE
#endif

// Portable device-code detection.
// Intel DPC++: __SYCL_DEVICE_ONLY__ during device compilation.
// CUDA nvcc: __CUDA_ARCH__ during device compilation.
// OpenMP target (CPU): no guard needed — function pointers work on CPU.
#if defined(__SYCL_DEVICE_ONLY__) || defined(__NVPTX__) || defined(__SPIRV__) || defined(__CUDA_ARCH__)
#define BIOCORE_IS_DEVICE_CODE 1
#else
#define BIOCORE_IS_DEVICE_CODE 0
#endif

// ==================== EXTERN: dispatch_user — из dispatch_generated.cpp ==========
// dispatch_generated.cpp генерируется SDK (biocore-gen-dispatch) при добавлении
// пользовательских плагинов. Содержит switch для типов 121-127.
// Базовая заглушка (без SDK) возвращает 0.0f / 1.0f для всех типов.
BIOCORE_DEVICE_CALLABLE double dispatch_user_modifier_current(uint8_t type, Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE double dispatch_user_release_modulator(uint8_t type, Modifier* m, float* cleft_nt, float dt);

// ==================== EXTERN: функции из плагинов (для GPU/SYCL) =========
// Эти extern используются dispatch_generated.cpp (генерируется SDK).
// Для CPU-пути функции загружаются через .so и регистрируются в таблицах.

// ==================== EXTERN: базовые модификаторы =======================
BIOCORE_DEVICE_CALLABLE extern double nav16_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nav12_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nav12dev_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nav11_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cav21_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cav14_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cabp4_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cacc_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double glast_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double glyt1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double gat3_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double eaat3_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double glyt2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double gat1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double aqp4_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cilia_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double serca_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ip3r_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ryr_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double hcn1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double hcn2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double hcn3_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double hcn4_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nalcn_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ca_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cl_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double metabotropic_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double myelin_modulator(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double synaptic_release_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double physical_sensor_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nak_pump_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nak_pump_a3_current(Modifier* m, Segment* s, float dt);

// ==================== EXTERN: ионотропные рецепторы ======================
BIOCORE_DEVICE_CALLABLE extern double ampa_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nmda_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nmda2b_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kainate_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double gabaa_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double glycine_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nicotinic_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ht3_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double p2x_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double p2x2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpv1_current(Modifier* m, Segment* s, float dt);

// ==================== EXTERN: метаботропные Gi/o =========================
BIOCORE_DEVICE_CALLABLE extern double gabab_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ht1a_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ht1b_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ht1d_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ht1f_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double d2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double d3_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double d4_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double m2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double m4_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double a1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double a3_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mglur2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mglur3_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mglur4_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mglur6_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mglur7_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mglur8_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double alpha2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cb1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cb2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double h3_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mt1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mt2_current(Modifier* m, Segment* s, float dt);

// ==================== EXTERN: метаботропные Gs ===========================
BIOCORE_DEVICE_CALLABLE extern double ht4_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ht6_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ht7_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double d1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double d5_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double a2a_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double a2b_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double beta1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double beta2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double h2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double taar1_current(Modifier* m, Segment* s, float dt);

// ==================== EXTERN: NB-рецепторы (Neuroblox-совместимые) ========
BIOCORE_DEVICE_CALLABLE extern double nb_nmda_moradi_full_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_ampa_sta_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_d1_modulator_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_d2_modulator_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_alpha7_er_nachr_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_beta2_nachr_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_ca_trpm4r_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_muscarinic_r_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_ampa_destexhe_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_nmda_destexhe_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_gabaa_destexhe_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_gabab_destexhe_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_msn_nmdar_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_msn_ampar_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_soma_atp_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_soma_thermal_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nb_creb_transcription_current(Modifier* m, Segment* s, float dt);

// ==================== EXTERN: метаботропные Gq/11 ========================
BIOCORE_DEVICE_CALLABLE extern double ht2a_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ht2b_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ht2c_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double alpha1a_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double alpha1b_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double m1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double m3_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double m5_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mglur1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mglur5_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double h1_current(Modifier* m, Segment* s, float dt);

// ==================== EXTERN: пресинаптические модуляторы ===============
BIOCORE_DEVICE_CALLABLE extern float ht1a_modulate_release(Modifier* m, float* cleft_nt, float dt);
BIOCORE_DEVICE_CALLABLE extern float ht1b_modulate_release(Modifier* m, float* cleft_nt, float dt);
BIOCORE_DEVICE_CALLABLE extern float d2_modulate_release(Modifier* m, float* cleft_nt, float dt);
BIOCORE_DEVICE_CALLABLE extern float a1_modulate_release(Modifier* m, float* cleft_nt, float dt);
BIOCORE_DEVICE_CALLABLE extern float a2a_modulate_release(Modifier* m, float* cleft_nt, float dt);
BIOCORE_DEVICE_CALLABLE extern float d1_modulate_release(Modifier* m, float* cleft_nt, float dt);
BIOCORE_DEVICE_CALLABLE extern float mglur2_modulate_release(Modifier* m, float* cleft_nt, float dt);
BIOCORE_DEVICE_CALLABLE extern float mglur3_modulate_release(Modifier* m, float* cleft_nt, float dt);
BIOCORE_DEVICE_CALLABLE extern float cb1_modulate_release(Modifier* m, float* cleft_nt, float dt);
BIOCORE_DEVICE_CALLABLE extern float h3_modulate_release(Modifier* m, float* cleft_nt, float dt);
BIOCORE_DEVICE_CALLABLE extern float nicotinic_modulate_release(Modifier* m, float* cleft_nt, float dt);
BIOCORE_DEVICE_CALLABLE extern float alpha2_modulate_release(Modifier* m, float* cleft_nt, float dt);

// === GENERIC-ТИПЫ (параметрическая кинетика, без .so) ===
BIOCORE_DEVICE_CALLABLE extern double camkii_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double calcineurin_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ca_l_type_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv42_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv43_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv32_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv33_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv22_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv14_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv16_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kir22_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kca31_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpc3_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpm7_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv31_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double task_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double can_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double pkd2l1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv13_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cng_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv11_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv12_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cav23_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv34_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv21_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double girk_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double katp_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpc4_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpc5_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpm4_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpm5_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double thik1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double thik2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cav22_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ecb_synthesis_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mapk_erk_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ikk_nfkb_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mtorc1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double protein_synth_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cofilin_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mmp9_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double hunger_sensor_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double light_sensor_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double olfactory_receptor_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double pir_ttype_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cav33_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cav31_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cav32_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double rod_phototransduction_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nicotinic_alpha7_current(Modifier* m, Segment* s, float dt);
// === Ca²⁺/K⁺/Cl⁻ гомеостаз (IDs 98-102) ===
BIOCORE_DEVICE_CALLABLE extern double pmca_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ncx_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kcc2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nkcc1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv_delayed_rectifier_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ca_leak_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double ca_buffer_current(Modifier* m, Segment* s, float dt);
// === Ионные каналы ритмогенеза (IDs 103-106) ===
BIOCORE_DEVICE_CALLABLE extern double sk_channel_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double bk_channel_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nap_channel_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nap_tadpole_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kir_channel_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double k2p_leak_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double thermal_noise_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kv7_mcurrent_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double pp1_inhibitor_darpp32_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double connexin_cx36_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double connexin_cx43_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double camp_pka_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double plc_ip3_dag_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double pkc_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double vibration_sensor_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double otolith_hair_cell_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double semicircular_cupula_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double electroreceptor_ampullary_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double magnetite_mechanoreceptor_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double cryptochrome_magnetic_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kna_channel_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double glial_kir41_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double glial_nak_pump_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nav17_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nav18_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nav19_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpv1_channel_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpm8_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpa1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double mrgpr_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double piezo2_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double tmc1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpv3_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kcnq4_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double nav13_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double kir71_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double trpc67_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double calhm13_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double otop1_current(Modifier* m, Segment* s, float dt);
BIOCORE_DEVICE_CALLABLE extern double enac_current(Modifier* m, Segment* s, float dt);

// ==================== ХЕЛПЕРЫ ДЛЯ ПРОВЕРКИ МИЕЛИНИЗАЦИИ ================

// Проверка: является ли тип ионотропным (блокируется миелином)
static inline BIOCORE_HOST_DEVICE bool is_ionotropic_type(uint8_t type) {
    return (type <= MOD_IONOTROPIC_CL) ||
           (type >= MOD_NT_RECEPTOR_AMPA && type <= MOD_NT_RECEPTOR_TRPV1) ||
           (type == MOD_NICOTINIC_ALPHA7) ||
           (type == MOD_P2X2) ||
           (type == MOD_NT_RECEPTOR_NMDA_2B);
}

// Проверка: есть ли на сегменте миелин
static inline BIOCORE_HOST_DEVICE bool segment_has_myelin(Segment* s) {
    for (uint8_t j = 0; j < s->modifier_count; j++) {
        if (s->modifiers[j].type == MOD_MYELIN) return true;
    }
    return false;
}

// Миелин блокирует мембранные каналы (ионотропные + утечка + потенциал-зависимые K/Ca/HCN/NaP),
// НО не насосы/транспортёры (NAK/PMCA/NCX/KCC2/NKCC1) и не внутриклеточные каскады (CaMKII/буферы).
static inline BIOCORE_HOST_DEVICE bool is_myelin_blocked(uint8_t type) {
    return is_ionotropic_type(type) ||
           type == MOD_K2P_LEAK || type == MOD_TASK || type == MOD_CAN || type == MOD_PKD2L1 || type == MOD_NALCN ||
           type == MOD_KV42 || type == MOD_KV43 || type == MOD_KV32 || type == MOD_KV33 || type == MOD_KV31 || type == MOD_KV22 || type == MOD_KV14 || type == MOD_KV16 || type == MOD_KV13 || type == MOD_KV11 || type == MOD_KV12 || type == MOD_KV34 || type == MOD_KV7_MCURRENT || type == MOD_KV_DELAYED_RECTIFIER ||
           type == MOD_KNA_CHANNEL || type == MOD_NAP_CHANNEL || type == MOD_NAP_TADPOLE ||
           type == MOD_CA_L_TYPE || type == MOD_PIR_TTYPE_CALCIUM || type == MOD_CAV33 || type == MOD_CAV31 || type == MOD_CAV32 || type == MOD_CAV21 || type == MOD_CAV23 || type == MOD_CAV22 ||
           type == MOD_HCN1 || type == MOD_HCN2 || type == MOD_HCN3 || type == MOD_HCN4 ||
           type == MOD_SK_CHANNEL || type == MOD_KCA31 || type == MOD_BK_CHANNEL;
}

// ==================== СМЕШАННЫЙ ДИСПЕТЧ (BIO + пользовательские) ==========

// Возвращает функцию модификатора: пользовательская (arbitrary) если задана, иначе био
static inline BIOCORE_HOST_DEVICE ModifierFunc get_modifier_func(uint8_t type) {
    if (type < MOD_TYPE_COUNT && arbitrary_library[type] != nullptr)
        return arbitrary_library[type];
    if (type < MOD_TYPE_COUNT)
        return biological_library[type];
    return nullptr;
}

// Возвращает функцию пресинаптического модулятора: пользовательская если задана, иначе био
static inline BIOCORE_HOST_DEVICE ReleaseModulatorFunc get_modulator_func(uint8_t type) {
    if (type < MOD_TYPE_COUNT && arbitrary_modulators[type] != nullptr)
        return arbitrary_modulators[type];
    if (type < MOD_TYPE_COUNT)
        return biological_modulators[type];
    return nullptr;
}

// ==================== ДИСПЕТЧЕРЫ ==========================================

// Диспетчер токов модификаторов.
// CPU-путь: проверяет modifier_current_table[type] (заполняется плагинами)
// GPU-путь: switch на встроенных функциях (SYCL не поддерживает function ptr)
// Пользовательские плагины без встроенного fallback — только CPU.
static inline BIOCORE_HOST_DEVICE double dispatch_modifier_current(uint8_t mode, uint8_t type, Modifier* m, Segment* s, float dt) {
    (void)mode;
    // CPU-путь: таблица функций, заполненная плагинами
#if !BIOCORE_IS_DEVICE_CODE
    if (type < MOD_TABLE_SIZE && modifier_current_table[type] != nullptr) {
        return modifier_current_table[type](m, s, dt);
    }
#endif
    // GPU-путь (или CPU без плагина): switch на встроенных функциях
    switch (type) {
        // Базовые модификаторы
        case MOD_CAV13:            return ca_current(m, s, dt);
        case MOD_IONOTROPIC_CL:    return cl_current(m, s, dt);
        case MOD_METABOTROPIC:     return metabotropic_current(m, s, dt);
        case MOD_MYELIN:           return myelin_modulator(m, s, dt);
        case MOD_SYNAPTIC_RELEASE: return synaptic_release_current(m, s, dt);
        case MOD_PHYSICAL_SENSOR:  return physical_sensor_current(m, s, dt);
        case MOD_PUMP_NAK:         return nak_pump_current(m, s, dt);
        case MOD_PUMP_NAK_A3:      return nak_pump_a3_current(m, s, dt);

        // Ионотропные рецепторы
        case MOD_NT_RECEPTOR_AMPA:      return ampa_current(m, s, dt);
        case MOD_NT_RECEPTOR_NMDA:      return nmda_current(m, s, dt);
        case MOD_NT_RECEPTOR_NMDA_2B:   return nmda2b_current(m, s, dt);
        case MOD_NT_RECEPTOR_KAINATE:   return kainate_current(m, s, dt);
        case MOD_NT_RECEPTOR_GABAA:     return gabaa_current(m, s, dt);
        case MOD_NT_RECEPTOR_GLYCINE:   return glycine_current(m, s, dt);
        case MOD_NT_RECEPTOR_NICOTINIC: return nicotinic_current(m, s, dt);
        case MOD_NT_RECEPTOR_5HT3:      return ht3_current(m, s, dt);
        case MOD_NT_RECEPTOR_P2X:       return p2x_current(m, s, dt);
        case MOD_NT_RECEPTOR_TRPV1:     return trpv1_current(m, s, dt);

        // Метаботропные Gi/o
        case MOD_NT_RECEPTOR_GABAB:  return gabab_current(m, s, dt);
        case MOD_NT_RECEPTOR_5HT1A:  return ht1a_current(m, s, dt);
        case MOD_NT_RECEPTOR_5HT1B:  return ht1b_current(m, s, dt);
        case MOD_NT_RECEPTOR_5HT1D:  return ht1d_current(m, s, dt);
        case MOD_NT_RECEPTOR_5HT1F:  return ht1f_current(m, s, dt);
        case MOD_NT_RECEPTOR_D2:     return d2_current(m, s, dt);
        case MOD_NT_RECEPTOR_D3:     return d3_current(m, s, dt);
        case MOD_NT_RECEPTOR_D4:     return d4_current(m, s, dt);
        case MOD_NT_RECEPTOR_M2:     return m2_current(m, s, dt);
        case MOD_NT_RECEPTOR_M4:     return m4_current(m, s, dt);
        case MOD_NT_RECEPTOR_A1:     return a1_current(m, s, dt);
        case MOD_NT_RECEPTOR_A3:     return a3_current(m, s, dt);
        case MOD_NT_RECEPTOR_MGLUR2: return mglur2_current(m, s, dt);
        case MOD_NT_RECEPTOR_MGLUR3: return mglur3_current(m, s, dt);
        case MOD_NT_RECEPTOR_MGLUR4: return mglur4_current(m, s, dt);
        case MOD_NT_RECEPTOR_MGLUR6: return mglur6_current(m, s, dt);
        case MOD_NT_RECEPTOR_MGLUR7: return mglur7_current(m, s, dt);
        case MOD_NT_RECEPTOR_MGLUR8: return mglur8_current(m, s, dt);
        case MOD_NT_RECEPTOR_ALPHA2: return alpha2_current(m, s, dt);
        case MOD_NT_RECEPTOR_CB1:    return cb1_current(m, s, dt);
        case MOD_NT_RECEPTOR_CB2:    return cb2_current(m, s, dt);
        case MOD_NT_RECEPTOR_H3:     return h3_current(m, s, dt);
        case MOD_NT_RECEPTOR_MT1:    return mt1_current(m, s, dt);
        case MOD_NT_RECEPTOR_MT2:    return mt2_current(m, s, dt);

        // Метаботропные Gs
        case MOD_NT_RECEPTOR_5HT4:  return ht4_current(m, s, dt);
        case MOD_NT_RECEPTOR_5HT6:  return ht6_current(m, s, dt);
        case MOD_NT_RECEPTOR_5HT7:  return ht7_current(m, s, dt);
        case MOD_NT_RECEPTOR_D1:    return d1_current(m, s, dt);
        case MOD_NT_RECEPTOR_D5:    return d5_current(m, s, dt);
        case MOD_NT_RECEPTOR_A2A:   return a2a_current(m, s, dt);
        case MOD_NT_RECEPTOR_A2B:   return a2b_current(m, s, dt);
        case MOD_NT_RECEPTOR_BETA1: return beta1_current(m, s, dt);
        case MOD_NT_RECEPTOR_BETA2: return beta2_current(m, s, dt);
        case MOD_NT_RECEPTOR_H2:    return h2_current(m, s, dt);
        case MOD_NT_RECEPTOR_TAAR1: return taar1_current(m, s, dt);

        // Метаботропные Gq/11
        case MOD_NT_RECEPTOR_5HT2A:   return ht2a_current(m, s, dt);
        case MOD_NT_RECEPTOR_5HT2B:   return ht2b_current(m, s, dt);
        case MOD_NT_RECEPTOR_5HT2C:   return ht2c_current(m, s, dt);
        case MOD_NT_RECEPTOR_ALPHA1A: return alpha1a_current(m, s, dt);
        case MOD_NT_RECEPTOR_ALPHA1B: return alpha1b_current(m, s, dt);
        case MOD_NT_RECEPTOR_M1:      return m1_current(m, s, dt);
        case MOD_NT_RECEPTOR_M3:      return m3_current(m, s, dt);
        case MOD_NT_RECEPTOR_M5:      return m5_current(m, s, dt);
        case MOD_NT_RECEPTOR_MGLUR1:  return mglur1_current(m, s, dt);
        case MOD_NT_RECEPTOR_MGLUR5:  return mglur5_current(m, s, dt);
        case MOD_NT_RECEPTOR_H1:      return h1_current(m, s, dt);

        // NB-рецепторы (Neuroblox-совместимые)
        case MOD_NB_NMDA_MORADI_FULL: return nb_nmda_moradi_full_current(m, s, dt);
        case MOD_NB_AMPA_STA:         return nb_ampa_sta_current(m, s, dt);
        case MOD_NB_D1_MODULATOR:     return nb_d1_modulator_current(m, s, dt);
        case MOD_NB_D2_MODULATOR:     return nb_d2_modulator_current(m, s, dt);
        case MOD_NB_ALPHA7_ER_NACHR:  return nb_alpha7_er_nachr_current(m, s, dt);
        case MOD_NB_BETA2_NACHR:      return nb_beta2_nachr_current(m, s, dt);
        case MOD_NB_CA_TRPM4R:        return nb_ca_trpm4r_current(m, s, dt);
        case MOD_NB_MUSCARINIC_R:     return nb_muscarinic_r_current(m, s, dt);
        case MOD_NB_AMPA_DESTEXHE:    return nb_ampa_destexhe_current(m, s, dt);
        case MOD_NB_NMDA_DESTEXHE:    return nb_nmda_destexhe_current(m, s, dt);
        case MOD_NB_GABAA_DESTEXHE:   return nb_gabaa_destexhe_current(m, s, dt);
        case MOD_NB_GABAB_DESTEXHE:   return nb_gabab_destexhe_current(m, s, dt);
        case MOD_NB_MSN_NMDAR:        return nb_msn_nmdar_current(m, s, dt);
        case MOD_NB_MSN_AMPAR:        return nb_msn_ampar_current(m, s, dt);
        case MOD_NB_SOMA_ATP:         return nb_soma_atp_current(m, s, dt);
        case MOD_NB_SOMA_THERMAL:     return nb_soma_thermal_current(m, s, dt);
        case MOD_NB_CREB_TRANSCRIPTION: return nb_creb_transcription_current(m, s, dt);
        case MOD_CAMKII:                 return camkii_current(m, s, dt);
        case MOD_CALCINEURIN:            return calcineurin_current(m, s, dt);
        case MOD_CA_L_TYPE:              return ca_l_type_current(m, s, dt);
        case MOD_KV42:                   return kv42_current(m, s, dt);
        case MOD_KV43:                   return kv43_current(m, s, dt);
        case MOD_KV32:                   return kv32_current(m, s, dt);
        case MOD_KV33:                   return kv33_current(m, s, dt);
        case MOD_KV22:                   return kv22_current(m, s, dt);
        case MOD_KV14:                   return kv14_current(m, s, dt);
        case MOD_KV16:                   return kv16_current(m, s, dt);
        case MOD_KIR22:                  return kir22_current(m, s, dt);
        case MOD_KCA31:                  return kca31_current(m, s, dt);
        case MOD_TRPC3:                  return trpc3_current(m, s, dt);
        case MOD_TRPM7:                  return trpm7_current(m, s, dt);
        case MOD_ECB_SYNTHESIS:          return ecb_synthesis_current(m, s, dt);
        case MOD_MAPK_ERK:               return mapk_erk_current(m, s, dt);
        case MOD_IKK_NFKB:               return ikk_nfkb_current(m, s, dt);
        case MOD_MTORC1:                 return mtorc1_current(m, s, dt);
        case MOD_PROTEIN_SYNTH:          return protein_synth_current(m, s, dt);
        case MOD_COFILIN:                return cofilin_current(m, s, dt);
        case MOD_MMP9:                   return mmp9_current(m, s, dt);
        case MOD_HUNGER_SENSOR:           return hunger_sensor_current(m, s, dt);
        case MOD_LIGHT_SENSOR:            return light_sensor_current(m, s, dt);
        case MOD_OLFACTORY_RECEPTOR:      return olfactory_receptor_current(m, s, dt);
        case MOD_PIR_TTYPE_CALCIUM:       return pir_ttype_current(m, s, dt);
        case MOD_CAV33:                    return cav33_current(m, s, dt);
        case MOD_CAV31:                    return cav31_current(m, s, dt);
        case MOD_CAV32:                    return cav32_current(m, s, dt);
        case MOD_ROD_PHOTOTRANSDUCTION:     return rod_phototransduction_current(m, s, dt);
        case MOD_NICOTINIC_ALPHA7:         return nicotinic_alpha7_current(m, s, dt);
        case MOD_PMCA:            return pmca_current(m, s, dt);
        case MOD_NCX:             return ncx_current(m, s, dt);
        case MOD_KCC2:            return kcc2_current(m, s, dt);
        case MOD_NKCC1:           return nkcc1_current(m, s, dt);
        case MOD_KV_DELAYED_RECTIFIER: return kv_delayed_rectifier_current(m, s, dt);
        case MOD_CA_LEAK:         return ca_leak_current(m, s, dt);
        case MOD_CA_BUFFER:       return ca_buffer_current(m, s, dt);
        case MOD_SK_CHANNEL:     return sk_channel_current(m, s, dt);
        case MOD_BK_CHANNEL:     return bk_channel_current(m, s, dt);
        case MOD_NAP_CHANNEL:    return nap_channel_current(m, s, dt);
        case MOD_NAP_TADPOLE:    return nap_tadpole_current(m, s, dt);
        case MOD_KIR_CHANNEL:    return kir_channel_current(m, s, dt);
        case MOD_K2P_LEAK:       return k2p_leak_current(m, s, dt);
        case MOD_THERMAL_NOISE:  return thermal_noise_current(m, s, dt);
        case MOD_KV7_MCURRENT:  return kv7_mcurrent_current(m, s, dt);
        case MOD_KV31:           return kv31_current(m, s, dt);
        case MOD_KV13:           return kv13_current(m, s, dt);
        case MOD_CNG:            return cng_current(m, s, dt);
        case MOD_KV11:           return kv11_current(m, s, dt);
        case MOD_KV12:           return kv12_current(m, s, dt);
        case MOD_CAV23:          return cav23_current(m, s, dt);
        case MOD_KV34:           return kv34_current(m, s, dt);
        case MOD_KV21:           return kv21_current(m, s, dt);
        case MOD_GIRK:           return girk_current(m, s, dt);
        case MOD_KATP:           return katp_current(m, s, dt);
        case MOD_TRPC4:          return trpc4_current(m, s, dt);
        case MOD_TRPC5:          return trpc5_current(m, s, dt);
        case MOD_TRPM4:          return trpm4_current(m, s, dt);
        case MOD_TRPM5:          return trpm5_current(m, s, dt);
        case MOD_THIK1:          return thik1_current(m, s, dt);
        case MOD_THIK2:          return thik2_current(m, s, dt);
        case MOD_CAV22:          return cav22_current(m, s, dt);
        case MOD_TASK:           return task_current(m, s, dt);
        case MOD_CAN:            return can_current(m, s, dt);
        case MOD_PKD2L1:         return pkd2l1_current(m, s, dt);
        case MOD_PP1_INHIBITOR_DARPP32: return pp1_inhibitor_darpp32_current(m, s, dt);
        case MOD_CONNEXIN_CX36: return connexin_cx36_current(m, s, dt);
        case MOD_CONNEXIN_CX43: return connexin_cx43_current(m, s, dt);
        case MOD_CAMP_PKA:     return camp_pka_current(m, s, dt);
        case MOD_PLC_IP3_DAG:  return plc_ip3_dag_current(m, s, dt);
        case MOD_PKC:          return pkc_current(m, s, dt);
        case MOD_VIBRATION_SENSOR:       return vibration_sensor_current(m, s, dt);
        case MOD_OTOLITH_HAIR_CELL:      return otolith_hair_cell_current(m, s, dt);
        case MOD_SEMICIRCULAR_CUPULA:    return semicircular_cupula_current(m, s, dt);
        case MOD_ELECTRORECEPTOR_AMPULLARY: return electroreceptor_ampullary_current(m, s, dt);
        case MOD_MAGNETITE_MECHANORECEPTOR: return magnetite_mechanoreceptor_current(m, s, dt);
        case MOD_CRYPTOCHROME_MAGNETIC:     return cryptochrome_magnetic_current(m, s, dt);
        case MOD_KNA_CHANNEL:              return kna_channel_current(m, s, dt);
        case MOD_GLIAL_KIR41:            return glial_kir41_current(m, s, dt);
        case MOD_GLIAL_NAK_PUMP:         return glial_nak_pump_current(m, s, dt);
        case MOD_AXON_NAV16:              return nav16_current(m, s, dt);
        case MOD_AXON_NAV12:              return nav12_current(m, s, dt);
        case MOD_NAV12DEV:                return nav12dev_current(m, s, dt);
        case MOD_NAV11:                   return nav11_current(m, s, dt);
        case MOD_CAV21:                   return cav21_current(m, s, dt);
        case MOD_CAV14:                   return cav14_current(m, s, dt);
        case MOD_CABP4:                   return cabp4_current(m, s, dt);
        case MOD_CACC:                    return cacc_current(m, s, dt);
        case MOD_GLAST:                   return glast_current(m, s, dt);
        case MOD_GLYT1:                   return glyt1_current(m, s, dt);
        case MOD_GAT3:                    return gat3_current(m, s, dt);
        case MOD_SERCA:                   return serca_current(m, s, dt);
        case MOD_IP3R:                    return ip3r_current(m, s, dt);
        case MOD_RYR:                     return ryr_current(m, s, dt);
        case MOD_HCN1:                    return hcn1_current(m, s, dt);
        case MOD_HCN2:                    return hcn2_current(m, s, dt);
        case MOD_HCN3:                    return hcn3_current(m, s, dt);
        case MOD_HCN4:                    return hcn4_current(m, s, dt);
        case MOD_NALCN:                   return nalcn_current(m, s, dt);
        case MOD_P2X2:                    return p2x2_current(m, s, dt);

        case MOD_NAV17:                   return nav17_current(m, s, dt);
        case MOD_NAV18:                   return nav18_current(m, s, dt);
        case MOD_NAV19:                   return nav19_current(m, s, dt);
        case MOD_TRPV1_CHANNEL:           return trpv1_channel_current(m, s, dt);
        case MOD_TRPM8:                   return trpm8_current(m, s, dt);
        case MOD_TRPA1:                   return trpa1_current(m, s, dt);
        case MOD_TRPV3:                   return trpv3_current(m, s, dt);
        case MOD_KCNQ4:                   return kcnq4_current(m, s, dt);
        case MOD_NAV13:                   return nav13_current(m, s, dt);
        case MOD_KIR71:                   return kir71_current(m, s, dt);
        case MOD_TRPC67:                  return trpc67_current(m, s, dt);
        case MOD_CALHM13:                 return calhm13_current(m, s, dt);
        case MOD_OTOP1:                   return otop1_current(m, s, dt);
        case MOD_ENAC:                    return enac_current(m, s, dt);
        case MOD_EAAT3:                   return eaat3_current(m, s, dt);
        case MOD_GLYT2:                   return glyt2_current(m, s, dt);
        case MOD_GAT1:                    return gat1_current(m, s, dt);
        case MOD_AQP4:                    return aqp4_current(m, s, dt);
        case MOD_CILIA:                   return cilia_current(m, s, dt);
        case MOD_MRGPR:                   return mrgpr_current(m, s, dt);
        case MOD_PIEZO2:                  return piezo2_current(m, s, dt);
        case MOD_TMC1:                    return tmc1_current(m, s, dt);

        // Пользовательские типы (132+): пробуем dispatch_generated
#if !BIOCORE_IS_DEVICE_CODE
        default: return dispatch_user_modifier_current(type, m, s, dt);
#else
        default: return 0.0f;
#endif
    }
}

// Диспетчер пресинаптических модуляторов.
// CPU-путь: проверяет modulator_release_table[type] (заполняется плагинами)
// GPU-путь: switch на встроенных функциях.
static inline BIOCORE_HOST_DEVICE double dispatch_release_modulator(uint8_t mode, uint8_t type, Modifier* m, float* cleft_nt, float dt) {
    (void)mode;
    // CPU-путь: таблица функций, заполненная плагинами
#if !BIOCORE_IS_DEVICE_CODE
    if (type < MOD_TABLE_SIZE && modulator_release_table[type] != nullptr) {
        return modulator_release_table[type](m, cleft_nt, dt);
    }
#endif
    // GPU-путь (или CPU без плагина): switch на встроенных функциях
    switch (type) {
        case MOD_NT_RECEPTOR_5HT1A:     return ht1a_modulate_release(m, cleft_nt, dt);
        case MOD_NT_RECEPTOR_5HT1B:     return ht1b_modulate_release(m, cleft_nt, dt);
        case MOD_NT_RECEPTOR_D2:        return d2_modulate_release(m, cleft_nt, dt);
        case MOD_NT_RECEPTOR_A1:        return a1_modulate_release(m, cleft_nt, dt);
        case MOD_NT_RECEPTOR_A2A:       return a2a_modulate_release(m, cleft_nt, dt);
        case MOD_NT_RECEPTOR_D1:        return d1_modulate_release(m, cleft_nt, dt);
        case MOD_NT_RECEPTOR_MGLUR2:    return mglur2_modulate_release(m, cleft_nt, dt);
        case MOD_NT_RECEPTOR_MGLUR3:    return mglur3_modulate_release(m, cleft_nt, dt);
        case MOD_NT_RECEPTOR_CB1:       return cb1_modulate_release(m, cleft_nt, dt);
        case MOD_NT_RECEPTOR_H3:        return h3_modulate_release(m, cleft_nt, dt);
        case MOD_NT_RECEPTOR_NICOTINIC: return nicotinic_modulate_release(m, cleft_nt, dt);
        case MOD_NT_RECEPTOR_ALPHA2:    return alpha2_modulate_release(m, cleft_nt, dt);
        // Пользовательские типы (121-127): пробуем dispatch_generated
#if !BIOCORE_IS_DEVICE_CODE
        default: return dispatch_user_release_modulator(type, m, cleft_nt, dt);
#else
        default: return 1.0f;
#endif
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// RRP vesicle dynamics (BION v7) — общие CPU/GPU inline
// ═══════════════════════════════════════════════════════════════════════════

// RRP recovery: Ca²⁺/CaMKII/Calcineurin-зависимый priming.
// Вызывается каждый тик ДО проверки pre-spike.
static inline BIOCORE_HOST_DEVICE void rrp_do_recovery(Synapse* syn, Segment* pre, float dt) {
    if (syn->RRP_max <= 0.0f || syn->RRP >= syn->RRP_max) return;
    float ca_pre = pre->intra_ions[ION_CA];
    float ca_f = ca_pre / (0.0005f + ca_pre); // EC50=0.5µM
    float camkii = 0, calcn = 0;
    for (int mj = 0; mj < pre->modifier_count; mj++) {
        if (pre->modifiers[mj].type == MOD_CAMKII)
            camkii = pre->modifiers[mj].state[0];
        if (pre->modifiers[mj].type == MOD_CALCINEURIN)
            calcn = pre->modifiers[mj].state[0];
    }
    float priming = syn->RRP_recovery
        + camkii * 0.02f   // CaMKII → p-Synapsin → мобилизация
        + calcn * 0.01f;   // Calcineurin → dynamin → эндоцитоз
    priming *= ca_f;
    float rec = (syn->RRP_max - syn->RRP) * priming * dt;
    syn->RRP += rec;
    if (syn->RRP > syn->RRP_max) syn->RRP = syn->RRP_max;
}

// RRP depletion: cap release_amount по available vesicles,
// deplete RRP на фактически released объём.
// Вызывается при успешном pre-spike release.
// Возвращает скорректированный release_amt.
static inline BIOCORE_HOST_DEVICE float rrp_do_release(Synapse* syn, float release_amount, float p) {
    if (syn->RRP_max <= 0.0f) return release_amount * p; // RRP не моделируется
    float avail = release_amount;
    if (avail > syn->RRP) avail = syn->RRP;
    float release_amt = avail * p;
    syn->RRP -= release_amt;
    if (syn->RRP < 0) syn->RRP = 0;
    return release_amt;
}

// Protein→parameter mapping (BION v7): nucleus protein levels
// модулируют RRP_max, release_probability, RRP_recovery.
// Вызывается перед rrp_do_recovery().
// Био: Munc13-1 (UNC13A) — priming (Brose 2019), SYT1 — Ca²⁺ sensor (Sudhof 2013),
// CPLX1 — clamp (Trimbuch & Rosenmund 2016).
static inline BIOCORE_HOST_DEVICE void rrp_apply_protein_mapping(
    Synapse* syn, const float* protein_level)
{
    if (!protein_level) return;
    float munc13 = protein_level[GENE_UNC13A];
    syn->RRP_max = 10.0f + munc13 * 40.0f;        // baseline 10 → max 50
    float syt1 = protein_level[GENE_SYT1];
    syn->release_probability = 0.3f + syt1 * 0.7f; // 0.3 → 1.0
    float cplx1 = protein_level[GENE_CPLX1];
    syn->RRP_recovery = 0.02f + munc13 * 0.08f - cplx1 * 0.01f;
    if (syn->RRP_recovery < 0.005f) syn->RRP_recovery = 0.005f;
    if (syn->RRP_recovery > 0.2f) syn->RRP_recovery = 0.2f;
}
