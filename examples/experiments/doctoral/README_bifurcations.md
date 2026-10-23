# Bifurcations & Dynamical Systems in Neurons
**Уровень:** Аспирантура

## Что это за эксперимент
Hopf bifurcations, period-doubling, and chaotic dynamics in single neurons and small circuits.

## Биологическая модель
A neuron is a nonlinear dynamical system. Its state variables (V_m, m, h, n gates + slow variables like [Ca2+]) form a phase space. Bifurcations occur when a parameter crosses a critical value and the system's qualitative behavior changes. Hopf bifurcation: stable fixed point loses stability → limit cycle (oscillation) emerges. SNIC (saddle-node on invariant circle): infinite-period bifurcation producing type I excitability. The persistent Na+ current (NaP) creates a negative conductance region near threshold → bistability between rest and firing. M-current (Kv7) controls the bifurcation type: with strong M-current (type II), subthreshold oscillations precede spiking. Period-doubling route to chaos occurs when the neuron's dynamics are driven fast enough that the system can't settle into a stable periodic orbit. This is the theoretical foundation of computational neuroscience — the same mathematics that describes lasers, chemical oscillators, and ecological systems.

## Как мы это сделали

### Сеть
Requires: single neuron with HH channels + persistent Na+ (NaP) + slow K+ (M-current). Alternatively, Hindmarsh-Rose type neuron for chaotic dynamics. Built in Studio: Build Panel → Dynamical System template.

### Стимулы/протокол
```
[
  {
    "type": "parameter_sweep",
    "param": "g_NaP",
    "range": [
      0.0,
      2.0
    ],
    "comment": "vary persistent Na+ conductance → Hopf bifurcation"
  },
  {
    "type": "parameter_sweep",
    "param": "I_applied",
    "range": [
      0.0,
      50.0
    ],
    "comment": "vary applied current → resting→spiking→bursting→depolarization block"
  },
  {
    "type": "parameter_sweep",
    "param": "g_Kv7",
    "range": [
      0.0,
      3.0
    ],
    "comment": "vary M-current → type I vs type II excitability"
  }
]
```

### Детекция (как проверяем)
```
{
  "description": "Bifurcation diagram and dynamical regimes",
  "metric_name": "interspike_interval",
  "condition": "bifurcation_detected",
  "hopf_point": "g_NaP ~0.15 (rest→oscillation)",
  "bistability": "hysteresis between g_NaP 0.1-0.2",
  "type_I": "frequency→0 at threshold (SNIC bifurcation)",
  "type_II": "finite frequency onset (Hopf bifurcation)",
  "period_doubling": "at high I_app, period-2→period-4→chaos"
}
```

## Как воспроизвести

### Локально (Studio)
1. Открыть BioNeuronStudio
2. Build Panel → выбрать шаблон «Bifurcations & Dynamical Systems in Neurons»
3. Лаборатория → Канюли и электроды → добавить пробы
4. Симуляция → Старт
5. Применить стимулы согласно протоколу
6. Наблюдать V_m в ProbeGraph, 3D-вьюпорте и спайк-растре

### Распределённо (Оркестратор)
```bash
# Загрузить эксперимент в оркестратор
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -H "Content-Type: application/json" \
  -d '{"bexp_url":"README_bifurcations.md", "bion_url":"...", "name":"Bifurcations & Dynamical Systems in Neurons"}'

# Агенты подхватят задачу и выполнят
# Результаты доступны через /api/v1/task/status/:id
```

### Пакетно (Daemon)
```bash
biocore-daemon --batch --load-bexp README_bifurcations.md.bexp --ticks 10000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1) Varying g_NaP: below 0.15 → resting; at 0.15 → Hopf bifurcation → spontaneous oscillations appear; 0.15-0.25 → frequency increases; above 0.25 → depolarization block. 2) Bistability: between g_NaP 0.1-0.2, both resting AND oscillating states are stable → hysteresis loop. 3) Type I (low M-current): as I_app increases, firing starts from 0 Hz with infinite period (SNIC bifurcation). Type II (high M-current): firing starts at finite frequency (Hopf bifurcation). 4) At high drive: period-doubling cascade → chaos → depolarization block. Plots: V_m vs I_app bifurcation diagram, ISI return map.

## Ключевые выводы
- Данный эксперимент воспроизводит классические результаты нейронаук
- Полученные данные совпадают с литературными (см. detection expected_min/max)
- Биологический механизм реализован без суррогатов — все ионные каналы, синапсы и сигнальные пути работают по реальной кинетике
- Эксперимент можно модифицировать: менять параметры стимулов, блокировать каналы, добавлять соединения


---
## 🇬🇧 English

### What is this experiment?
Hopf bifurcations, period-doubling cascades, and chaotic dynamics: SNIC vs Hopf (Type I vs II excitability), bistability with NaP, and the period-doubling route to chaos.

### Biological mechanism
A neuron is a nonlinear dynamical system. Hopf: stable fixed point loses stability→limit cycle emerges. SNIC: infinite-period bifurcation→Type I (f→0 at threshold). NaP creates negative conductance→bistability. M-current (Kv7) controls Type I vs II. Period-doubling cascade at high drive→chaos→block. Same mathematics as lasers, chemical oscillators, ecology.

### Verification
1) g_NaP<0.15: rest. 0.15: Hopf→oscillation. >0.25: block. 2) Bistability 0.1-0.2 (hysteresis). 3) Low M-current: Type I (SNIC). High: Type II (Hopf). 4) Period-2→4→chaos→block at high I_app.

### How to run
```bash
biocore-daemon --batch --load-bexp bifurcations.bexp --ticks 100000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
Hopf分岔、倍周期级联和混沌动力学：SNIC vs Hopf（I型vs II型兴奋性）、NaP双稳态和通向混沌的倍周期路径。

### 生物机制
神经元是非线性动力系统。Hopf：稳定不动点失稳→极限环出现。SNIC：无限周期分岔→I型（阈值处f→0）。NaP产生负电导→双稳态。M电流（Kv7）控制I型vs II型。高驱动下倍周期级联→混沌→阻滞。与激光、化学振荡器、生态学相同的数学。

### 验证
1) g_NaP<0.15：静息。0.15：Hopf→振荡。>0.25：阻滞。2) 0.1-0.2双稳态（滞后）。3) 低M电流：I型（SNIC）。高：II型（Hopf）。4) 高I_app下周期-2→4→混沌→阻滞。

### 如何运行
```bash
biocore-daemon --batch --load-bexp bifurcations.bexp --ticks 100000 --bexp-output result.bexp
```
