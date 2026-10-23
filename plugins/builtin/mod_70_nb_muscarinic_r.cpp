// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_70_nb_muscarinic_r.cpp — NB_MuscarinicR
// Тип: MOD_NB_MUSCARINIC_R (70)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: M1-подобный метаботропный ACh-рецептор (Caulfield & Birdsall 1998).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nb_muscarinic_r_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — I_NCM (ACh→Ca→g суррогат) заменён на честный Gq-каскад (single-source с m1/m3/m5).
    // M1/M3/M5 → Gq/11 → PLC → IP3/DAG. Читает neurotransmitter[NT_ACETYLCHOLINE].
    return gq_signal(m, s, dt, NT_ACETYLCHOLINE);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_MUSCARINIC_R, nb_muscarinic_r_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_MuscarinicR","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":70,"name":"NB_MuscarinicR","category":"NB-рецепторы","description":"M1-подобный метаботропный ACh-рецептор (Caulfield & Birdsall 1998)","release":false}]})BIOCORE";
}
