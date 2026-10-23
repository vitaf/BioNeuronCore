// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#include "daemon_session.h"
#include <cstring>

void SessionContext::freeNetwork() {
    // Остановить симуляцию если запущена
    if (sim_running.load()) {
        shutdown = true;
        if (sim_thread.joinable()) sim_thread.join();
        sim_running = false;
    }

    if (simDataFromGpu) {
        // USM-память (освобождается через gpu_destroy)
        segments = nullptr;
        synapses = nullptr;
        connections = nullptr;
        neurons = nullptr;
        nuclei = nullptr;
        num_segments = 0;
        num_synapses = 0;
        num_connections = 0;
        num_neurons = 0;
        num_nuclei = 0;
        // gpu_sim освобождается владельцем (DaemonServer)
    } else {
        // CPU-память
        delete[] segments;
        delete[] synapses;
        delete[] connections;
        delete[] neurons;
        delete[] nuclei;
        segments = nullptr;
        synapses = nullptr;
        connections = nullptr;
        neurons = nullptr;
        nuclei = nullptr;
        num_segments = 0;
        num_synapses = 0;
        num_connections = 0;
        num_neurons = 0;
        num_nuclei = 0;
    }
    simDataFromGpu = false;
    initial_nuclei.clear();
    consolidation_started = false;
}
