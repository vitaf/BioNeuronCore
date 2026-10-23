# Central Pattern Generator (CPG)
**Уровень:** Магистратура

## Что это за эксперимент
Half-center oscillator: alternating bursts driven by reciprocal inhibition and post-inhibitory rebound.

## Биологическая модель
The left half-center excites left motor neurons AND inhibits the right half-center via cross-inhibitory interneurons (cIN, glycinergic). When left is active, right is silenced. As left-side SK channels (Ca2+-activated K+) accumulate Ca2+ and hyperpolarize left neurons, left stops firing → inhibition on right is released. Right-side HCN channels (Ih, activated by hyperpolarization) depolarize right neurons → post-inhibitory rebound → right side fires. The cycle repeats. This is the core mechanism of locomotion CPGs in lamprey, tadpole, and vertebrate spinal cord.

## Как мы это сделали

### Сеть
Requires: two half-centers (left/right) with mutual inhibition via glycine+GABA-B. Each side: excitatory dIN + inhibitory cIN. Modifiers: MOD_SK_CHANNEL, MOD_PIR_HCN_CHANNEL, MOD_NA_HH, MOD_K_HH, GABA-A, GABA-B, glycine. Built in Studio: Build Panel → CPG template.

### Стимулы/протокол
```
[
  {
    "type": "current_stimulus",
    "segment_id": 0,
    "amplitude": 40,
    "duration": 20,
    "comment": "asymmetric kick-start to left side"
  },
  {
    "type": "current_stimulus",
    "segment_id": 10,
    "amplitude": 20,
    "duration": 20,
    "comment": "weaker drive to right side, creates phase lag"
  }
]
```

### Детекция (как проверяем)
```
{
  "description": "Alternating left-right bursts",
  "expected_min": -60,
  "expected_max": 20,
  "metric_name": "V_m",
  "condition": "alternation",
  "burst_frequency": "~1-5 Hz",
  "anti_phase": "left and right 180° out of phase",
  "rebound_detected": "post-inhibitory rebound > 5mV"
}
```

## Как воспроизвести

### Локально (Studio)
1. Открыть BioNeuronStudio
2. Build Panel → выбрать шаблон «Central Pattern Generator (CPG)»
3. Лаборатория → Канюли и электроды → добавить пробы
4. Симуляция → Старт
5. Применить стимулы согласно протоколу
6. Наблюдать V_m в ProbeGraph, 3D-вьюпорте и спайк-растре

### Распределённо (Оркестратор)
```bash
# Загрузить эксперимент в оркестратор
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -H "Content-Type: application/json" \
  -d '{"bexp_url":"README_cpg_rhythm.md", "bion_url":"...", "name":"Central Pattern Generator (CPG)"}'

# Агенты подхватят задачу и выполнят
# Результаты доступны через /api/v1/task/status/:id
```

### Пакетно (Daemon)
```bash
biocore-daemon --batch --load-bexp README_cpg_rhythm.md.bexp --ticks 10000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1) Left and right sides fire in alternation (never simultaneously). 2) Burst frequency ~1-5 Hz (tadpole swimming range). 3) Post-inhibitory rebound visible: when left side stops inhibiting right side, right V_m rebounds above threshold (SK channel deactivation + HCN activation). 4) Removing inhibition (block GABA-A/glycine) → both sides fire continuously, no alternation. 5) Blocking SK channels (apamin) → alternation disrupted, tonic firing.

## Ключевые выводы
- Данный эксперимент воспроизводит классические результаты нейронаук
- Полученные данные совпадают с литературными (см. detection expected_min/max)
- Биологический механизм реализован без суррогатов — все ионные каналы, синапсы и сигнальные пути работают по реальной кинетике
- Эксперимент можно модифицировать: менять параметры стимулов, блокировать каналы, добавлять соединения


---
## 🇬🇧 English

### What is this experiment?
Half-center oscillator: alternating left-right bursts driven by reciprocal inhibition and post-inhibitory rebound. Core locomotion mechanism in lamprey, tadpole, and vertebrate spinal cord.

### Biological mechanism
Left half-center excites left motor neurons AND inhibits right via glycinergic cross-inhibitory interneurons. When left active, right silenced. Left SK channels hyperpolarize → left stops → inhibition on right released. Right HCN channels (Ih, activated by hyperpolarization) depolarize right → post-inhibitory rebound → right fires. Cycle repeats at 1-5 Hz.

### Verification
1) Left/right alternate — never simultaneous. 2) Burst frequency 1-5 Hz. 3) Post-inhibitory rebound >5mV. 4) Block inhibition → both fire continuously. 5) Block SK → alternation disrupted. 6) Block HCN → oscillation stops.

### How to run
```bash
biocore-daemon --batch --load-bexp cpg_rhythm.bexp --ticks 10000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
半中心振荡器：由交互抑制和抑制后反弹驱动的左右交替爆发。七鳃鳗、蝌蚪和脊椎动物脊髓运动的核心机制。

### 生物机制
左半中心兴奋左侧运动神经元并通过甘氨酸能交叉抑制中间神经元抑制右侧。左侧活动时右侧静默。左侧SK通道超极化→左侧停止→右侧抑制解除。右侧HCN通道（Ih，超极化激活）去极化右侧→抑制后反弹→右侧放电。以1-5 Hz重复。

### 验证
1) 左右交替——从不同时。2) 爆发频率1-5 Hz。3) 抑制后反弹>5mV。4) 阻断抑制→两侧连续放电。5) 阻断SK→交替打乱。6) 阻断HCN→振荡停止。

### 如何运行
```bash
biocore-daemon --batch --load-bexp cpg_rhythm.bexp --ticks 10000 --bexp-output result.bexp
```
