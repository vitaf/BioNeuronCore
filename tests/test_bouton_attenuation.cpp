// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_bouton_attenuation.cpp — #9: аттенюация пресинаптического ПД через делитель напряжения.
// Kv1 в pre_autoreceptors бутона реполяризует V_bouton → Ca²⁺-вход (bouton_Ca) меньше.
// (Усиление Na для mossy fiber — отдельно: требует Ca-канала со слабой инактивацией, не Cav2.1.)

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cstring>
#include "types.h"
#include "modifier_helpers.h"

static void init_pre_ions(Segment& s) {
    s.intra_ions[ION_NA] = 10.0f; s.intra_ions[ION_K] = 140.0f; s.intra_ions[ION_CA] = 0.0001f;
    s.intra_ions[ION_CL] = 10.0f; s.intra_ions[ION_MG] = 1.0f;
    s.extra_ions[ION_NA] = 120.0f; s.extra_ions[ION_K] = 5.0f; s.extra_ions[ION_CA] = 1.0f;
    s.extra_ions[ION_CL] = 120.0f; s.extra_ions[ION_MG] = 1.0f;
}

TEST_CASE("Bouton attenuation: Kv1 reduces Ca entry", "[bouton-attenuation]") {
    Segment pre = {};
    pre.V_m = -20.0f;  // окно Cav2.1 (активация > инактивации)
    init_pre_ions(pre);
    pre.modifier_count = 1;
    pre.modifiers[0].type = MOD_CAV21;
    pre.modifiers[0].density = 100.0f;
    // steady-state гейтинг Cav2.1 по pre->V_m (иначе за 1 шаг dt=0.1 m/h не активируются)
    pre.modifiers[0].state[0] = cav21_m_inf(pre.V_m);
    pre.modifiers[0].state[1] = cav21_h_inf(pre.V_m, 0.1f);  // Ca=0.1 µM (baseline)

    Synapse syn_noKv = {}, syn_Kv = {};
    syn_noKv.pre_area = 1.0f;
    syn_Kv.pre_area = 1.0f;
    syn_Kv.pre_autoreceptor_count = 1;
    syn_Kv.pre_autoreceptors[0].type = MOD_KV11;
    syn_Kv.pre_autoreceptors[0].density = 9.0f;

    float dt = 0.1f;
    bouton_ca_step(&pre, &syn_noKv, dt);  // без Kv1 (изопотенциален аксону)
    bouton_ca_step(&pre, &syn_Kv, dt);    // с Kv1 (аттенюированный ПД)

    printf("[ATTEN] bouton_Ca: no-Kv1=%.6f  with-Kv1=%.6f mM\n", syn_noKv.bouton_Ca, syn_Kv.bouton_Ca);

    CHECK(syn_Kv.bouton_Ca < syn_noKv.bouton_Ca);  // Kv1 аттенюирует → меньше Ca²⁺-вход
    CHECK(syn_Kv.bouton_Ca > 0.0f);                // но Ca²⁺-вход не обнулён
}
