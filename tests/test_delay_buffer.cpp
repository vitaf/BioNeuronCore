// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_delay_buffer.cpp — тесты буфера задержанных синаптических выбросов

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "delay_buffer.h"
#include <cstring>

TEST_CASE("DelayedRelease size is 16 bytes", "[delay_buffer]") {
    REQUIRE(sizeof(DelayedRelease) == 16);
}

TEST_CASE("MAX_DELAY_SLOTS is 4", "[delay_buffer]") {
    REQUIRE(MAX_DELAY_SLOTS == 4);
}

TEST_CASE("process_delayed_releases — empty slots return false", "[delay_buffer]") {
    DelayedRelease slots[MAX_DELAY_SLOTS] = {};
    float cleft[NT_COUNT] = {};

    bool applied = process_delayed_releases(slots, MAX_DELAY_SLOTS, 100, cleft);
    REQUIRE(applied == false);
    for (int n = 0; n < NT_COUNT; n++)
        REQUIRE(cleft[n] == 0.0f);
}

TEST_CASE("process_delayed_releases — arrival_tick in future not processed", "[delay_buffer]") {
    DelayedRelease slots[MAX_DELAY_SLOTS] = {};
    slots[0].arrival_tick = 200;
    slots[0].amount = 1.5f;
    slots[0].nt_type = NT_GLUTAMATE;
    float cleft[NT_COUNT] = {};

    bool applied = process_delayed_releases(slots, MAX_DELAY_SLOTS, 100, cleft);
    REQUIRE(applied == false);
    REQUIRE(cleft[NT_GLUTAMATE] == 0.0f);
    REQUIRE(slots[0].arrival_tick == 200); // не освобождён
}

TEST_CASE("process_delayed_releases — arrival_tick at current tick is processed", "[delay_buffer]") {
    DelayedRelease slots[MAX_DELAY_SLOTS] = {};
    slots[0].arrival_tick = 100;
    slots[0].amount = 2.0f;
    slots[0].nt_type = NT_GABA;
    float cleft[NT_COUNT] = {};

    bool applied = process_delayed_releases(slots, MAX_DELAY_SLOTS, 100, cleft);
    REQUIRE(applied == true);
    REQUIRE(cleft[NT_GABA] == 2.0f);
    REQUIRE(slots[0].arrival_tick == 0); // освобождён
}

TEST_CASE("process_delayed_releases — past tick is processed", "[delay_buffer]") {
    DelayedRelease slots[MAX_DELAY_SLOTS] = {};
    slots[0].arrival_tick = 50;
    slots[0].amount = 0.5f;
    slots[0].nt_type = NT_GLYCINE;
    float cleft[NT_COUNT] = {};

    bool applied = process_delayed_releases(slots, MAX_DELAY_SLOTS, 200, cleft);
    REQUIRE(applied == true);
    REQUIRE(cleft[NT_GLYCINE] == 0.5f);
    REQUIRE(slots[0].arrival_tick == 0);
}

TEST_CASE("process_delayed_releases — multiple slots accumulate", "[delay_buffer]") {
    DelayedRelease slots[MAX_DELAY_SLOTS] = {};
    slots[0].arrival_tick = 100;
    slots[0].amount = 1.0f;
    slots[0].nt_type = NT_GLUTAMATE;
    slots[1].arrival_tick = 100;
    slots[1].amount = 2.0f;
    slots[1].nt_type = NT_GLUTAMATE;
    float cleft[NT_COUNT] = {};

    bool applied = process_delayed_releases(slots, MAX_DELAY_SLOTS, 100, cleft);
    REQUIRE(applied == true);
    REQUIRE(cleft[NT_GLUTAMATE] == 3.0f);
    REQUIRE(slots[0].arrival_tick == 0);
    REQUIRE(slots[1].arrival_tick == 0);
}

TEST_CASE("enqueue_delayed_release — finds empty slot", "[delay_buffer]") {
    DelayedRelease slots[MAX_DELAY_SLOTS] = {};
    bool ok = enqueue_delayed_release(slots, MAX_DELAY_SLOTS, 150, 0.8f, NT_DOPAMINE);
    REQUIRE(ok == true);
    REQUIRE(slots[0].arrival_tick == 150);
    REQUIRE(slots[0].amount == 0.8f);
    REQUIRE(slots[0].nt_type == NT_DOPAMINE);
}

TEST_CASE("enqueue_delayed_release — uses next free slot", "[delay_buffer]") {
    DelayedRelease slots[MAX_DELAY_SLOTS] = {};
    slots[0].arrival_tick = 100;
    slots[0].amount = 1.0f;
    bool ok = enqueue_delayed_release(slots, MAX_DELAY_SLOTS, 200, 2.0f, NT_SEROTONIN);
    REQUIRE(ok == true);
    REQUIRE(slots[1].arrival_tick == 200);
    REQUIRE(slots[1].amount == 2.0f);
    REQUIRE(slots[1].nt_type == NT_SEROTONIN);
    // slot 0 untouched
    REQUIRE(slots[0].arrival_tick == 100);
    REQUIRE(slots[0].amount == 1.0f);
}

TEST_CASE("enqueue_delayed_release — returns false when full", "[delay_buffer]") {
    DelayedRelease slots[MAX_DELAY_SLOTS] = {};
    for (int i = 0; i < MAX_DELAY_SLOTS; i++)
        slots[i].arrival_tick = 100 + i;

    bool ok = enqueue_delayed_release(slots, MAX_DELAY_SLOTS, 200, 1.0f, NT_GLUTAMATE);
    REQUIRE(ok == false); // все слоты заняты
}

TEST_CASE("delay_ms_to_ticks — zero delay returns zero", "[delay_buffer]") {
    REQUIRE(delay_ms_to_ticks(0.0f, 0.1f) == 0);
    REQUIRE(delay_ms_to_ticks(0.0f, 0.0f) == 0);
    REQUIRE(delay_ms_to_ticks(-1.0f, 0.1f) == 0);
}

TEST_CASE("delay_ms_to_ticks — rounds to nearest tick", "[delay_buffer]") {
    // delay=2.0ms, dt=0.1ms → 20 ticks
    REQUIRE(delay_ms_to_ticks(2.0f, 0.1f) == 20);
    // delay=1.0ms, dt=0.5ms → 2 ticks
    REQUIRE(delay_ms_to_ticks(1.0f, 0.5f) == 2);
    // delay=0.7ms, dt=0.1ms → 7 ticks
    REQUIRE(delay_ms_to_ticks(0.7f, 0.1f) == 7);
    // delay=0.04ms, dt=0.1ms → 0 ticks (rounds to 0)
    REQUIRE(delay_ms_to_ticks(0.04f, 0.1f) == 0);
    // delay=0.06ms, dt=0.1ms → 1 tick (rounds to 1)
    REQUIRE(delay_ms_to_ticks(0.06f, 0.1f) == 1);
}

TEST_CASE("end-to-end — enqueue then process", "[delay_buffer]") {
    DelayedRelease slots[MAX_DELAY_SLOTS] = {};
    float cleft[NT_COUNT] = {};

    // Enqueue at tick 100 with delay → arrival at 120
    bool ok = enqueue_delayed_release(slots, MAX_DELAY_SLOTS, 120, 1.5f, NT_GLUTAMATE);
    REQUIRE(ok == true);

    // Tick 110 — ещё не прибыл
    bool applied = process_delayed_releases(slots, MAX_DELAY_SLOTS, 110, cleft);
    REQUIRE(applied == false);
    REQUIRE(cleft[NT_GLUTAMATE] == 0.0f);

    // Tick 120 — прибыл
    applied = process_delayed_releases(slots, MAX_DELAY_SLOTS, 120, cleft);
    REQUIRE(applied == true);
    REQUIRE(cleft[NT_GLUTAMATE] == 1.5f);
    REQUIRE(slots[0].arrival_tick == 0);

    // Повторная обработка не должна добавить ещё раз
    applied = process_delayed_releases(slots, MAX_DELAY_SLOTS, 130, cleft);
    REQUIRE(applied == false);
    REQUIRE(cleft[NT_GLUTAMATE] == 1.5f);
}
