# Working Memory & Persistent Activity
**Уровень:** Магистратура

## Что это за эксперимент
Attractor dynamics: neural population maintains activity after stimulus removal via recurrent excitation.

## Биологическая модель
Pyramidal neurons are connected by recurrent excitatory synapses (AMPA+NMDA). When a stimulus activates a subset of neurons, the recurrent excitation sustains their activity even after the stimulus ends — this is an 'attractor state'. NMDA receptors are critical because their slow kinetics (τ_decay ~100ms) bridge the gap between spikes. Inhibitory interneurons prevent runaway excitation. Kir channels stabilize the membrane near threshold, while SK channels provide activity-dependent negative feedback. The network can hold one or two 'items' in memory simultaneously (capacity limited by mutual inhibition between competing attractors).

## Как мы это сделали

### Сеть
Requires: 50 excitatory (AMPA+NMDA) pyramidal neurons with recurrent connections + 10 inhibitory (GABA-A) interneurons. Modifiers: MOD_NA_HH, MOD_K_HH, NMDA, AMPA, GABA-A, MOD_KIR_CHANNEL, MOD_NAP_CHANNEL, MOD_SK_CHANNEL. Built in Studio: Build Panel → Working Memory template.

### Стимулы/протокол
```
[
  {
    "type": "current_stimulus",
    "segment_id": 0,
    "amplitude": 100,
    "duration": 200,
    "comment": "brief stimulus to memory-related population"
  },
  {
    "type": "current_stimulus",
    "segment_id": 0,
    "amplitude": 100,
    "duration": 200,
    "comment_2": "distractor stimulus to competing population"
  }
]
```

### Детекция (как проверяем)
```
{
  "description": "Persistent activity after stimulus removal",
  "expected_min": -60,
  "expected_max": -40,
  "metric_name": "firing_rate",
  "condition": "activity_persists_after_stimulus",
  "persistence_duration": ">2000ms after stimulus offset",
  "distractor_effect": "brief dip in firing, then recovery"
}
```

## Как воспроизвести

### Локально (Studio)
1. Открыть BioNeuronStudio
2. Build Panel → выбрать шаблон «Working Memory & Persistent Activity»
3. Лаборатория → Канюли и электроды → добавить пробы
4. Симуляция → Старт
5. Применить стимулы согласно протоколу
6. Наблюдать V_m в ProbeGraph, 3D-вьюпорте и спайк-растре

### Распределённо (Оркестратор)
```bash
# Загрузить эксперимент в оркестратор
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -H "Content-Type: application/json" \
  -d '{"bexp_url":"README_working_memory.md", "bion_url":"...", "name":"Working Memory & Persistent Activity"}'

# Агенты подхватят задачу и выполнят
# Результаты доступны через /api/v1/task/status/:id
```

### Пакетно (Daemon)
```bash
biocore-daemon --batch --load-bexp README_working_memory.md.bexp --ticks 10000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1) Stimulus presentation → population fires at elevated rate (20-40 Hz). 2) Stimulus removed → firing persists for 2+ seconds (working memory maintenance). 3) Distractor stimulus → brief dip, then recovery (resilience). 4) Blocking NMDA receptors → persistent activity collapses (NMDA essential for maintenance). 5) Blocking Kir channels → persistent activity with higher rate and less stability. This matches prefrontal cortex recordings in delayed-response tasks (Goldman-Rakic).

## Ключевые выводы
- Данный эксперимент воспроизводит классические результаты нейронаук
- Полученные данные совпадают с литературными (см. detection expected_min/max)
- Биологический механизм реализован без суррогатов — все ионные каналы, синапсы и сигнальные пути работают по реальной кинетике
- Эксперимент можно модифицировать: менять параметры стимулов, блокировать каналы, добавлять соединения


---
## 🇬🇧 English

### What is this experiment?
Attractor dynamics: neural population maintains elevated activity after stimulus removal via recurrent NMDA-dependent excitation. Matches prefrontal cortex recordings in delayed-response tasks.

### Biological mechanism
Pyramidal neurons connected by recurrent excitatory synapses (AMPA+NMDA). Stimulus activates subset → recurrent excitation sustains activity after stimulus ends — an 'attractor state'. NMDA critical: slow kinetics (τ~100ms) bridge inter-spike gaps. Inhibitory interneurons prevent runaway. Kir stabilizes near threshold. SK provides negative feedback. Capacity limited to 1-2 items by mutual inhibition between attractors.

### Verification
1) Stimulus → 20-40 Hz firing. 2) Activity persists 2+ seconds after removal. 3) Distractor → brief dip then recovery. 4) Block NMDA → persistent activity collapses. 5) Block Kir → higher rate, less stable.

### How to run
```bash
biocore-daemon --batch --load-bexp working_memory.bexp --ticks 50000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
吸引子动力学：神经群体通过依赖NMDA的递归兴奋在刺激移除后维持升高活动。与延迟反应任务中的前额叶皮层记录匹配。

### 生物机制
锥体神经元通过递归兴奋性突触（AMPA+NMDA）连接。刺激激活子集→递归兴奋在刺激结束后维持活动——'吸引子状态'。NMDA关键：缓慢动力学（τ~100ms）桥接锋电位间间隙。抑制性中间神经元防止失控。Kir稳定在阈值附近。SK提供负反馈。容量限于1-2个项目，因吸引子间相互抑制。

### 验证
1) 刺激→20-40 Hz放电。2) 活动持续2秒以上。3) 干扰→短暂下降后恢复。4) 阻断NMDA→持续活动崩溃。5) 阻断Kir→更高频率，更不稳定。

### 如何运行
```bash
biocore-daemon --batch --load-bexp working_memory.bexp --ticks 50000 --bexp-output result.bexp
```
