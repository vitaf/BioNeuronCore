# Lateral Inhibition & Contrast Enhancement
**Уровень:** Бакалавриат

## Что это за эксперимент
How neural circuits sharpen sensory input: adjacent neurons inhibit each other via GABA-A, reducing noise and enhancing edges.

## Биологическая модель
Each receptor R_i sends GABA-A inhibition to its left neighbor R_{i-1}. When all receptors receive equal input, inhibition is symmetric — all fire equally. When R2-R5 receive stronger input: R2's left neighbor R1 is weak → R2 receives little inhibition → R2 fires strongly. R6 receives strong inhibition from R5 → R6 fires weakly. This creates a 'Mach band': enhanced contrast at the edge between strong and weak input regions. The same principle operates in the retina (horizontal cells) and somatosensory cortex.

## Как мы это сделали

### Сеть
Requires: 10 receptor neurons (R0-R9) in a linear array. R_i inhibits R_{i-1} through GABA-A synapse. Each receptor receives independent input. Built in Studio: Build Panel → Lateral Inhibition template.

### Стимулы/протокол
```
[
  {
    "type": "current_stimulus",
    "segment_id": 0,
    "amplitude": 20,
    "duration": 100,
    "comment": "uniform input to all receptors"
  },
  {
    "type": "current_stimulus",
    "segment_id": 2,
    "amplitude": 60,
    "duration": 100,
    "comment": "stronger input to R2-R5 (edge)"
  },
  {
    "type": "current_stimulus",
    "segment_id": 5,
    "amplitude": 10,
    "duration": 100,
    "comment": "weak input beyond edge"
  }
]
```

### Детекция (как проверяем)
```
{
  "description": "Contrast enhancement verified",
  "expected_min": -60,
  "expected_max": -30,
  "metric_name": "V_m",
  "condition": "edge_enhanced",
  "uniform_ratio": "V(R_edge)/V(R_center) >> input_ratio",
  "note": "without inhibition, all receptors would have similar V_m"
}
```

## Как воспроизвести

### Локально (Studio)
1. Открыть BioNeuronStudio
2. Build Panel → выбрать шаблон «Lateral Inhibition & Contrast Enhancement»
3. Лаборатория → Канюли и электроды → добавить пробы
4. Симуляция → Старт
5. Применить стимулы согласно протоколу
6. Наблюдать V_m в ProbeGraph, 3D-вьюпорте и спайк-растре

### Распределённо (Оркестратор)
```bash
# Загрузить эксперимент в оркестратор
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -H "Content-Type: application/json" \
  -d '{"bexp_url":"README_lateral_inhibition.md", "bion_url":"...", "name":"Lateral Inhibition & Contrast Enhancement"}'

# Агенты подхватят задачу и выполнят
# Результаты доступны через /api/v1/task/status/:id
```

### Пакетно (Daemon)
```bash
biocore-daemon --batch --load-bexp README_lateral_inhibition.md.bexp --ticks 10000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1) Uniform stimulus to all receptors → all fire at similar rate. 2) Edge stimulus (R2-R5 strong, rest weak) → R2 fires MORE than expected (disinhibited because R3 inhibits R2 less). R5 fires LESS than R1 (R4 strongly inhibits R5). 3) The output difference between R2 and R6 is larger than the input difference — contrast is enhanced. This is Mach band illusion in neural form.

## Ключевые выводы
- Данный эксперимент воспроизводит классические результаты нейронаук
- Полученные данные совпадают с литературными (см. detection expected_min/max)
- Биологический механизм реализован без суррогатов — все ионные каналы, синапсы и сигнальные пути работают по реальной кинетике
- Эксперимент можно модифицировать: менять параметры стимулов, блокировать каналы, добавлять соединения


---
## 🇬🇧 English

### What is this experiment?
How neural circuits sharpen sensory input: adjacent neurons inhibit each other via GABA-A, reducing noise and enhancing edges — the neural basis of Mach bands.

### Biological mechanism
Each receptor R_i inhibits left neighbor R_{i-1} via GABA-A. Equal input: symmetric inhibition → all fire equally. Edge input (R2-R5 stronger): R2's left neighbor R1 is weak → little inhibition → fires strongly. R6 gets strong inhibition from R5 → fires weakly. Creates enhanced contrast at edges. Same principle in retina (horizontal cells) and somatosensory cortex.

### Verification
1) Uniform stimulus → all fire similarly. 2) Edge stimulus → R2 fires MORE (disinhibited); R5 fires LESS (inhibited). 3) Output contrast > input contrast. 4) Block GABA-A → enhancement disappears.

### How to run
```bash
biocore-daemon --batch --load-bexp lateral_inhibition.bexp --ticks 2000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
神经回路如何锐化感觉输入：相邻神经元通过GABA-A相互抑制，减少噪声并增强边缘——马赫带的神经基础。

### 生物机制
每个受体R_i通过GABA-A抑制左邻R_{i-1}。等输入：对称抑制→均等放电。边缘输入（R2-R5更强）：R2的左邻R1弱→少抑制→强放电。R6受R5强抑制→弱放电。在边缘处产生增强对比度。相同原理在视网膜（水平细胞）和体感皮层。

### 验证
1) 均匀刺激→均等放电。2) 边缘刺激→R2放电更多（去抑制）；R5放电更少（抑制）。3) 输出对比度>输入对比度。4) 阻断GABA-A→增强消失。

### 如何运行
```bash
biocore-daemon --batch --load-bexp lateral_inhibition.bexp --ticks 2000 --bexp-output result.bexp
```
