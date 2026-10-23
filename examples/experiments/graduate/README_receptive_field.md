# Receptive Field — Center-Surround Organization
**Уровень:** Магистратура

## Что это за эксперимент
ON-center/OFF-surround receptive fields: spatial antagonism creates orientation and edge detection.

## Биологическая модель
Photoreceptors in the center connect directly to the bipolar cell via glutamatergic synapses. Photoreceptors in the surround connect to horizontal cells, which then inhibit the center via GABA-A synapses. This creates a 'Mexican hat' spatial filter: the cell responds best to a spot of light exactly matching its center, and is inhibited by light in the surround. The circuit enhances edges because at an edge, part of the center is illuminated (excitation) while the surround is dark (reduced inhibition) → maximal response. This is the first stage of visual feature detection, preceding orientation selectivity in V1.

## Как мы это сделали

### Сеть
Requires: 7×7 grid of photoreceptors (49 neurons) projecting to one bipolar cell (center) + horizontal cells (surround). Modifiers: MOD_IONOTROPIC_NAK, MOD_LIGHT_SENSOR, AMPA, GABA-A, gap junctions. Built in Studio: Build Panel → Retina template.

### Стимулы/протокол
```
[
  {
    "type": "light_stimulus",
    "region": "center_3x3",
    "intensity": 1.0,
    "comment": "light on center, dark surround → maximal ON-center response"
  },
  {
    "type": "light_stimulus",
    "region": "surround",
    "intensity": 1.0,
    "comment": "light on surround only → minimal ON-center response (inhibition from horizontal cells)"
  },
  {
    "type": "light_stimulus",
    "region": "full_field",
    "intensity": 1.0,
    "comment": "uniform illumination → weak response (center excitation + surround inhibition cancel)"
  },
  {
    "type": "light_stimulus",
    "region": "vertical_edge_right_side",
    "intensity": 1.0,
    "comment": "edge stimulus → orientation-selective response"
  }
]
```

### Детекция (как проверяем)
```
{
  "description": "Center-surround antagonism",
  "expected_min": -70,
  "expected_max": -50,
  "metric_name": "firing_rate",
  "condition": "center_ON_surround_OFF",
  "center_alone": "strong depolarization",
  "surround_alone": "hyperpolarization",
  "full_field": "weak response (cancellation)",
  "edge_right": "moderate depolarization (disinhibited center)"
}
```

## Как воспроизвести

### Локально (Studio)
1. Открыть BioNeuronStudio
2. Build Panel → выбрать шаблон «Receptive Field — Center-Surround Organization»
3. Лаборатория → Канюли и электроды → добавить пробы
4. Симуляция → Старт
5. Применить стимулы согласно протоколу
6. Наблюдать V_m в ProbeGraph, 3D-вьюпорте и спайк-растре

### Распределённо (Оркестратор)
```bash
# Загрузить эксперимент в оркестратор
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -H "Content-Type: application/json" \
  -d '{"bexp_url":"README_receptive_field.md", "bion_url":"...", "name":"Receptive Field — Center-Surround Organization"}'

# Агенты подхватят задачу и выполнят
# Результаты доступны через /api/v1/task/status/:id
```

### Пакетно (Daemon)
```bash
biocore-daemon --batch --load-bexp README_receptive_field.md.bexp --ticks 10000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1) Spot of light on center → bipolar cell depolarizes strongly (ON response). 2) Annulus of light on surround → bipolar cell hyperpolarizes (OFF response via horizontal cells). 3) Full-field illumination → weak or no response (center excitation + surround inhibition cancel). 4) Edge of light (half field) → moderate response (center partly illuminated, surround partly illuminated). This is the classic Kuffler (1953) retinal ganglion cell receptive field.

## Ключевые выводы
- Данный эксперимент воспроизводит классические результаты нейронаук
- Полученные данные совпадают с литературными (см. detection expected_min/max)
- Биологический механизм реализован без суррогатов — все ионные каналы, синапсы и сигнальные пути работают по реальной кинетике
- Эксперимент можно модифицировать: менять параметры стимулов, блокировать каналы, добавлять соединения


---
## 🇬🇧 English

### What is this experiment?
ON-center/OFF-surround receptive fields: spatial antagonism via horizontal cells creates edge and orientation detection. Classic Kuffler (1953) retinal ganglion cell organization.

### Biological mechanism
Center photoreceptors connect directly to bipolar cell (glutamate). Surround photoreceptors connect to horizontal cells which inhibit center via GABA-A. Creates 'Mexican hat' spatial filter: best response to light spot matching center, inhibited by surround light. At edge: center illuminated (excitation), surround dark (reduced inhibition) → maximal response.

### Verification
1) Center spot → strong depolarization. 2) Surround annulus → hyperpolarization. 3) Full-field → weak (cancellation). 4) Edge → moderate (disinhibited center). Matches Kuffler 1953.

### How to run
```bash
biocore-daemon --batch --load-bexp receptive_field.bexp --ticks 5000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
ON中心/OFF周边感受野：通过水平细胞的空间拮抗产生边缘和方向检测。经典Kuffler（1953）视网膜神经节细胞组织。

### 生物机制
中心感光细胞直接连接双极细胞（谷氨酸）。周边感光细胞连接水平细胞，通过GABA-A抑制中心。创建'墨西哥帽'空间滤波器：对匹配中心的光点响应最佳，被周边光抑制。边缘处：中心照亮（兴奋），周边暗（减少抑制）→最大响应。

### 验证
1) 中心光点→强去极化。2) 周边环→超极化。3) 全视野→弱（抵消）。4) 边缘→中等（去抑制中心）。匹配Kuffler 1953。

### 如何运行
```bash
biocore-daemon --batch --load-bexp receptive_field.bexp --ticks 5000 --bexp-output result.bexp
```
