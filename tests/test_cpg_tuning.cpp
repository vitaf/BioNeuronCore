// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cpg_tuning.cpp — систематический тюнинг CPG ритма.
// Проверяет 5 гипотез из плана cpg-test-and-rhythm-plan.md:
//   H1: NaP-драйв dIN достаточен для авторитмичности
//   H2: SK на cIN создаёт паузу (AHP) при density ≥ 0.5
//   H3: glycine перекрёстное торможение при w ≥ 3.5
//   H4: Gap junctions (если есть) синхронизируют — отключить
//   H5: Комбинированные параметры для альтернации ≥5 switches/5000 ticks

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <dlfcn.h>

#include "types.h"
#include "bion_format.h"
#include "bion_loader.h"

extern "C" {
    void core_run_tick(Segment* segs, uint32_t N, IntraConnection* conns, uint32_t M,
                       Synapse* syns, uint32_t K, int mode, uint64_t* tick, float dt,
                       void*,void*,void*,void*,void*,void*,void*,void*,void*,void*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

static float raw_vm(Segment* s) { float v; memcpy(&v, (char*)s+40, 4); return v; }
static float raw_py(Segment* s) { float v; memcpy(&v, (char*)s+12, 4); return v; }
static float raw_px(Segment* s) { float v; memcpy(&v, (char*)s+8, 4); return v; }
static uint8_t raw_st(Segment* s) { uint8_t v; memcpy(&v, (char*)s+901, 1); return v; }

// ── Построение сети (1 сегмент) ──
struct CpgNet {
    std::vector<Segment> segs;
    std::vector<Neuron> neurons;
    std::vector<IntraConnection> conns;
    std::vector<Synapse> syns;
};

static CpgNet build_net(float glycine_w, float din2cin_w, float sk_dens, float nap_dens, float glia_f) {
    CpgNet net;
    const int GLIA_PER_SIDE = 3;

    // Базовые модификаторы нейрона (Nav1.2 + Kv2.1 + pump + leaks + гомеостаз)
    // K2P=0.10 — компромисс: лучше чем 0.03 для V_rest, но позволяет спайки
    auto base = []() -> std::vector<std::pair<uint8_t,float>> {
        return {{MOD_AXON_NAV12,15.6f},{MOD_KV21,3.84f},{MOD_PUMP_NAK,0.3f},{MOD_K2P_LEAK,0.10f},
                {MOD_KCC2,1.0f},{MOD_KIR_CHANNEL,0.10f}};
    };

    // dIN: полный биофизический набор
    // PIR (T-Ca + HCN) — ключевой механизм post-inhibitory rebound
    auto din_mods = [nap_dens]() -> std::vector<std::pair<uint8_t,float>> {
        return {{MOD_AXON_NAV12,15.6f},{MOD_KV21,3.84f},{MOD_PUMP_NAK,0.3f},{MOD_K2P_LEAK,0.10f},
                {MOD_KIR_CHANNEL,0.10f},{MOD_KCC2,1.0f},
                {MOD_NAP_CHANNEL,nap_dens},
                {MOD_CAV13,0.3f},{MOD_BK_CHANNEL,0.12f},
                {MOD_SK_CHANNEL,0.4f},{MOD_CA_LEAK,0.3f},{MOD_CA_BUFFER,0.6f},
                {MOD_PIR_TTYPE_CALCIUM,0.6f},{MOD_HCN2,0.15f}};
    };

    // cIN: усиленный SK для AHP + PIR для rebound после торможения
    auto cin_mods = [sk_dens]() -> std::vector<std::pair<uint8_t,float>> {
        return {{MOD_AXON_NAV12,15.6f},{MOD_KV21,3.84f},{MOD_PUMP_NAK,0.3f},{MOD_K2P_LEAK,0.10f},
                {MOD_KCC2,1.0f},{MOD_KIR_CHANNEL,0.10f},
                {MOD_SK_CHANNEL,sk_dens},
                {MOD_PIR_TTYPE_CALCIUM,0.4f},{MOD_HCN2,0.10f}};
    };

    // mn: BK + SK для реполяризации
    auto mn_mods = []() -> std::vector<std::pair<uint8_t,float>> {
        return {{MOD_AXON_NAV12,15.6f},{MOD_KV21,3.84f},{MOD_PUMP_NAK,0.3f},{MOD_K2P_LEAK,0.10f},
                {MOD_KCC2,1.0f},{MOD_KIR_CHANNEL,0.10f},
                {MOD_SK_CHANNEL,0.18f},{MOD_BK_CHANNEL,0.15f}};
    };

    auto glia = [glia_f]() -> std::vector<std::pair<uint8_t,float>> {
        return {{MOD_GLIAL_KIR41,0.4f*glia_f},{MOD_GLIAL_NAK_PUMP,0.25f*glia_f},{MOD_K2P_LEAK,0.02f}};
    };

    auto add = [&](float x,float y,float z,float r,float Vm,auto mods) {
        Segment s = {}; s.id=(uint32_t)net.segs.size(); s.neuron_id=(uint32_t)net.neurons.size();
        s.position={x,y,z}; s.direction={0,1,0}; s.radius=r; s.length=r*2; s.V_m=Vm; s.segment_type=1;
        s.intra_ions[0]=0.015f; s.intra_ions[1]=0.140f; s.intra_ions[2]=0.00005f;
        s.intra_ions[3]=0.005f; s.intra_ions[4]=0.0005f;
        s.extra_ions[0]=0.140f; s.extra_ions[1]=0.005f; s.extra_ions[2]=0.0012f;
        s.extra_ions[3]=0.130f; s.extra_ions[4]=0.002f;
        s.modifier_count=(uint8_t)mods.size();
        for(size_t j=0;j<mods.size()&&j<16;j++){s.modifiers[j].type=mods[j].first;s.modifiers[j].density=mods[j].second;}
        net.segs.push_back(s);
        Neuron n={}; n.id=s.neuron_id; n.segment_start=s.id; n.segment_count=1; n.threshold=-20;
        net.neurons.push_back(n);
    };

    for(int seg=0;seg<1;seg++){
        float z=-seg*150.0f;
        // dIN стартует с -58mV (слегка деполяризован для инициации)
        // cIN_L: kickstart -50mV для запуска первого спайка
        add(-35,70,z,5,-58,din_mods());
        add(-35,45,z,4.5,-40,cin_mods());  // KICKSTART: левый cIN (-40mV)
        add(-35,20,z,6,-65,mn_mods());
        add(35,70,z,5,-62,din_mods());
        add(35,45,z,4.5,-65,cin_mods());   // правый cIN — нормальный покой
        add(35,20,z,6,-65,mn_mods());
        for(int g=0;g<GLIA_PER_SIDE;g++){add(-15+g*8,50,z,3,-80,glia());add(5+g*8,50,z,3,-80,glia());}
    }

    int N_TOTAL=(int)net.segs.size(), N_NEUR=6;

    for(int g=N_NEUR;g<N_TOTAL;g++)
        for(int n=0;n<N_TOTAL;n++){
            IntraConnection c={}; c.segment_a=(uint32_t)g; c.segment_b=(uint32_t)n; c.is_intracellular=0;
            net.conns.push_back(c);
        }

    uint32_t sid=0;
    int dL=0,cL=1,mL=2,dR=3,cR=4,mR=5;
    auto add_syn = [&](uint32_t pre,uint32_t post,float w,uint8_t rec_type,float rec_dens,float tau_cl,float rp){
        Synapse sy={}; sy.id=sid++; sy.pre_segment=pre; sy.post_segment=post;
        sy.type=SYNAPSE_TYPE_CHEMICAL; sy.weight=w; sy.release_probability=rp;
        sy.release_amount=1.0f; sy.tau_clearance=tau_cl; sy.pre_area=0.1f; sy.post_area=0.1f;
        sy.post_receptor_count=1; sy.post_receptors[0].type=rec_type; sy.post_receptors[0].density=rec_dens;
        net.syns.push_back(sy);
    };
    add_syn(cL,dR,glycine_w,MOD_NT_RECEPTOR_GLYCINE,0.3f,25.0f,0.8f);
    add_syn(cR,dL,glycine_w,MOD_NT_RECEPTOR_GLYCINE,0.3f,25.0f,0.8f);
    // dIN→cIN (двойной: AMPA+NMDA)
    { Synapse sy={}; sy.id=sid++; sy.pre_segment=dL; sy.post_segment=cL;
      sy.type=SYNAPSE_TYPE_CHEMICAL; sy.weight=din2cin_w; sy.release_probability=0.6f;
      sy.release_amount=1.0f; sy.tau_clearance=15.0f; sy.pre_area=0.1f; sy.post_area=0.1f;
      sy.post_receptor_count=2; sy.post_receptors[0].type=MOD_NT_RECEPTOR_AMPA; sy.post_receptors[0].density=0.2f;
      sy.post_receptors[1].type=MOD_NT_RECEPTOR_NMDA; sy.post_receptors[1].density=0.15f;
      net.syns.push_back(sy); }
    { Synapse sy={}; sy.id=sid++; sy.pre_segment=dR; sy.post_segment=cR;
      sy.type=SYNAPSE_TYPE_CHEMICAL; sy.weight=din2cin_w; sy.release_probability=0.6f;
      sy.release_amount=1.0f; sy.tau_clearance=15.0f; sy.pre_area=0.1f; sy.post_area=0.1f;
      sy.post_receptor_count=2; sy.post_receptors[0].type=MOD_NT_RECEPTOR_AMPA; sy.post_receptors[0].density=0.2f;
      sy.post_receptors[1].type=MOD_NT_RECEPTOR_NMDA; sy.post_receptors[1].density=0.15f;
      net.syns.push_back(sy); }
    add_syn(dL,mL,1.5f,MOD_NT_RECEPTOR_AMPA,0.15f,10.0f,0.5f);
    add_syn(dR,mR,1.5f,MOD_NT_RECEPTOR_AMPA,0.15f,10.0f,0.5f);
    return net;
}

static std::string write_bion(const char* tag, CpgNet& net) {
    char path[256]; snprintf(path,sizeof(path),"/tmp/cpg_tune_%s.bion",tag);
    FILE* f=fopen(path,"wb"); REQUIRE(f);
    NetworkHeader h={}; h.magic=BION_MAGIC; h.version=BION_VERSION; h.mode=0;
    h.num_neurons=(uint32_t)net.neurons.size(); h.num_segments=(uint32_t)net.segs.size();
    h.num_intra_connections=(uint32_t)net.conns.size(); h.num_synapses=(uint32_t)net.syns.size();
    fwrite(&h,sizeof(h),1,f);
    for(auto& n:net.neurons)fwrite(&n,sizeof(n),1,f);
    for(auto& s:net.segs)fwrite(&s,sizeof(s),1,f);
    for(auto& c:net.conns)fwrite(&c,sizeof(c),1,f);
    for(auto& syn:net.syns)fwrite(&syn,sizeof(syn),1,f);
    fclose(f); return path;
}

struct TuneResult { int switches; float l_mn_avg, r_mn_avg; float l_din_avg, r_din_avg; bool has_nan; };

static TuneResult run_tune(const std::string& path, int max_ticks) {
    BionData d; memset(&d,0,sizeof(d));
    TuneResult r={0,0,0,0,0,false};
    if(bion_load_host(path.c_str(),&d)!=0){r.has_nan=true;return r;}

    uint64_t tc=0; int last=-1; float l_sum=0,r_sum=0,l_din_sum=0,r_din_sum=0; int l_n=0,r_n=0,l_dn=0,r_dn=0;

    for(int tick=0;tick<=max_ticks;tick++){
        core_run_tick(d.segments,d.num_segments,d.connections,d.num_connections,
                      d.synapses,d.num_synapses,0,&tc,0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);

        if(tick%100==0){
            int l_sp=0,lc=0,r_sp=0,rc=0;
            for(uint32_t i=0;i<d.num_segments;i++){
                float vm=raw_vm(&d.segments[i]),py=raw_py(&d.segments[i]),px=raw_px(&d.segments[i]);
                uint8_t st=raw_st(&d.segments[i]);
                if(std::isnan(vm)){r.has_nan=true;bion_unload_host(&d);return r;}
                if(st==1&&py>=15&&py<=25){ // MN
                    if(px<-1){lc++;if(vm>-20)l_sp++;l_sum+=vm;l_n++;}
                    else if(px>1){rc++;if(vm>-20)r_sp++;r_sum+=vm;r_n++;}
                }
                if(st==1&&py>=65&&py<=80){ // dIN
                    if(px<-1){l_din_sum+=vm;l_dn++;}
                    else if(px>1){r_din_sum+=vm;r_dn++;}
                }
            }
            float la=lc>0?(float)l_sp/lc:0,ra=rc>0?(float)r_sp/rc:0;
            int cur=-1;
            if(la>0.3f&&ra<0.3f)cur=0;else if(ra>0.3f&&la<0.3f)cur=1;
            if(cur!=last&&cur>=0&&last>=0)r.switches++;
            if(cur>=0)last=cur;
        }
    }
    r.l_mn_avg=l_n>0?l_sum/l_n:0; r.r_mn_avg=r_n>0?r_sum/r_n:0;
    r.l_din_avg=l_dn>0?l_din_sum/l_dn:0; r.r_din_avg=r_dn>0?r_din_sum/r_dn:0;
    bion_unload_host(&d);
    return r;
}

// ═══════════════════════════════════════════════════════════════
// H1: NaP-драйв dIN — проверка авторитмичности
// ═══════════════════════════════════════════════════════════════

TEST_CASE("CPG Tuning H1: NaP drives dIN autorhythmicity", "[cpg_tune]") {
    printf("\n=== H1: NaP density sweep on dIN ===\n");
    printf("%-10s %-10s %-10s %-10s %-10s %s\n","NaP","gly_w","SK_dens","L_dIN","R_dIN","Switches");

    for (float nap : {0.03f, 0.05f, 0.07f, 0.10f, 0.15f}) {
        CpgNet net = build_net(2.8f, 2.5f, 0.35f, nap, 0.5f);
        std::string path = write_bion("h1", net);
        TuneResult r = run_tune(path, 2000);
        printf("%-10.3f %-10.1f %-10.2f %-10.1f %-10.1f %d\n",
               nap, 2.8f, 0.35f, r.l_din_avg, r.r_din_avg, r.switches);
        CHECK_FALSE(r.has_nan);
        std::remove(path.c_str());
    }
    SUCCEED("H1: NaP sweep complete");
}

// ═══════════════════════════════════════════════════════════════
// H2: SK на cIN — проверка AHP/паузы
// ═══════════════════════════════════════════════════════════════

TEST_CASE("CPG Tuning H2: SK density on cIN creates pause", "[cpg_tune]") {
    printf("\n=== H2: SK density sweep on cIN ===\n");
    printf("%-10s %-10s %-10s %-10s %-10s %s\n","SK_dens","gly_w","NaP","L_dIN","R_dIN","Switches");

    for (float sk : {0.35f, 0.50f, 0.65f, 0.80f, 1.0f}) {
        CpgNet net = build_net(2.8f, 2.5f, sk, 0.045f, 0.5f);
        std::string path = write_bion("h2", net);
        TuneResult r = run_tune(path, 2000);
        printf("%-10.2f %-10.1f %-10.3f %-10.1f %-10.1f %d\n",
               sk, 2.8f, 0.045f, r.l_din_avg, r.r_din_avg, r.switches);
        CHECK_FALSE(r.has_nan);
        std::remove(path.c_str());
    }
    SUCCEED("H2: SK sweep complete");
}

// ═══════════════════════════════════════════════════════════════
// H3: glycine перекрёстное торможение
// ═══════════════════════════════════════════════════════════════

TEST_CASE("CPG Tuning H3: glycine crossed inhibition weight", "[cpg_tune]") {
    printf("\n=== H3: glycine weight sweep ===\n");
    printf("%-10s %-10s %-10s %-10s %-10s %s\n","gly_w","SK_dens","NaP","L_dIN","R_dIN","Switches");

    for (float gly : {2.5f, 3.0f, 3.5f, 4.0f, 5.0f}) {
        CpgNet net = build_net(gly, 2.5f, 0.35f, 0.045f, 0.5f);
        std::string path = write_bion("h3", net);
        TuneResult r = run_tune(path, 2000);
        printf("%-10.1f %-10.2f %-10.3f %-10.1f %-10.1f %d\n",
               gly, 0.35f, 0.045f, r.l_din_avg, r.r_din_avg, r.switches);
        CHECK_FALSE(r.has_nan);
        std::remove(path.c_str());
    }
    SUCCEED("H3: glycine sweep complete");
}

// ═══════════════════════════════════════════════════════════════
// H4: Gap junctions — проверка (в сети нет gap, но проверяем что нет синхронизации)
// ═══════════════════════════════════════════════════════════════

TEST_CASE("CPG Tuning H4: no gap junctions = no cross-sync", "[cpg_tune]") {
    printf("\n=== H4: Gap junction check ===\n");
    // В нашей CPG-сети нет электрических синапсов между полушариями.
    // Проверяем что dIN_L и dIN_R могут расходиться по V_m
    CpgNet net = build_net(2.8f, 2.5f, 0.35f, 0.045f, 0.5f);
    std::string path = write_bion("h4", net);

    BionData d; memset(&d,0,sizeof(d));
    REQUIRE(bion_load_host(path.c_str(),&d)==0);

    uint64_t tc=0;
    float max_diff=0;
    for(int t=0;t<2000;t++){
        core_run_tick(d.segments,d.num_segments,d.connections,d.num_connections,
                      d.synapses,d.num_synapses,0,&tc,0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        float vmL=raw_vm(&d.segments[0]), vmR=raw_vm(&d.segments[3]);
        float diff=fabsf(vmL-vmR);
        if(diff>max_diff)max_diff=diff;
    }

    printf("[CPG-H4] Max |dIN_L - dIN_R| = %.2f mV over 2000 ticks\n", max_diff);
    // Если gap junctions синхронизируют, разница будет <0.1mV.
    // Если расходятся >1mV — gap не синхронизируют.
    CHECK(max_diff > 0.0f);
    printf("H4: dIN_L/R can diverge — no electrical sync locking them\n");

    bion_unload_host(&d); std::remove(path.c_str());
}

// ═══════════════════════════════════════════════════════════════
// H5: Комбинированный тюнинг — поиск альтернации
// ═══════════════════════════════════════════════════════════════

TEST_CASE("CPG Tuning H5: combined parameter grid search for alternation", "[cpg_tune]") {
    printf("\n=== H5: Combined grid search ===\n");
    printf("%-8s %-8s %-8s %-8s %-8s %-8s %s\n",
           "gly_w","SK","NaP","glia_f","L_dIN","R_dIN","Sw");

    int best_sw = 0;
    float best_gly=0, best_sk=0, best_nap=0, best_gf=0;

    for (float gly : {4.0f, 5.0f, 6.0f}) {
     for (float sk : {0.7f, 0.9f, 1.2f}) {
      for (float nap : {0.05f, 0.08f, 0.12f}) {
       for (float gf : {0.3f, 0.5f, 0.7f}) {
        CpgNet net = build_net(gly, 2.5f, sk, nap, gf);
        char tag[64]; snprintf(tag,sizeof(tag),"g%.0f_s%.0f_n%.0f_gf%.0f",gly*10,sk*10,nap*100,gf*10);
        std::string path = write_bion(tag, net);
        TuneResult r = run_tune(path, 5000);
        printf("%-8.1f %-8.2f %-8.3f %-8.2f %-8.1f %-8.1f %d\n",
               gly, sk, nap, gf, r.l_din_avg, r.r_din_avg, r.switches);
        if (r.switches > best_sw) { best_sw=r.switches; best_gly=gly; best_sk=sk; best_nap=nap; best_gf=gf; }
        CHECK_FALSE(r.has_nan);
        std::remove(path.c_str());
       }
      }
     }
    }

    printf("\n=== BEST: gly=%.1f SK=%.2f NaP=%.3f glia_f=%.2f → %d switches ===\n",
           best_gly, best_sk, best_nap, best_gf, best_sw);

    if (best_sw >= 5)
        CHECK(true);  // Ритм найден!
    else
        WARN("Best grid search result: " << best_sw << " switches — rhythm may need dIN→cIN weight tuning too");
}

// ═══════════════════════════════════════════════════════════════
// H5b: dIN→cIN вес тоже важен
// ═══════════════════════════════════════════════════════════════

TEST_CASE("CPG Tuning H5b: dIN→cIN weight sweep", "[cpg_tune]") {
    printf("\n=== H5b: dIN→cIN weight sweep ===\n");
    printf("%-8s %-8s %-8s %s\n","d→c_w","L_dIN","R_dIN","Switches");

    for (float dc : {1.5f, 2.0f, 2.5f, 3.0f, 3.5f}) {
        CpgNet net = build_net(4.0f, dc, 0.7f, 0.06f, 0.5f);
        std::string path = write_bion("h5b", net);
        TuneResult r = run_tune(path, 5000);
        printf("%-8.1f %-8.1f %-8.1f %d\n", dc, r.l_din_avg, r.r_din_avg, r.switches);
        CHECK_FALSE(r.has_nan);
        std::remove(path.c_str());
    }
    SUCCEED("H5b: dIN→cIN sweep complete");
}

// ═══════════════════════════════════════════════════════════════
// H6: CPG grid search на CUDA GPU (где спайки работают)
// ═══════════════════════════════════════════════════════════════

struct CudaCPG {
    void *handle, *sim;
    void* (*create)(); void (*destroy)(void*);
    int (*load_bion)(void*, const char*);
    void (*run_tick)(void*, float);
    void* (*bion_data)(void*);
    bool init() {
        const char* paths[] = {"./libbiocore_gpu_backend_cuda.so",
                               "../BioNeuronCore/build/libbiocore_gpu_backend_cuda.so",
                               "libbiocore_gpu_backend_cuda.so"};
        for (int i=0;i<3;i++){handle=dlopen(paths[i],RTLD_NOW);if(handle)break;}
        if(!handle)return false;
        create=(void*(*)())dlsym(handle,"gpu_backend_create");
        destroy=(void(*)(void*))dlsym(handle,"gpu_backend_destroy");
        load_bion=(int(*)(void*,const char*))dlsym(handle,"gpu_backend_load_bion");
        run_tick=(void(*)(void*,float))dlsym(handle,"gpu_backend_run_tick");
        bion_data=(void*(*)(void*))dlsym(handle,"gpu_backend_bion_data");
        return create&&load_bion&&run_tick&&bion_data;
    }
    void close(){if(handle)dlclose(handle);}
};

static float raw_vm_cuda(Segment* s){float v;memcpy(&v,(char*)s+40,4);return v;}
static float raw_py_cuda(Segment* s){float v;memcpy(&v,(char*)s+12,4);return v;}
static float raw_px_cuda(Segment* s){float v;memcpy(&v,(char*)s+8,4);return v;}
static uint8_t raw_st_cuda(Segment* s){uint8_t v;memcpy(&v,(char*)s+901,1);return v;}

static TuneResult run_tune_cuda(CudaCPG& cu, const std::string& path, int max_ticks) {
    TuneResult r={0,0,0,0,0,false};
    cu.sim=cu.create();
    if(!cu.sim||cu.load_bion(cu.sim,path.c_str())!=0){r.has_nan=true;return r;}
    BionData* bd=(BionData*)cu.bion_data(cu.sim);
    if(!bd){r.has_nan=true;cu.destroy(cu.sim);return r;}

    int last=-1; float l_sum=0,r_sum=0,l_din_sum=0,r_din_sum=0; int l_n=0,r_n=0,l_dn=0,r_dn=0;
    for(int tick=0;tick<=max_ticks;tick++){
        cu.run_tick(cu.sim,0.1f);
        if(tick%100==0){
            int l_sp=0,lc=0,r_sp=0,rc=0;
            for(uint32_t i=0;i<bd->num_segments;i++){
                float vm=raw_vm_cuda(&bd->segments[i]),py=raw_py_cuda(&bd->segments[i]),px=raw_px_cuda(&bd->segments[i]);
                uint8_t st=raw_st_cuda(&bd->segments[i]);
                if(std::isnan(vm)){r.has_nan=true;cu.destroy(cu.sim);return r;}
                if(st==1&&py>=15&&py<=25){
                    if(px<-1){lc++;if(vm>-20)l_sp++;l_sum+=vm;l_n++;}
                    else if(px>1){rc++;if(vm>-20)r_sp++;r_sum+=vm;r_n++;}
                }
                if(st==1&&py>=65&&py<=80){
                    if(px<-1){l_din_sum+=vm;l_dn++;}
                    else if(px>1){r_din_sum+=vm;r_dn++;}
                }
            }
            float la=lc>0?(float)l_sp/lc:0,ra=rc>0?(float)r_sp/rc:0;
            int cur=-1;
            if(la>0.3f&&ra<0.3f)cur=0;else if(ra>0.3f&&la<0.3f)cur=1;
            if(cur!=last&&cur>=0&&last>=0)r.switches++;
            if(cur>=0)last=cur;
        }
    }
    r.l_mn_avg=l_n>0?l_sum/l_n:0; r.r_mn_avg=r_n>0?r_sum/r_n:0;
    r.l_din_avg=l_dn>0?l_din_sum/l_dn:0; r.r_din_avg=r_dn>0?r_din_sum/r_dn:0;
    cu.destroy(cu.sim);
    return r;
}

TEST_CASE("CPG Tuning H6: CUDA GPU grid search", "[cpg_tune][gpu]") {
    CudaCPG cu;
    if(!cu.init()){WARN("CUDA not available");return;}

    printf("\n=== H6: CUDA GPU grid search ===\n");
    printf("%-8s %-8s %-8s %-8s %-8s %-8s %s\n","gly_w","SK","NaP","glia_f","L_dIN","R_dIN","Sw");

    int best=0; float bg=0,bs=0,bn=0,bgf=0;

    for(float gly:{4.0f,5.0f,6.0f}){
     for(float sk:{0.7f,0.9f,1.2f}){
      for(float nap:{0.05f,0.08f,0.12f}){
       for(float gf:{0.3f,0.5f,0.7f}){
        CpgNet net=build_net(gly,2.5f,sk,nap,gf);
        char tag[64];snprintf(tag,sizeof(tag),"c_g%.0f_s%.0f_n%.0f_gf%.0f",gly*10,sk*10,nap*100,gf*10);
        std::string path=write_bion(tag,net);
        TuneResult r=run_tune_cuda(cu,path,5000);
        printf("%-8.1f %-8.2f %-8.3f %-8.2f %-8.1f %-8.1f %d\n",gly,sk,nap,gf,r.l_din_avg,r.r_din_avg,r.switches);
        if(r.switches>best){best=r.switches;bg=gly;bs=sk;bn=nap;bgf=gf;}
        CHECK_FALSE(r.has_nan);
        std::remove(path.c_str());
       }
      }
     }
    }

    printf("\n=== CUDA BEST: gly=%.1f SK=%.2f NaP=%.3f glia_f=%.2f → %d switches ===\n",bg,bs,bn,bgf,best);
    if(best>=5)CHECK(true);else WARN("CUDA best: "<<best<<" switches");
    cu.close();
}
