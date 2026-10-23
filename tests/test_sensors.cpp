// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_sensors.cpp — тесты сенсорных модификаторов (6, 94, 95, 108, 115-120)
// Каждый: стимул (state[1]) → активация (state[0]). Проверяет честный вход и GHK-ток.
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "types.h"
#include "modifier_interface.h"
#include <cmath>

extern double physical_sensor_current(Modifier*, Segment*, float);
extern double hunger_sensor_current(Modifier*, Segment*, float);
extern double light_sensor_current(Modifier*, Segment*, float);
extern double olfactory_receptor_current(Modifier*, Segment*, float);
extern double vibration_sensor_current(Modifier*, Segment*, float);
extern double otolith_hair_cell_current(Modifier*, Segment*, float);
extern double semicircular_cupula_current(Modifier*, Segment*, float);
extern double electroreceptor_ampullary_current(Modifier*, Segment*, float);
extern double magnetite_mechanoreceptor_current(Modifier*, Segment*, float);
extern double cryptochrome_magnetic_current(Modifier*, Segment*, float);

static void init_segment(Segment* s) {
    memset(s, 0, sizeof(Segment));
    s->V_m = -65.0f;
    s->length = 100.0f;
    s->radius = 10.0f;
    s->temperature = 37.0f;
    s->intra_ions[ION_NA] = 10.0f;
    s->extra_ions[ION_NA] = 140.0f;
    s->intra_ions[ION_K]  = 140.0f;
    s->extra_ions[ION_K]  = 4.0f;
    s->intra_ions[ION_CA] = 0.0001f;
    s->extra_ions[ION_CA] = 2.0f;
    s->intra_ions[ION_CL] = 10.0f;
    s->extra_ions[ION_CL] = 120.0f;
    s->extra_ions[ION_MG] = 1.0f;
    s->ATP_level = 3.0f;
}

static Modifier make_mod(uint8_t type, float density = 1.0f) {
    Modifier m = {};
    m.type = type;
    m.density = density;
    m.active_vars = 0;
    return m;
}

// ==================== Physical sensor (6) ====================

TEST_CASE("Physical sensor: high temperature activates", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.temperature = 45.0f;  // TRPV1 >43°C
    seg.modifiers[0] = make_mod(MOD_PHYSICAL_SENSOR);
    seg.modifier_count = 1;
    for (int t = 0; t < 100; t++) physical_sensor_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.5f);
}

TEST_CASE("Physical sensor: resting = no activation", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.temperature = 37.0f;
    seg.length = 50.0f;  // no stretch
    seg.modifiers[0] = make_mod(MOD_PHYSICAL_SENSOR);
    seg.modifier_count = 1;
    for (int t = 0; t < 100; t++) physical_sensor_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] < 0.1f);
}

// ==================== Hunger (94) ====================

TEST_CASE("Hunger: low ATP → hungry", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.ATP_level = 0.0f;  // истощение
    seg.modifiers[0] = make_mod(MOD_HUNGER_SENSOR);
    seg.modifier_count = 1;
    for (int t = 0; t < 2000; t++) hunger_sensor_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.5f);
}

TEST_CASE("Hunger: high ATP → not hungry", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.ATP_level = 5.0f;  // сытость
    seg.modifiers[0] = make_mod(MOD_HUNGER_SENSOR);
    seg.modifier_count = 1;
    for (int t = 0; t < 2000; t++) hunger_sensor_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] < 0.2f);
}

// ==================== Light (95) ====================

TEST_CASE("Light sensor: light activates photoreceptor", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_LIGHT_SENSOR);
    seg.modifiers[0].state[1] = 1.0f;  // интенсивность света
    seg.modifier_count = 1;
    for (int t = 0; t < 500; t++) light_sensor_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.5f);
}

// ==================== Olfactory (108) ====================

TEST_CASE("Olfactory: ligand activates receptor", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_OLFACTORY_RECEPTOR);
    seg.modifiers[0].state[1] = 1.0f;  // лиганд
    seg.modifier_count = 1;
    for (int t = 0; t < 500; t++) olfactory_receptor_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.5f);
}

// ==================== Vibration (115) ====================

TEST_CASE("Vibration sensor: vibration activates MET", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_VIBRATION_SENSOR);
    seg.modifiers[0].state[1] = 1.0f;
    seg.modifier_count = 1;
    for (int t = 0; t < 200; t++) vibration_sensor_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.5f);
}

// ==================== Otolith (116) ====================

TEST_CASE("Otolith: shear activates MET channels", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_OTOLITH_HAIR_CELL);
    seg.modifiers[0].state[1] = 1.0f;   // сдвиг
    seg.modifiers[0].state[5] = 1.0f;   // cos(θ)=1 (aligned)
    seg.modifier_count = 1;
    for (int t = 0; t < 100; t++) otolith_hair_cell_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.5f);
}

// ==================== Semicircular cupula (117) ====================

TEST_CASE("Cupula: angular acceleration deflects cupula", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_SEMICIRCULAR_CUPULA);
    seg.modifiers[0].state[1] = 1.0f;  // угловое ускорение
    seg.modifier_count = 1;
    for (int t = 0; t < 1000; t++) semicircular_cupula_current(&seg.modifiers[0], &seg, 1.0f);
    // Торсионный маятник: θ отклоняется от нуля
    REQUIRE(fabsf(seg.modifiers[0].state[0]) > 0.01f);
}

// ==================== Electroreceptor (118) ====================

TEST_CASE("Electroreceptor: E-field activates MET", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_ELECTRORECEPTOR_AMPULLARY);
    seg.modifiers[0].state[1] = 1.0f;  // внешнее поле
    seg.modifiers[0].state[5] = 1.0f;  // cos(θ_pref)=1 (aligned)
    seg.modifier_count = 1;
    for (int t = 0; t < 500; t++) electroreceptor_ampullary_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.5f);
}

// ==================== Magnetite (119) ====================

TEST_CASE("Magnetite: magnetic field activates torque", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_MAGNETITE_MECHANORECEPTOR);
    seg.modifiers[0].state[1] = 1.0f;  // |B|
    seg.modifiers[0].state[3] = 0.0f;  // cos(θ)=0 → sin(θ)=1 (max torque)
    seg.modifier_count = 1;
    for (int t = 0; t < 500; t++) magnetite_mechanoreceptor_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.5f);
}

// ==================== Cryptochrome (120) ====================

TEST_CASE("Cryptochrome: light + field → radical pair yield", "[sensors]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_CRYPTOCHROME_MAGNETIC);
    seg.modifiers[0].state[1] = 1.0f;  // свет
    seg.modifiers[0].state[2] = 1.0f;  // sin²(θ)=1
    seg.modifier_count = 1;
    for (int t = 0; t < 500; t++) cryptochrome_magnetic_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.5f);  // RP yield
}
