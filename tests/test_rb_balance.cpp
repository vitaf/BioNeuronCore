// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_rb_balance.cpp — декомпозиция токов изолированного RB (Rohon-Beard) на покое
// и при voltage-clamp (сома). Методика calibration-process Принцип C2: для каждого
// модификатора сомы замерить фактический ток на заданном V → найти почему пик −23,
// а не регенеративный ПД (+30). ТОЛЬКО диагностика (плотности менять в cpg_config.h).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cstring>
#include "types.h"
#include "bion_loader.h"
#include "cpg_config.h"
#include "kernel_dispatch.h"

extern "C" {
    void core_run_tick(Segment*, uint32_t, IntraConnection*, uint32_t,
                       Synapse*, uint32_t, int, uint64_t*, float,
                       CurrentStimulus*, uint32_t*, VoltageClamp*, uint32_t*,
                       RampStimulus*, uint32_t*, IonInjection*, uint32_t*,
                       LigandInjection*, uint32_t*, MechanoStimulus*, uint32_t*);
}

static const char* mod_name(uint8_t t) {
    switch (t) {
        case MOD_IONOTROPIC_CL:  return "CL_leak";
        case MOD_PHYSICAL_SENSOR:return "PHYS_SENSOR";
        case MOD_PUMP_NAK:       return "PUMP_NAK";
        case MOD_PMCA:           return "PMCA";
        case MOD_KCC2:           return "KCC2";
        case MOD_CA_BUFFER:      return "CA_BUFFER";
        case MOD_K2P_LEAK:       return "K2P_LEAK";
        case MOD_NALCN:          return "NALCN";
        case MOD_NAV12DEV:     return "Nav1.2";
        case MOD_KV11:           return "Kv1.1";
        case MOD_KV21:           return "Kv2.1";
        default:                 return "?";
    }
}

static void tick(BionData& d, uint64_t& tc, int n,
                 CurrentStimulus* stim, uint32_t* sc,
                 VoltageClamp* vc, uint32_t* vcc) {
    for (int t = 0; t < n; t++)
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      stim, sc, vc, vcc,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

static void print_seg(BionData& d, uint32_t idx, const char* label) {
    Segment* s = &d.segments[idx];
    printf("[RB-BAL] %s (seg %u): V=%.2f mV\n", label, idx, s->V_m);
    printf("[RB-BAL]   Na_i=%.1f Na_o=%.1f | K_i=%.1f K_o=%.1f | Cl_i=%.1f Cl_o=%.1f\n",
           s->intra_ions[ION_NA], s->extra_ions[ION_NA],
           s->intra_ions[ION_K],  s->extra_ions[ION_K],
           s->intra_ions[ION_CL], s->extra_ions[ION_CL]);
    float sum = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++) {
        Modifier mm = s->modifiers[j];
        double I = dispatch_modifier_current(0, mm.type, &mm, s, 0.1f);
        sum += (float)I;
        printf("[RB-BAL]   %-12s (type %3d)  I = %8.4f nA\n",
               mod_name(mm.type), (int)mm.type, (float)I);
    }
    printf("[RB-BAL]   SUM = %.4f nA\n", sum);
}

static void run_point(float clamp_v, bool do_clamp) {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, 0, cpg::Y_RB, 0, cpg::R_RB, cpg::V_RB,
                    cpg::rb_soma_mods(), cpg::rb_ais_mods(), cpg::rb_axon_mods());
    char path[256]; snprintf(path, sizeof(path), "/tmp/rb_balance.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);

    uint64_t tc = 0;
    tick(d, tc, 3000, nullptr, nullptr, nullptr, nullptr);  // settle (300 мс)

    if (do_clamp) {
        VoltageClamp vc = {0, clamp_v, tc, tc + 2000};
        uint32_t vcc = 1;
        tick(d, tc, 2000, nullptr, nullptr, &vc, &vcc);     // clamp (200 мс)
    }

    char label[64];
    snprintf(label, sizeof(label), do_clamp ? "clamp %+.0f" : "rest", clamp_v);
    print_seg(d, 0, label);

    bion_unload_host(&d); std::remove(path);
}

TEST_CASE("RB inside build_cpg(2): why soma depolarized to -58", "[rb-net]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/rb_net.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);

    uint64_t tc = 0;
    tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

    int rb0 = cpg::rbL_idx(0, 0);
    print_seg(d, rb0,     "RB soma (net)");
    print_seg(d, rb0 + 1, "RB AIS  (net)");
    print_seg(d, rb0 + 2, "RB axon (net)");

    // Трассировка release: RB спайк → аксон (V) → Cav2.1 → bouton_Ca → cleft_glu → dIN.
    int dl0 = cpg::dinL_idx(0, 0);
    int rb_axon = rb0 + 2;
    int syn_idx = -1;
    for (uint32_t s = 0; s < d.num_synapses; s++) {
        if (d.synapses[s].pre_segment == (uint32_t)rb_axon &&
            d.synapses[s].post_segment == (uint32_t)dl0) { syn_idx = (int)s; break; }
    }
    printf("[RB-REL] RB->dIN synapse idx=%d (pre=axon %d, post=dIN %d)\n", syn_idx, rb_axon, dl0);

    CurrentStimulus stim = {(uint32_t)rb0, 800.0f, 50.0f, tc};
    uint32_t sc = 1;
    for (int t = 0; t < 200; t++) {
        tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
        if (t == 1 || t == 3 || t == 5 || t == 10 || t == 20 || t == 50 || t == 100) {
            float glu = (syn_idx >= 0) ? d.synapses[syn_idx].cleft_neurotransmitter[NT_GLUTAMATE] : -1.0f;
            float bca = (syn_idx >= 0) ? d.synapses[syn_idx].bouton_Ca : -1.0f;
            float rrp = (syn_idx >= 0) ? d.synapses[syn_idx].RRP : -1.0f;
            float postglu = d.segments[dl0].neurotransmitter[NT_GLUTAMATE];
            printf("[RB-REL] t=%3d  V_ax=%.2f  cleft_glu=%.5f  postGlu(dIN)=%.5f  RRP=%.1f  dIN=%.2f\n",
                   t, d.segments[rb_axon].V_m, glu, postglu, rrp, d.segments[dl0].V_m);
        }
    }

    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("dIN: HCN1 density scan -> rest + spike from RB", "[rb-hcn-scan]") {
    float hcn[] = {120.0f, 60.0f, 30.0f, 15.0f, 0.0f};
    for (float h : hcn) {
        cpg::CpgNetwork net = cpg::build_cpg(2);
        char path[256]; snprintf(path, sizeof(path), "/tmp/rb_hcn.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);

        for (uint32_t i = 0; i < d.num_segments; i++)
            for (uint8_t j = 0; j < d.segments[i].modifier_count; j++)
                if (d.segments[i].modifiers[j].type == MOD_HCN1)
                    d.segments[i].modifiers[j].density = h;

        uint64_t tc = 0;
        tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

        int rb0 = cpg::rbL_idx(0, 0), dl0 = cpg::dinL_idx(0, 0);
        float rest = d.segments[dl0].V_m;

        CurrentStimulus stim = {(uint32_t)rb0, 800.0f, 50.0f, tc};
        uint32_t sc = 1;
        float peak_din = -999.0f;
        for (int t = 0; t < 1000; t++) {
            tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
            if (d.segments[dl0].V_m > peak_din) peak_din = d.segments[dl0].V_m;
        }
        printf("[RB-HCN] HCN1_dens=%6.1f  dIN rest=%.2f  peak(from RB)=%.2f mV\n", h, rest, peak_din);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("RB->dIN: does raising AMPA density make dIN spike?", "[rb-ampa-scan]") {
    float ampa[] = {40.0f, 400.0f, 4000.0f};
    for (float a : ampa) {
        cpg::CpgNetwork net = cpg::build_cpg(2);
        char path[256]; snprintf(path, sizeof(path), "/tmp/rb_ampa.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);

        uint64_t tc = 0;
        tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

        int rb0 = cpg::rbL_idx(0, 0), dl0 = cpg::dinL_idx(0, 0), rb_axon = rb0 + 2;
        for (uint32_t s = 0; s < d.num_synapses; s++)
            if (d.synapses[s].pre_segment == (uint32_t)rb_axon &&
                d.synapses[s].post_segment == (uint32_t)dl0)
                d.synapses[s].post_receptors[0].density = a;

        CurrentStimulus stim = {(uint32_t)rb0, 800.0f, 50.0f, tc};
        uint32_t sc = 1;
        float peak_din = -999.0f;
        for (int t = 0; t < 1000; t++) {
            tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
            if (d.segments[dl0].V_m > peak_din) peak_din = d.segments[dl0].V_m;
        }
        printf("[RB-AMPA] RB_DIN_AMPA=%7.0f  peak dIN=%.2f mV\n", a, peak_din);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("RB soma: per-modifier currents at rest and clamped V", "[rb-balance]") {
    run_point(0, false);      // покой
    run_point(-55, true);
    run_point(-40, true);
    run_point(-23, true);     // где застревает пик
    run_point(0, true);       // спайк-плато
    CHECK(true);
}

// ── Скан Na-плотности: плотность решает пик или инактивация (V½_inact=−63.5) ──
static void set_nav12_density(BionData& d, float dens) {
    for (uint32_t i = 0; i < d.num_segments; i++)
        for (uint8_t j = 0; j < d.segments[i].modifier_count; j++)
            if (d.segments[i].modifiers[j].type == MOD_NAV12DEV)
                d.segments[i].modifiers[j].density = dens;
}

static float rb_stim_peak(BionData& d, uint64_t& tc) {
    CurrentStimulus stim = {0, 800.0f, 50.0f, tc};
    uint32_t sc = 1;
    float peak = -999.0f;
    for (int t = 0; t < 1000; t++) {
        tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
        if (d.segments[0].V_m > peak) peak = d.segments[0].V_m;
    }
    return peak;
}

TEST_CASE("RB Na-density scan: peak vs density", "[rb-na-scan]") {
    float dens[] = {3.62f, 10.0f, 36.0f, 100.0f, 300.0f, 500.0f};
    for (float d : dens) {
        cpg::CpgNetwork net;
        cpg::add_neuron(net, 0, cpg::Y_RB, 0, cpg::R_RB, cpg::V_RB,
                        cpg::rb_soma_mods(), cpg::rb_ais_mods(), cpg::rb_axon_mods());
        char path[256]; snprintf(path, sizeof(path), "/tmp/rb_nascan.bion");
        cpg::save_bion(net, path);
        BionData bd; memset(&bd, 0, sizeof(bd));
        bion_load_host(path, &bd);

        set_nav12_density(bd, d);
        uint64_t tc = 0;
        tick(bd, tc, 3000, nullptr, nullptr, nullptr, nullptr);
        float peak = rb_stim_peak(bd, tc);
        printf("[RB-NA] Na_dens=%7.1f  peak=%.2f mV\n", d, peak);

        bion_unload_host(&bd); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("RB->dIN: N stimulated RBs -> dIN peak", "[rb-n-scan]") {
    int counts[] = {1, 2, 5, 10};
    for (int n_rb : counts) {
        cpg::CpgNetwork net = cpg::build_cpg(2);
        char path[256]; snprintf(path, sizeof(path), "/tmp/rb_nscan.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

        int rb0 = cpg::rbL_idx(0, 0), dl0 = cpg::dinL_idx(0, 0);
        CurrentStimulus st[10]; uint32_t sc = 0;
        for (int n = 0; n < n_rb; n++)
            st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};
        float peak_din = -999.0f, peak_rb = -999.0f;
        for (int t = 0; t < 1000; t++) {
            tick(d, tc, 1, st, &sc, nullptr, nullptr);
            if (d.segments[dl0].V_m > peak_din) peak_din = d.segments[dl0].V_m;
            if (d.segments[rb0].V_m > peak_rb) peak_rb = d.segments[rb0].V_m;
        }
        printf("[RB-N] N_RB=%2d  peak dIN=%.2f  peak RB=%.2f mV\n", n_rb, peak_din, peak_rb);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("cIN: Na density scan -> spike from d->c", "[cin-na-scan]") {
    float na[] = {1.18f, 10.0f, 30.0f, 60.0f};
    for (float a : na) {
        cpg::CpgNetwork net = cpg::build_cpg(2);
        char path[256]; snprintf(path, sizeof(path), "/tmp/cin_nascan.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

        int cin0 = cpg::cinL_idx(0, 0), din0 = cpg::dinL_idx(0, 0);
        for (int part = 0; part < 4; part++)
            for (uint8_t j = 0; j < d.segments[cin0 + part].modifier_count; j++)
                if (d.segments[cin0 + part].modifiers[j].type == MOD_NAV12DEV)
                    d.segments[cin0 + part].modifiers[j].density = a;

        CurrentStimulus stim = {(uint32_t)din0, 800.0f, 50.0f, tc};
        uint32_t sc = 1;
        float peak_cin = -999.0f;
        for (int t = 0; t < 1000; t++) {
            tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
            if (d.segments[cin0].V_m > peak_cin) peak_cin = d.segments[cin0].V_m;
        }
        printf("[CIN-NA] CIN_NA_DENS=%6.2f  peak cIN=%.2f mV\n", a, peak_cin);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

static void set_mod_density(BionData& d, uint32_t seg, uint8_t type, float dens) {
    for (uint8_t j = 0; j < d.segments[seg].modifier_count; j++)
        if (d.segments[seg].modifiers[j].type == type)
            d.segments[seg].modifiers[j].density = dens;
}

TEST_CASE("dIN rebound: NaP x HCN1 scan (isolated dIN, -100pA -> rebound)", "[rebound-scan]") {
    float naps[] = {10.6f, 20.0f, 30.0f};
    float hcns[] = {120.0f, 240.0f, 360.0f};
    for (float nap : naps) for (float hcn : hcns) {
        cpg::CpgNetwork net;
        cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                        cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
        char path[256]; snprintf(path, sizeof(path), "/tmp/rebound.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 3000, nullptr, nullptr, nullptr, nullptr);

        set_mod_density(d, 0, MOD_NAP_TADPOLE, nap);      // NaP на соме (seg 0)
        set_mod_density(d, 0, MOD_HCN1, hcn);             // HCN1 на соме (униполярный dIN, seg 0)
        tick(d, tc, 1000, nullptr, nullptr, nullptr, nullptr);

        CurrentStimulus stim = {0, -100.0f, 50.0f, tc};
        uint32_t sc = 1;
        float peak = -999.0f, trough = 999.0f;
        for (int t = 0; t < 1500; t++) {
            tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
            if (d.segments[0].V_m > peak) peak = d.segments[0].V_m;
            if (d.segments[0].V_m < trough) trough = d.segments[0].V_m;
        }
        printf("[REBOUND] NaP=%5.1f HCN1=%5.1f  trough=%.2f peak=%.2f | Na_i=%.1f Na_o=%.1f K_i=%.1f K_o=%.1f Cl_i=%.1f Cl_o=%.1f\n",
               nap, hcn, trough, peak,
               d.segments[0].intra_ions[ION_NA], d.segments[0].extra_ions[ION_NA],
               d.segments[0].intra_ions[ION_K],  d.segments[0].extra_ions[ION_K],
               d.segments[0].intra_ions[ION_CL], d.segments[0].extra_ions[ION_CL]);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

static float get_state(BionData& d, uint32_t seg, uint8_t type, int idx) {
    for (uint8_t j = 0; j < d.segments[seg].modifier_count; j++)
        if (d.segments[seg].modifiers[j].type == type)
            return d.segments[seg].modifiers[j].state[idx];
    return -1.0f;
}

TEST_CASE("dIN rebound in network: trace V/h/Ih on dIN_R after RB->dIN_L", "[rebound-trace]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/reb_trace.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

    int dinL = cpg::dinL_idx(0, 0), dinR = cpg::dinR_idx(0, 0);
    int cinL = cpg::cinL_idx(0, 0), cinR = cpg::cinR_idx(0, 0);

    CurrentStimulus st[10]; uint32_t sc = 0;
    for (int n = 0; n < cpg::RB_PER_SIDE; n++)
        st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};

    for (int t = 0; t < 600; t++) {
        tick(d, tc, 1, st, &sc, nullptr, nullptr);
        if (t % 50 == 0) {
            float h_naR = get_state(d, dinR, MOD_NAV12DEV, 1);   // Nav h (де-инактивация)
            float m_ihR = get_state(d, dinR + 3, MOD_HCN1, 0);   // Ih m (дендрит)
            float h_naL = get_state(d, dinL, MOD_NAV12DEV, 1);
            printf("[REB-T] t=%3d  dL=%.2f dR=%.2f cL=%.2f cR=%.2f | hNa(dR)=%.3f hNa(dL)=%.3f Ih_m(dR)=%.3f\n",
                   t, d.segments[dinL].V_m, d.segments[dinR].V_m,
                   d.segments[cinL].V_m, d.segments[cinR].V_m, h_naR, h_naL, m_ihR);
        }
    }
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("d->d NMDA background: is dR(seg1) source firing / cleft glutamate?", "[dd-nmda-bg]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/dd_bg.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

    int dR0 = cpg::dinR_idx(0, 0), dR1 = cpg::dinR_idx(1, 0);
    int dL0 = cpg::dinL_idx(0, 0), dL1 = cpg::dinL_idx(1, 0);
    int cL0 = cpg::cinL_idx(0, 0), cL1 = cpg::cinL_idx(1, 0);

    // Найти d→d синапсы: dR1→dR0 и dL0→dL1 (ипсилатеральные, кросс-сегмент)
    int syn_dR = -1, syn_dL = -1;
    for (uint32_t s = 0; s < d.num_synapses; s++) {
        if (d.synapses[s].pre_segment == (uint32_t)cpg::axon_of(dR1) &&
            d.synapses[s].post_segment == (uint32_t)dR0) syn_dR = (int)s;
        if (d.synapses[s].pre_segment == (uint32_t)cpg::axon_of(dL0) &&
            d.synapses[s].post_segment == (uint32_t)dL1) syn_dL = (int)s;
    }
    printf("[DDBG] syn dR1->dR0=%d  dL0->dL1=%d\n", syn_dR, syn_dL);

    CurrentStimulus st[10]; uint32_t sc = 0;
    for (int n = 0; n < cpg::RB_PER_SIDE; n++)
        st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};

    for (int t = 0; t < 600; t++) {
        tick(d, tc, 1, st, &sc, nullptr, nullptr);
        if (t == 50 || t == 100 || t == 150 || t == 200 || t == 250 || t == 300 || t == 400) {
            float glu_dR = (syn_dR >= 0) ? d.synapses[syn_dR].cleft_neurotransmitter[NT_GLUTAMATE] : -1;
            float glu_dL = (syn_dL >= 0) ? d.synapses[syn_dL].cleft_neurotransmitter[NT_GLUTAMATE] : -1;
            float bca_dR = (syn_dR >= 0) ? d.synapses[syn_dR].bouton_Ca : -1;
            // Mg²⁺-блок NMDA на пост-сегменте dR0 (V_m) и dL1
            float mg_dR = 1.0f / (1.0f + (1.0f/3.57f) * expf(-0.062f * d.segments[dR0].V_m));
            float mg_dL = 1.0f / (1.0f + (1.0f/3.57f) * expf(-0.062f * d.segments[dL1].V_m));
            printf("[DDBG] t=%3d dL0=%.2f dL1=%.2f dR0=%.2f dR1=%.2f cL0=%.2f cL1=%.2f | dRcleft_glu=%.5f bCa=%.4f mg_dR0=%.3f | dLcleft_glu=%.5f mg_dL1=%.3f\n",
                   t, d.segments[dL0].V_m, d.segments[dL1].V_m, d.segments[dR0].V_m, d.segments[dR1].V_m,
                   d.segments[cL0].V_m, d.segments[cL1].V_m, glu_dR, bca_dR, mg_dR, glu_dL, mg_dL);
        }
    }
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("dR rebound vs glycine strength (cL->dR trough depth)", "[gly-scan]") {
    float glys[] = {80.0f, 160.0f, 320.0f, 640.0f};
    for (float g : glys) {
        cpg::CpgNetwork net = cpg::build_cpg(2);
        char path[256]; snprintf(path, sizeof(path), "/tmp/gly_scan.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

        int dR0 = cpg::dinR_idx(0, 0), cL0 = cpg::cinL_idx(0, 0);
        // cL->dR глициновые синапсы (pre = cL axon, post = dR soma)
        for (uint32_t s = 0; s < d.num_synapses; s++)
            if (d.synapses[s].pre_segment == (uint32_t)cpg::axon_of(cL0) &&
                d.synapses[s].post_segment == (uint32_t)dR0)
                for (int r = 0; r < d.synapses[s].post_receptor_count; r++)
                    if (d.synapses[s].post_receptors[r].type == MOD_NT_RECEPTOR_GLYCINE)
                        d.synapses[s].post_receptors[r].density = g;

        CurrentStimulus st[10]; uint32_t sc = 0;
        for (int n = 0; n < cpg::RB_PER_SIDE; n++)
            st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};

        float trough = 999, peak = -999; int spikes = 0, prev = 0;
        for (int t = 0; t < 600; t++) {
            tick(d, tc, 1, st, &sc, nullptr, nullptr);
            float v = d.segments[dR0].V_m;
            if (v < trough) trough = v;
            if (v > peak) peak = v;
            int above = (v > 0.0f) ? 1 : 0;
            if (above && !prev) spikes++;
            prev = above;
        }
        printf("[GLY] gly=%4.0f  dR trough=%.2f peak=%.2f spikes=%d\n", g, trough, peak, spikes);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("cL firing pattern: spike count + dR inhibition duration", "[cl-burst]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/cl_burst.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

    int dL0 = cpg::dinL_idx(0, 0), dR0 = cpg::dinR_idx(0, 0), cL0 = cpg::cinL_idx(0, 0);
    CurrentStimulus st[10]; uint32_t sc = 0;
    for (int n = 0; n < cpg::RB_PER_SIDE; n++)
        st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};

    int cl_spikes = 0, dl_spikes = 0; int prev_cl = 0, prev_dl = 0;
    int cl_above_thr = 0;  // тики, когда cL выше −30 (спайк)
    for (int t = 0; t < 600; t++) {
        tick(d, tc, 1, st, &sc, nullptr, nullptr);
        float vcl = d.segments[cL0].V_m, vdl = d.segments[dL0].V_m;
        int acl = (vcl > -30.0f) ? 1 : 0;   // порог cL спайка
        int adl = (vdl > 0.0f) ? 1 : 0;
        if (acl && !prev_cl) cl_spikes++;
        if (adl && !prev_dl) dl_spikes++;
        if (acl) cl_above_thr++;
        prev_cl = acl; prev_dl = adl;
        if (t >= 95 && t <= 160 && (t % 5 == 0))
            printf("[CLB] t=%3d dL=%.2f cL=%.2f dR=%.2f\n", t, vdl, vcl, d.segments[dR0].V_m);
    }
    printf("[CLB] dL spikes=%d  cL spikes=%d  cL ticks>=-30 = %d\n", dl_spikes, cl_spikes, cl_above_thr);
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("d->c excitation scan -> cL spikes + dR rebound", "[d2c-scan]") {
    float ampa[] = {1000.0f, 2000.0f, 4000.0f, 8000.0f};
    for (float a : ampa) {
        cpg::CpgNetwork net = cpg::build_cpg(2);
        char path[256]; snprintf(path, sizeof(path), "/tmp/d2c_scan.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

        int dL0 = cpg::dinL_idx(0, 0), cL0 = cpg::cinL_idx(0, 0), dR0 = cpg::dinR_idx(0, 0);
        // dL->cL AMPA+NMDA синапсы
        for (uint32_t s = 0; s < d.num_synapses; s++)
            if (d.synapses[s].pre_segment == (uint32_t)cpg::axon_of(dL0) &&
                d.synapses[s].post_segment == (uint32_t)cL0)
                for (int r = 0; r < d.synapses[s].post_receptor_count; r++)
                    if (d.synapses[s].post_receptors[r].type == MOD_NT_RECEPTOR_AMPA)
                        d.synapses[s].post_receptors[r].density = a;

        CurrentStimulus st[10]; uint32_t sc = 0;
        for (int n = 0; n < cpg::RB_PER_SIDE; n++)
            st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};

        int cl_spikes = 0, dr_spikes = 0; int prev_cl = 0, prev_dr = 0;
        float dr_trough = 999, dr_peak = -999, ih_max = -999;
        for (int t = 0; t < 600; t++) {
            tick(d, tc, 1, st, &sc, nullptr, nullptr);
            float vcl = d.segments[cL0].V_m, vdr = d.segments[dR0].V_m;
            int acl = (vcl > -30.0f) ? 1 : 0;
            int adr = (vdr > 0.0f) ? 1 : 0;
            if (acl && !prev_cl) cl_spikes++;
            if (adr && !prev_dr) dr_spikes++;
            prev_cl = acl; prev_dr = adr;
            if (vdr < dr_trough) dr_trough = vdr;
            if (vdr > dr_peak) dr_peak = vdr;
            float ih = get_state(d, dR0 + 3, MOD_HCN1, 0);
            if (ih > ih_max) ih_max = ih;
        }
        printf("[D2C] AMPA=%5.0f  cL spikes=%d  dR spikes=%d  dR trough=%.2f peak=%.2f Ih_m_max=%.3f\n",
               a, cl_spikes, dr_spikes, dr_trough, dr_peak, ih_max);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("dR rebound: V + Na/K/Cl ion dynamics trace", "[ion-dyn]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/ion_dyn.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

    int dR0 = cpg::dinR_idx(0, 0), dL0 = cpg::dinL_idx(0, 0);
    CurrentStimulus st[10]; uint32_t sc = 0;
    for (int n = 0; n < cpg::RB_PER_SIDE; n++)
        st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};

    for (int t = 0; t < 600; t++) {
        tick(d, tc, 1, st, &sc, nullptr, nullptr);
        if (t % 40 == 0) {
            Segment* s = &d.segments[dR0];
            float ih = get_state(d, dR0 + 3, MOD_HCN1, 0);
            float hna = get_state(d, dR0, MOD_NAV12DEV, 1);
            printf("[ION] t=%3d dL=%.2f dR=%.2f | Na_i=%.2f Na_o=%.2f K_i=%.2f K_o=%.2f Cl_i=%.2f Cl_o=%.2f | Ih_m=%.3f hNa=%.3f\n",
                   t, d.segments[dL0].V_m, s->V_m,
                   s->intra_ions[ION_NA], s->extra_ions[ION_NA],
                   s->intra_ions[ION_K], s->extra_ions[ION_K],
                   s->intra_ions[ION_CL], s->extra_ions[ION_CL], ih, hna);
        }
    }
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("Cl_o drift during settle (isolated network, no reservoir)", "[cl-drift]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/cl_drift.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    int dR0 = cpg::dinR_idx(0, 0);
    for (int t = 0; t <= 3000; t += 300) {
        if (t > 0) tick(d, tc, 300, nullptr, nullptr, nullptr, nullptr);
        Segment* s = &d.segments[dR0];
        float Ecl = -26.7f * logf(s->extra_ions[ION_CL] / s->intra_ions[ION_CL]);
        printf("[CLDR] t=%4d V=%.2f Cl_i=%.3f Cl_o=%.3f E_Cl=%.1f | Na_i=%.2f K_i=%.2f\n",
               t, s->V_m, s->intra_ions[ION_CL], s->extra_ions[ION_CL], Ecl,
               s->intra_ions[ION_NA], s->intra_ions[ION_K]);
    }
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("dR rebound with Cl_o held at 130 (reservoir) vs drifted", "[cl-hold]") {
    for (int mode = 0; mode < 2; mode++) {
        cpg::CpgNetwork net = cpg::build_cpg(2);
        char path[256]; snprintf(path, sizeof(path), "/tmp/cl_hold.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);
        int dR0 = cpg::dinR_idx(0, 0), dL0 = cpg::dinL_idx(0, 0);

        CurrentStimulus st[10]; uint32_t sc = 0;
        for (int n = 0; n < cpg::RB_PER_SIDE; n++)
            st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};

        int dr_spikes = 0, prev_dr = 0; float dr_peak = -999;
        for (int t = 0; t < 600; t++) {
            tick(d, tc, 1, st, &sc, nullptr, nullptr);
            if (mode == 1) d.segments[dR0].extra_ions[ION_CL] = 130.0f;  // держать Cl_o (резервуар)
            float v = d.segments[dR0].V_m;
            int above = (v > 0.0f) ? 1 : 0;
            if (above && !prev_dr) dr_spikes++;
            prev_dr = above;
            if (v > dr_peak) dr_peak = v;
        }
        float Ecl = -26.7f * logf(d.segments[dR0].extra_ions[ION_CL] / d.segments[dR0].intra_ions[ION_CL]);
        printf("[CLHOLD] mode=%d (%s)  dR spikes=%d peak=%.2f  E_Cl=%.1f\n",
               mode, mode==1?"Cl_o=130":"drifted", dr_spikes, dr_peak, Ecl);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("dR soma vs dendrite V during inhibition (Ih sees shallower hyperpolarization?)", "[dend-v]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/dend_v.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);
    int dR0 = cpg::dinR_idx(0, 0);
    CurrentStimulus st[10]; uint32_t sc = 0;
    for (int n = 0; n < cpg::RB_PER_SIDE; n++)
        st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};
    for (int t = 0; t < 400; t++) {
        tick(d, tc, 1, st, &sc, nullptr, nullptr);
        if (t % 50 == 0) {
            float v_soma = d.segments[dR0].V_m;
            float v_dend = d.segments[dR0 + 3].V_m;
            float ih = get_state(d, dR0 + 3, MOD_HCN1, 0);
            printf("[DENDV] t=%3d soma=%.2f dend=%.2f (delta %.2f) Ih_m=%.3f\n",
                   t, v_soma, v_dend, v_soma - v_dend, ih);
        }
    }
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("dIN rest: per-modifier current decomposition (soma + dend)", "[din-rest]") {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
    char path[256]; snprintf(path, sizeof(path), "/tmp/din_rest.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 3000, nullptr, nullptr, nullptr, nullptr);
    for (int part = 0; part < 4; part++) {
        Segment* s = &d.segments[part];
        float sum = 0.0f;
        printf("[DIN-REST] part=%d V=%.2f:", part, s->V_m);
        for (uint8_t j = 0; j < s->modifier_count; j++) {
            Modifier mm = s->modifiers[j];
            double I = dispatch_modifier_current(0, mm.type, &mm, s, 0.1f);
            sum += (float)I;
            printf("  t%d=%.3f", (int)mm.type, (float)I);
        }
        printf("  SUM=%.3f nA\n", sum);
    }
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("dIN K2P scan -> rest + rebound peak (reduce shunt)", "[k2p-scan]") {
    float k2ps[] = {4.17f, 3.0f, 2.0f, 1.0f, 0.5f};
    for (float k : k2ps) {
        cpg::CpgNetwork net = cpg::build_cpg(2);
        char path[256]; snprintf(path, sizeof(path), "/tmp/k2p_scan.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

        // K2P на соме dIN (обе стороны, все сегменты)
        for (int side = 0; side < 2; side++) {
            int base = (side == 0) ? cpg::dinL_idx(0, 0) : cpg::dinR_idx(0, 0);
            for (int seg = 0; seg < 2; seg++) {
                int s0 = (side == 0) ? cpg::dinL_idx(seg, 0) : cpg::dinR_idx(seg, 0);
                set_mod_density(d, s0, MOD_K2P_LEAK, k);
            }
        }

        int dR0 = cpg::dinR_idx(0, 0);
        CurrentStimulus st[10]; uint32_t sc = 0;
        for (int n = 0; n < cpg::RB_PER_SIDE; n++)
            st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};

        float trough = 999, peak = -999; int spikes = 0, prev = 0;
        for (int t = 0; t < 600; t++) {
            tick(d, tc, 1, st, &sc, nullptr, nullptr);
            float v = d.segments[dR0].V_m;
            if (v < trough) trough = v;
            if (v > peak) peak = v;
            int above = (v > 0.0f) ? 1 : 0;
            if (above && !prev) spikes++;
            prev = above;
        }
        printf("[K2P] K2P=%4.2f  dR rest=%.2f trough=%.2f peak=%.2f spikes=%d\n",
               k, d.segments[dR0].V_m, trough, peak, spikes);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("d->d synchronous firing: summed EPSP on target dIN", "[dd-sync]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/dd_sync.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

    int target = cpg::dinR_idx(0, 0);  // целевой dIN (seg0, сома)
    // Стимулировать все dR seg1 (пресинапс d→d для target, кросс-сегмент)
    CurrentStimulus st[32]; uint32_t sc = 0;
    for (int n = 0; n < cpg::N_DIN_PER_SIDE; n++)
        st[sc++] = {(uint32_t)cpg::dinR_idx(1, n), 800.0f, 5.0f, tc};

    float base = d.segments[target].V_m;
    float peak = base;
    for (int t = 0; t < 400; t++) {
        tick(d, tc, 1, st, &sc, nullptr, nullptr);
        float v = d.segments[target].V_m;
        if (v > peak) peak = v;
        if (t % 20 == 0)
            printf("[DD-TIME] t=%3d  V=%.2f  dV=%.2f\n", t, v, v - base);
    }
    printf("[DDSYNC] N=%d  target base=%.2f peak=%.2f  dEPSP=%.2f mV\n",
           cpg::N_DIN_PER_SIDE, base, peak, peak - base);
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("dR rebound: V soma/AIS + NaP m + Na m/h (gate dynamics)", "[reb-gates]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/reb_gates.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);
    int dR0 = cpg::dinR_idx(0, 0);
    CurrentStimulus st[10]; uint32_t sc = 0;
    for (int n = 0; n < cpg::RB_PER_SIDE; n++)
        st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};
    for (int t = 0; t < 600; t++) {
        tick(d, tc, 1, st, &sc, nullptr, nullptr);
        if (t % 50 == 0) {
            float vs = d.segments[dR0].V_m, va = d.segments[dR0 + 1].V_m;
            float nap_m = get_state(d, dR0, MOD_NAP_TADPOLE, 0);
            float na_m_soma = get_state(d, dR0, MOD_NAV12DEV, 0);
            float na_m_ais = get_state(d, dR0 + 1, MOD_NAV12DEV, 0);
            float na_h_ais = get_state(d, dR0 + 1, MOD_NAV12DEV, 1);
            float na_m_inf = 1.0f / (1.0f + expf((-16.0f - va) / 6.0f));
            printf("[RG] t=%3d soma=%.2f ais=%.2f | NaP_m=%.3f Na_m(soma)=%.3f Na_m(ais)=%.3f Na_h(ais)=%.3f | Na_m_inf(ais)=%.3f\n",
                   t, vs, va, nap_m, na_m_soma, na_m_ais, na_h_ais, na_m_inf);
        }
    }
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("dR soma vs AIS V during rebound (does AIS spike?)", "[ais-rebound]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/ais_reb.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);
    int dR0 = cpg::dinR_idx(0, 0);
    CurrentStimulus st[10]; uint32_t sc = 0;
    for (int n = 0; n < cpg::RB_PER_SIDE; n++)
        st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};
    int soma_spikes = 0, ais_spikes = 0; int ps = 0, pa = 0;
    for (int t = 0; t < 600; t++) {
        tick(d, tc, 1, st, &sc, nullptr, nullptr);
        float vs = d.segments[dR0].V_m, va = d.segments[dR0 + 1].V_m;
        int as = (vs > 0.0f) ? 1 : 0, aa = (va > 0.0f) ? 1 : 0;
        if (as && !ps) soma_spikes++;
        if (aa && !pa) ais_spikes++;
        ps = as; pa = aa;
        if (t % 50 == 0)
            printf("[AIS-RB] t=%3d soma=%.2f ais=%.2f\n", t, vs, va);
    }
    printf("[AIS-RB] soma spikes=%d ais spikes=%d\n", soma_spikes, ais_spikes);
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("NaP voltage-clamp: current at -40/-10/0 mV", "[nap-clamp]") {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
    char path[256]; snprintf(path, sizeof(path), "/tmp/nap_clamp.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 3000, nullptr, nullptr, nullptr, nullptr);
    for (float v : {-40.0f, -10.0f, 0.0f}) {
        Segment* s = &d.segments[0];
        s->V_m = v;
        // релаксировать m-гейт NaP к m_inf(v) (прогон 100 тиков по 0.1мс)
        for (int k = 0; k < 100; k++)
            for (uint8_t j = 0; j < s->modifier_count; j++)
                if (s->modifiers[j].type == MOD_NAP_TADPOLE)
                    dispatch_modifier_current(0, MOD_NAP_TADPOLE, &s->modifiers[j], s, 0.1f);
        float inap = 0.0f;
        for (uint8_t j = 0; j < s->modifier_count; j++)
            if (s->modifiers[j].type == MOD_NAP_TADPOLE)
                inap = (float)dispatch_modifier_current(0, MOD_NAP_TADPOLE, &s->modifiers[j], s, 0.1f);
        float area = 4.0f * 3.14159f * cpg::R_DIN * cpg::R_DIN;  // сома ~4πr²
        printf("[NAPC] V=%.0f mV  NaP=%.3f pA  (area=%.0f µm²)  = %.4f pA/µm²\n",
               v, inap, area, inap / area);
    }
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

TEST_CASE("dIN NaP scan -> dL saturation (plateau)", "[nap-scan]") {
    float naps[] = {0.0f, 3.0f, 10.6f};
    for (float nap : naps) {
        cpg::CpgNetwork net = cpg::build_cpg(2);
        char path[256]; snprintf(path, sizeof(path), "/tmp/nap_scan.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

        set_mod_density(d, cpg::dinL_idx(0, 0), MOD_NAP_TADPOLE, nap);
        set_mod_density(d, cpg::dinR_idx(0, 0), MOD_NAP_TADPOLE, nap);
        tick(d, tc, 1000, nullptr, nullptr, nullptr, nullptr);

        int dl0 = cpg::dinL_idx(0, 0);
        CurrentStimulus st[10]; uint32_t sc = 0;
        for (int n = 0; n < cpg::RB_PER_SIDE; n++)
            st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};

        float peak = -999.0f;
        for (int t = 0; t < 600; t++) {
            tick(d, tc, 1, st, &sc, nullptr, nullptr);
            if (d.segments[dl0].V_m > peak) peak = d.segments[dl0].V_m;
        }
        printf("[NAP] NaP=%5.1f  dL peak=%.2f  dL@t=600=%.2f mV\n",
               nap, peak, d.segments[dl0].V_m);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("dIN isolated: single-spike test (short suprathreshold stim)", "[din-single]") {
    float amps[] = {100.0f, 200.0f, 400.0f};
    float naps[] = {0.0f, 10.6f};
    for (float amp : amps) for (float nap : naps) {
        cpg::CpgNetwork net;
        cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                        cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
        char path[256]; snprintf(path, sizeof(path), "/tmp/din_single.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 3000, nullptr, nullptr, nullptr, nullptr);
        set_mod_density(d, 0, MOD_NAP_TADPOLE, nap);

        CurrentStimulus stim = {0, amp, 2.0f, tc};  // короткий надпороговый (2 мс)
        uint32_t sc = 1;
        float peak = -999; int spikes = 0; int prev_above = 0;
        for (int t = 0; t < 300; t++) {
            tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
            float v = d.segments[0].V_m;
            if (v > peak) peak = v;
            int above = (v > 0.0f) ? 1 : 0;
            if (above && !prev_above) spikes++;
            prev_above = above;
            if (t == 200) printf("[DIN-SINGLE] amp=%5.0f NaP=%5.1f  peak=%.2f spikes=%d V@200=%.2f mV\n",
                                 amp, nap, peak, spikes, v);
        }
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("dIN: Kv2.1 density scan -> single-spike", "[kv21-scan]") {
    float kv21s[] = {2.595f, 10.0f, 20.0f, 40.0f};
    for (float kv21 : kv21s) {
        cpg::CpgNetwork net;
        cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                        cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
        char path[256]; snprintf(path, sizeof(path), "/tmp/kv21.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 3000, nullptr, nullptr, nullptr, nullptr);
        set_mod_density(d, 0, MOD_KV21, kv21);

        CurrentStimulus stim = {0, 200.0f, 2.0f, tc};
        uint32_t sc = 1;
        float peak = -999; int spikes = 0; int prev_above = 0;
        for (int t = 0; t < 300; t++) {
            tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
            float v = d.segments[0].V_m;
            if (v > peak) peak = v;
            int above = (v > 0.0f) ? 1 : 0;
            if (above && !prev_above) spikes++;
            prev_above = above;
        }
        printf("[KV21] dens=%5.1f  peak=%.2f  spikes=%d  V@300=%.2f mV\n", kv21, peak, spikes, d.segments[0].V_m);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("dIN: Kv2.1 dynamics on spike (does I_Ks activate?)", "[kv21-dyn]") {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
    char path[256]; snprintf(path, sizeof(path), "/tmp/kv21_dyn.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 3000, nullptr, nullptr, nullptr, nullptr);

    CurrentStimulus stim = {0, 200.0f, 2.0f, tc};
    uint32_t sc = 1;
    for (int t = 0; t < 60; t++) {
        tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
        if (t == 1 || t == 2 || t == 3 || t == 5 || t == 10 || t == 20 || t == 40) {
            float kv21_m = get_state(d, 0, MOD_KV21, 0);   // m (активация)
            float kv21_h = get_state(d, 0, MOD_KV21, 1);   // h (U-type)
            float kv11_m = get_state(d, 0, MOD_KV11, 0);   // для сравнения Kv1.1
            printf("[KV21-DYN] t=%2d  V=%.2f  Kv21_m=%.4f Kv21_h=%.4f  Kv11_m=%.4f\n",
                   t, d.segments[0].V_m, kv21_m, kv21_h, kv11_m);
        }
    }
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}

static void set_state(BionData& d, uint32_t seg, uint8_t type, int idx, float val) {
    for (uint8_t j = 0; j < d.segments[seg].modifier_count; j++)
        if (d.segments[seg].modifiers[j].type == type)
            d.segments[seg].modifiers[j].state[idx] = val;
}

TEST_CASE("dIN: slow recovery frac scan -> single-spike", "[slow-scan]") {
    float fracs[] = {0.3f, 0.5f, 0.7f, 0.9f};
    for (float frac : fracs) {
        cpg::CpgNetwork net;
        cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                        cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
        char path[256]; snprintf(path, sizeof(path), "/tmp/slow.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 3000, nullptr, nullptr, nullptr, nullptr);
        for (int part = 0; part < 4; part++)
            set_state(d, part, MOD_NAV12DEV, 4, frac);  // доля slow recovery (0..1)

        CurrentStimulus stim = {0, 200.0f, 2.0f, tc};
        uint32_t sc = 1;
        float peak = -999; int spikes = 0; int prev_above = 0;
        for (int t = 0; t < 300; t++) {
            tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
            float v = d.segments[0].V_m;
            if (v > peak) peak = v;
            int above = (v > 0.0f) ? 1 : 0;
            if (above && !prev_above) spikes++;
            prev_above = above;
        }
        printf("[SLOW] frac=%.1f  peak=%.2f  spikes=%d  V@300=%.2f mV\n",
               frac, peak, spikes, d.segments[0].V_m);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("dIN: V½_inact × slow-доля scan -> single-spike", "[vhalf-slow-scan]") {
    float vhalfs[] = {45.0f, 50.0f, 52.0f, 55.0f};
    float fracs[] = {0.3f, 0.5f, 0.7f};
    for (float vh : vhalfs) for (float frac : fracs) {
        cpg::CpgNetwork net;
        cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                        cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
        char path[256]; snprintf(path, sizeof(path), "/tmp/vhs.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 3000, nullptr, nullptr, nullptr, nullptr);
        for (int part = 0; part < 4; part++) {
            set_state(d, part, MOD_NAV12DEV, 2, vh);    // V½_инакт (мВ)
            set_state(d, part, MOD_NAV12DEV, 4, frac);  // slow-доля
        }

        CurrentStimulus stim = {0, 400.0f, 2.0f, tc};
        uint32_t sc = 1;
        float peak = -999; int spikes = 0; int prev_above = 0;
        for (int t = 0; t < 300; t++) {
            tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
            float v = d.segments[0].V_m;
            if (v > peak) peak = v;
            int above = (v > 0.0f) ? 1 : 0;
            if (above && !prev_above) spikes++;
            prev_above = above;
        }
        printf("[VHS] V½=%.0f frac=%.1f  peak=%.2f  spikes=%d  V@300=%.2f mV\n",
               vh, frac, peak, spikes, d.segments[0].V_m);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("RB->dIN: V½_inact scan -> dIN peak (баланс кик vs single-spike)", "[rb-vhalf-scan]") {
    float vhalfs[] = {40.0f, 45.0f, 50.0f, 52.0f};
    for (float vh : vhalfs) {
        cpg::CpgNetwork net = cpg::build_cpg(2);
        char path[256]; snprintf(path, sizeof(path), "/tmp/rbvh.bion");
        cpg::save_bion(net, path);
        BionData d; memset(&d, 0, sizeof(d));
        bion_load_host(path, &d);
        uint64_t tc = 0;
        tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);
        for (int side = 0; side < 2; side++) {
            int base = (side == 0) ? cpg::dinL_idx(0, 0) : cpg::dinR_idx(0, 0);
            for (int part = 0; part < 4; part++)
                set_state(d, base + part, MOD_NAV12DEV, 2, vh);
        }

        int dl0 = cpg::dinL_idx(0, 0);
        CurrentStimulus st[5]; uint32_t sc = 0;
        for (int n = 0; n < 5; n++)
            st[sc++] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};
        float peak = -999.0f;
        for (int t = 0; t < 1000; t++) {
            tick(d, tc, 1, st, &sc, nullptr, nullptr);
            if (d.segments[dl0].V_m > peak) peak = d.segments[dl0].V_m;
        }
        printf("[RBVH] V½_inact=%.0f  peak dIN (5 RB)=%.2f mV\n", vh, peak);
        bion_unload_host(&d); std::remove(path);
    }
    CHECK(true);
}

TEST_CASE("d->c: release trace (does dIN axon conduct + release glutamate)", "[dc-trace]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/dc_trace.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);
    uint64_t tc = 0;
    tick(d, tc, 2000, nullptr, nullptr, nullptr, nullptr);

    int din0 = cpg::dinL_idx(0, 0), cin0 = cpg::cinL_idx(0, 0), din_axon = din0 + 2;
    int syn_idx = -1;
    for (uint32_t s = 0; s < d.num_synapses; s++)
        if (d.synapses[s].pre_segment == (uint32_t)din_axon &&
            d.synapses[s].post_segment == (uint32_t)cin0) { syn_idx = (int)s; break; }
    printf("[DC] d->c synapse idx=%d (pre=axon %d, post=cIN %d)\n", syn_idx, din_axon, cin0);

    CurrentStimulus stim = {(uint32_t)din0, 800.0f, 50.0f, tc};
    uint32_t sc = 1;
    for (int t = 0; t < 200; t++) {
        tick(d, tc, 1, &stim, &sc, nullptr, nullptr);
        if (t == 1 || t == 3 || t == 5 || t == 10 || t == 20 || t == 50 || t == 100) {
            float glu = (syn_idx >= 0) ? d.synapses[syn_idx].cleft_neurotransmitter[NT_GLUTAMATE] : -1.0f;
            float bca = (syn_idx >= 0) ? d.synapses[syn_idx].bouton_Ca : -1.0f;
            float rrp = (syn_idx >= 0) ? d.synapses[syn_idx].RRP : -1.0f;
            printf("[DC] t=%3d  V_dinSoma=%.2f V_dinAxon=%.2f  boutonCa=%.4f cleft_glu=%.4f RRP=%.1f  cIN=%.2f\n",
                   t, d.segments[din0].V_m, d.segments[din_axon].V_m, bca, glu, rrp, d.segments[cin0].V_m);
        }
    }
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}
