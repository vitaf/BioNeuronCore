// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_182_enac.cpp — ENaC Na⁺-утечка
// Тип: MOD_ENAC (182). Категория: Other
// Био: ENaC — эпителиальный Na⁺-канал (солёный вкус тип I), конститутивный Po≈0.5
// (Kellenberger 2002). GHK-парадигма (γ=4.5 пС), амилорид-ингибиция (state[5], IC50 100 нМ).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double enac_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // конститутивный Po ~0.5
    float open = ENAC_PO;

    // амилорид-ингибиция (state[5], нМ): IC50 100 нМ
    double amil = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open /= (1.0f + amil / 100.0f);

    double P_single = ghk_single_channel_P(ENAC_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_NA] -= (I * dt) / cpm;
    s->extra_ions[ION_NA] += (I * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_ENAC, enac_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"ENaC Na⁺-утечка","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":182,"name":"ENaC Na⁺-утечка","category":"Other","description":"ENaC — эпителиальный Na⁺-канал (солёный вкус тип I). Конститутивный Po≈0.5. Амилорид-ингибиция (state[5] нМ, IC50 100 нМ). γ=4.5 пС. GHK Na⁺ (z=1). Kellenberger 2002.","release":false}]})BIOCORE";
}
