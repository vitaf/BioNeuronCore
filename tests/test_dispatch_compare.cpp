// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_dispatch_compare.cpp — сравнение .so плагинов vs fallback на CPU
#include <cstdio>
#include <cmath>
#include <cstring>

#include "types.h"
#include "modifier_interface.h"
#include "kernel_dispatch.h"
#include "plugin_manager.h"

int main() {
    int pass = 0, fail = 0;

    for (int i = 0; i < MOD_TYPE_COUNT; i++) {
        biological_library[i] = nullptr;
        arbitrary_library[i] = nullptr;
    }
    init_biological_library();
    plugin_load_builtin(".");  // загружаем .so из текущей директории

    Segment seg;
    memset(&seg, 0, sizeof(seg));
    seg.segment_type = 1;
    seg.radius = 5.0f;
    seg.length = 10.0f;
    seg.V_m = -65.0f;
    seg.intra_ions[0] = 0.015f;
    seg.intra_ions[1] = 0.140f;
    seg.intra_ions[2] = 0.00005f;
    seg.intra_ions[3] = 0.005f;
    seg.extra_ions[0] = 0.140f;
    seg.extra_ions[1] = 0.005f;
    seg.extra_ions[2] = 0.0012f;
    seg.extra_ions[3] = 0.130f;
    seg.ATP_level = 0.1f;

    Modifier mod;
    memset(&mod, 0, sizeof(mod));
    mod.state[7] = 4.0f * PI * seg.radius * seg.radius;

    auto test_one = [&](const char* label, uint8_t type, float density,
                        double (*direct_fn)(Modifier*, Segment*, float)) {
        mod.type = type;
        mod.density = density;

        double direct = direct_fn(&mod, &seg, 0.1f);

        ModifierFunc saved = modifier_current_table[type];
        modifier_current_table[type] = nullptr;
        double via_dispatch_fb = dispatch_modifier_current(0, type, &mod, &seg, 0.1f);
        modifier_current_table[type] = saved;

        double via_dispatch_pl = dispatch_modifier_current(0, type, &mod, &seg, 0.1f);

        printf("%-20s: direct=%+.6f  fb=%+.6f  plugin=%+.6f",
               label, direct, via_dispatch_fb, via_dispatch_pl);

        int ok = 1;
        if (fabs(direct) > 1e-6 && fabs(direct - via_dispatch_fb) > fabs(direct)*0.01 + 0.001) {
            printf("  FAIL: direct!=fb (%.2f%%)", fabs(direct-via_dispatch_fb)/fabs(direct)*100);
            ok = 0;
        }
        if (fabs(direct) > 1e-6 && fabs(direct - via_dispatch_pl) > fabs(direct)*0.01 + 0.001) {
            printf("  FAIL: direct!=plugin (%.2f%%)", fabs(direct-via_dispatch_pl)/fabs(direct)*100);
            ok = 0;
        }
        printf("\n");
        if (ok) pass++; else fail++;
    };

    test_one("K2P_LEAK",       107, 0.03f, k2p_leak_current);
    test_one("KIR_CHANNEL",    106, 0.008f, kir_channel_current);
    test_one("AXON_NAV12",    142, 15.6f, nav12_current);
    test_one("KV11",          156, 3.84f, kv21_current);
    test_one("PUMP_NAK",       7,   0.2f, nak_pump_current);
    test_one("SK_CHANNEL",     103, 0.25f, sk_channel_current);
    test_one("BK_CHANNEL",     104, 0.03f, bk_channel_current);
    test_one("NAP_CHANNEL",    105, 0.08f, nap_channel_current);
    test_one("KV42",           85,  0.3f, kv42_current);
    test_one("KV7_MCURRENT",   112, 1.0f, kv7_mcurrent_current);

    printf("\n=== %d PASS, %d FAIL ===\n", pass, fail);
    return fail ? 1 : 0;
}
