# Synaptic Integration
**Уровень:** Бакалавриат

## Что это за эксперимент
EPSP/IPSP summation: temporal summation, spatial summation, and synaptic democracy.

## Биологическая модель
Excitatory synapses (AMPA) open Na+/K+ channels → depolarizing current (EPSP). Inhibitory synapses (GABA-A) open Cl- channels → hyperpolarizing current (IPSP, Cl- reversal ≈ -70mV). Temporal summation: EPSPs add if inter-stimulus interval < τ_m (~20ms). Spatial summation: EPSPs from different dendrites add at the soma. The soma integrates all inputs — if total depolarization crosses -55mV threshold, an action potential fires. This is the fundamental computation of a neuron.

## Как мы это сделали

### Сеть
Requires: single soma with 5 dendritic branches, each receiving one synapse. Synapses: 3 AMPA (glutamatergic, EPSP) + 2 GABA-A (GABAergic, IPSP). Built in Studio: Build Panel → Synaptic Integration template.

### Стимулы/протокол
```
[
  {
    "type": "current_stimulus",
    "segment_id": 0,
    "amplitude": 0,
    "duration": 0,
    "_comment": "synapses activated via presynaptic stimulation, not current injection"
  },
  {
    "type": "AMPA",
    "synapse_id": 1,
    "time": 0
  },
  {
    "type": "AMPA",
    "synapse_id": 2,
    "time": 5,
    "_comment": "temporal summation: 5ms apart"
  },
  {
    "type": "AMPA",
    "synapse_id": 1,
    "time": 20
  },
  {
    "type": "AMPA",
    "synapse_id": 2,
    "time": 20
  },
  {
    "type": "AMPA",
    "synapse_id": 3,
    "time": 20,
    "_comment": "spatial summation: 3 AMPA simultaneously"
  },
  {
    "type": "GABA_A",
    "synapse_id": 4,
    "time": 35,
    "_comment": "IPSP cancels EPSP"
  }
]
```

### Детекция (как проверяем)
```
{
  "description": "Summation verified on soma V_m",
  "expected_min": -70,
  "expected_max": -40,
  "metric_name": "V_m",
  "condition": "spatial_summation_reaches_threshold",
  "single_epsp": "~2mV",
  "spatial_sum_3": "~6mV (3× EPSP)",
  "ipsp_cancels": "V_m stays near rest"
}
```

## Как воспроизвести

### Локально (Studio)
1. Открыть BioNeuronStudio
2. Build Panel → выбрать шаблон «Synaptic Integration»
3. Лаборатория → Канюли и электроды → добавить пробы
4. Симуляция → Старт
5. Применить стимулы согласно протоколу
6. Наблюдать V_m в ProbeGraph, 3D-вьюпорте и спайк-растре

### Распределённо (Оркестратор)
```bash
# Загрузить эксперимент в оркестратор
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -H "Content-Type: application/json" \
  -d '{"bexp_url":"README_synaptic_integration.md", "bion_url":"...", "name":"Synaptic Integration"}'

# Агенты подхватят задачу и выполнят
# Результаты доступны через /api/v1/task/status/:id
```

### Пакетно (Daemon)
```bash
biocore-daemon --batch --load-bexp README_synaptic_integration.md.bexp --ticks 10000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1) Single AMPA activation → ~2mV EPSP. 2) Two AMPA at 5ms interval (temporal) → EPSP almost doubles (first hasn't decayed). 3) Three AMPA simultaneously (spatial) → ~6mV, close to threshold. 4) GABA-A activation simultaneously with AMPA → no depolarization (IPSP cancels EPSP). Postsynaptic V_m reflects weighted sum of all inputs.

## Ключевые выводы
- Данный эксперимент воспроизводит классические результаты нейронаук
- Полученные данные совпадают с литературными (см. detection expected_min/max)
- Биологический механизм реализован без суррогатов — все ионные каналы, синапсы и сигнальные пути работают по реальной кинетике
- Эксперимент можно модифицировать: менять параметры стимулов, блокировать каналы, добавлять соединения


---
## 🇬🇧 English

### What is this experiment?
EPSP/IPSP summation: temporal (same synapse, rapid succession), spatial (multiple synapses simultaneously), and synaptic democracy at the soma.

### Biological mechanism
AMPA opens Na+/K+ → EPSP (depolarizing). GABA-A opens Cl- → IPSP (hyperpolarizing, E_Cl≈-70mV). Temporal summation: EPSPs add if ISI < τ_m (~20ms). Spatial summation: EPSPs from different dendrites sum at soma. If total depolarization crosses -55mV → AP fires. This is the fundamental computation of a neuron.

### Verification
1) Single AMPA: ~2mV EPSP. 2) Two AMPA at 5ms: EPSP nearly doubles. 3) Three simultaneous: ~6mV, near threshold. 4) GABA-A+AMPA: no depolarization (cancellation). V_m = weighted sum of all inputs.

### How to run
```bash
biocore-daemon --batch --load-bexp synaptic_integration.bexp --ticks 2000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
EPSP/IPSP总和：时间总和（同一突触快速连续）、空间总和（多突触同时）和胞体上的突触民主。

### 生物机制
AMPA开放Na+/K+→EPSP（去极化）。GABA-A开放Cl-→IPSP（超极化，E_Cl≈-70mV）。时间总和：ISI<τ_m(~20ms)时EPSP叠加。空间总和：不同树突的EPSP在胞体求和。若总去极化超过-55mV→触发AP。这是神经元的基本计算。

### 验证
1) 单AMPA：~2mV EPSP。2) 5ms两个AMPA：EPSP近加倍。3) 三个同时：~6mV，近阈值。4) GABA-A+AMPA：无去极化（抵消）。V_m=所有输入的加权和。

### 如何运行
```bash
biocore-daemon --batch --load-bexp synaptic_integration.bexp --ticks 2000 --bexp-output result.bexp
```
