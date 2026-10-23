// dispatch_generated.cpp — АВТОГЕНЕРАЦИЯ BioNeuronSDK.
// Сгенерировано biocore-gen-dispatch. НЕ РЕДАКТИРОВАТЬ ВРУЧНУЮ.
// Пользовательских типов: 2

#include "types.h"
#include "kernel_dispatch.h"

// WEAK-заглушки: текущие функции пользовательских плагинов
// Вызываются ТОЛЬКО если .so плагин не загружен (modifier_current_table[id] == NULL).
// Возвращают 0.0f — нет тока, нейрон в покое (биологически безопасно).
__attribute__((weak))
BIOCORE_DEVICE_CALLABLE double plugin_mod_124_current(Modifier* m, Segment* s, float dt) {
    (void)m; (void)s; (void)dt;
    return 0.0;
}

// WEAK-заглушки: пресинаптические модуляторы пользовательских плагинов
// Вызываются ТОЛЬКО если .so плагин не загружен (modulator_release_table[id] == NULL).
// Возвращают 1.0f — нет модуляции выброса (биологически безопасно).
__attribute__((weak))
BIOCORE_DEVICE_CALLABLE float plugin_mod_125_release(Modifier* m, float* cleft_nt, float dt) {
    (void)m; (void)cleft_nt; (void)dt;
    return 1.0f;
}

BIOCORE_DEVICE_CALLABLE double dispatch_user_modifier_current(uint8_t type, Modifier* m, Segment* s, float dt) {
    switch (type) {
        case 124: return plugin_mod_124_current(m, s, dt);
        default: return 0.0f;
    }
}

BIOCORE_DEVICE_CALLABLE double dispatch_user_release_modulator(uint8_t type, Modifier* m, float* cleft_nt, float dt) {
    switch (type) {
        case 125: return plugin_mod_125_release(m, cleft_nt, dt);
        default: return 1.0f;
    }
}
