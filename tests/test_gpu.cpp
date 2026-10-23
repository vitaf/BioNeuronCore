// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#include <sycl/sycl.hpp>
#include <cstdio>
#include <cmath>
#include "types.h"
#include "modifier_interface.h"
#include "simulation.h"

// Минимальный SYCL-тест: создание сети вручную (без .bion), симуляция нескольких тиков, проверка результата
int main() {
    init_biological_library();

    // Выбор устройства: пробуем GPU, при ошибке — CPU
    sycl::device dev;
    try {
        dev = sycl::device(sycl::gpu_selector_v);
        printf("GPU-устройство: %s\n", dev.get_info<sycl::info::device::name>().c_str());
    } catch (sycl::exception& e) {
        printf("GPU не найден (%s), пробуем CPU...\n", e.what());
        dev = sycl::device(sycl::cpu_selector_v);
        printf("CPU-устройство: %s\n", dev.get_info<sycl::info::device::name>().c_str());
    }

    Simulation sim(dev);
    printf("Очередь создана.\n");
    sim.header.mode = MODE_BIOLOGICAL;
    sim.num_segments = 1;
    sim.num_connections = 0;
    sim.num_synapses = 0;

    // Выделение памяти через USM
    sim.segments = sycl::malloc_shared<Segment>(1, sim.q);
    sim.connections = nullptr;
    sim.synapses = nullptr;
    sim.neurons = sycl::malloc_shared<Neuron>(1, sim.q);

    if (!sim.segments || !sim.neurons) {
        printf("Ошибка выделения USM-памяти\n");
        return 1;
    }

    // Инициализация сегмента (как в тесте resting potential)
    Segment& seg = sim.segments[0];
    seg = {};
    seg.id = 0;
    seg.neuron_id = 0;
    seg.V_m = -65.0f;
    seg.intra_ions[ION_NA] = 10.0f;   seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_K]  = 140.0f;  seg.extra_ions[ION_K]  = 4.0f;
    seg.intra_ions[ION_CL] = 10.0f;   seg.extra_ions[ION_CL] = 120.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;

    // Модификаторы: Na/K каналы + насос
    seg.modifiers[0] = {MOD_AXON_NAV12, {0,0,0}, 15.6f, {0.05f, 0.6f}, 2, 0, {0, 0}};
    seg.modifiers[1] = {MOD_KV21, {0,0,0}, 3.84f, {0.05f, 1.0f}, 2, 0, {0, 0}};
    seg.modifiers[2] = {MOD_PUMP_NAK, {0,0,0}, 1.0f, {}, 0, 0, {0, 0}};
    seg.modifier_count = 3;

    // Инициализация нейрона
    sim.neurons[0] = {};
    sim.neurons[0].id = 0;
    sim.neurons[0].segment_start = 0;
    sim.neurons[0].segment_count = 1;
    sim.neurons[0].soma_index = 0;

    // Симуляция 500 тиков (ожидаем стабилизацию V_m около -65 мВ)
    float dt = 1.0f;
    printf("Запуск симуляции на %d тиков...\n", 500);
    int result = 0;
    for (int tick = 0; tick < 500; tick++) {
        try {
            sim.run_tick(dt);
        } catch (sycl::exception& e) {
            printf("Ошибка SYCL на тике %d: %s\n", tick, e.what());
            result = 1;
            break;
        }
    }
    if (result != 0) {
        // Освобождение — через симуляцию (установка nullptr + деструктор)
        sycl::free(sim.segments, sim.q); sim.segments = nullptr;
        sycl::free(sim.neurons, sim.q);  sim.neurons = nullptr;
        return result;
    }
    printf("Симуляция завершена.\n");

    float final_V = seg.V_m;
    printf("После 500 тиков (SYCL): V_m = %.2f мВ\n", final_V);

    // Проверка: потенциал покоя должен быть около -65 мВ
    bool ok = (final_V > -75.0f && final_V < -55.0f);
    printf("Проверка: V_m в диапазоне [-75, -55]: %s\n", ok ? "OK" : "FAIL");

    // Очистка (nullptr после free чтобы деструктор не повторял)
    sycl::free(sim.segments, sim.q); sim.segments = nullptr;
    sycl::free(sim.neurons, sim.q);  sim.neurons = nullptr;

    return ok ? 0 : 1;
}
