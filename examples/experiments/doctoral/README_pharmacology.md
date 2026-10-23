# Computational Pharmacology — Drug Screening
**Уровень:** Аспирантура

## Что это за эксперимент
Virtual drug screening: how different compounds affect ion channels and alter network dynamics.

## Биологическая модель
Each drug acts on specific ion channel targets with defined kinetics. Kv7 openers (retigabine) shift the activation curve left → more K+ current → hyperpolarization → reduced excitability. Na+ channel blockers (phenytoin) bind preferentially to the inactivated state → use-dependent block (more effective during high-frequency firing, as in seizures). GABA-A PAMs (diazepam) increase Cl- channel open probability → stronger inhibition. NMDA blockers (memantine) are uncompetitive → they enter the channel only when it's open and get trapped → voltage-dependent unblock at depolarization → preferential block of pathological (excessive) NMDA activation while sparing physiological activation.

## Как мы это сделали

### Сеть
Requires: network with Kv7, Nav1.x, Cav, GABA-A, AMPA, NMDA channels. Drug library defined in JSON. Built in Studio: network + pharmacology via ScreeningEngine in orchestrator.

### Стимулы/протокол
```
[
  {
    "type": "drug_application",
    "compound": "retigabine",
    "target": "Kv7",
    "action": "open",
    "ic50": 0.5,
    "emax": 2.0,
    "comment": "anticonvulsant — opens K+ channels"
  },
  {
    "type": "drug_application",
    "compound": "phenytoin",
    "target": "Nav1.2",
    "action": "block",
    "ic50": 5.0,
    "emax": 0.1,
    "comment": "anticonvulsant — blocks Na+ channels (use-dependent)"
  },
  {
    "type": "drug_application",
    "compound": "diazepam",
    "target": "GABA-A",
    "action": "modulate",
    "ic50": 0.05,
    "emax": 3.0,
    "comment": "anxiolytic/anticonvulsant — PAM at GABA-A"
  },
  {
    "type": "drug_application",
    "compound": "memantine",
    "target": "NMDA",
    "action": "block",
    "ic50": 1.0,
    "emax": 0.3,
    "comment": "Alzheimer's — uncompetitive NMDA blocker"
  }
]
```

### Детекция (как проверяем)
```
{
  "description": "Dose-response curves and therapeutic windows",
  "expected_min": -70,
  "expected_max": -30,
  "metric_name": "firing_rate",
  "condition": "drug_reduces_seizure_activity",
  "retigabine_ec50": "~0.5µM",
  "phenytoin_use_dependence": "block increases with frequency",
  "therapeutic_index": "IC50_side_effect / IC50_therapeutic > 10",
  "drug_interaction": "diazepam + retigabine synergistic at low doses"
}
```

## Как воспроизвести

### Локально (Studio)
1. Открыть BioNeuronStudio
2. Build Panel → выбрать шаблон «Computational Pharmacology — Drug Screening»
3. Лаборатория → Канюли и электроды → добавить пробы
4. Симуляция → Старт
5. Применить стимулы согласно протоколу
6. Наблюдать V_m в ProbeGraph, 3D-вьюпорте и спайк-растре

### Распределённо (Оркестратор)
```bash
# Загрузить эксперимент в оркестратор
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -H "Content-Type: application/json" \
  -d '{"bexp_url":"README_pharmacology.md", "bion_url":"...", "name":"Computational Pharmacology — Drug Screening"}'

# Агенты подхватят задачу и выполнят
# Результаты доступны через /api/v1/task/status/:id
```

### Пакетно (Daemon)
```bash
biocore-daemon --batch --load-bexp README_pharmacology.md.bexp --ticks 10000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1) Retigabine: dose-dependent reduction in firing rate (EC50 ~0.5µM). 2) Phenytoin: frequency-dependent block (more block at higher firing rates — use-dependence). 3) Diazepam: left-shifts GABA dose-response curve (PAM effect). 4) Memantine: voltage-dependent block of NMDA (trapped in channel, released only at depolarization). 5) Drug combination: retigabine + low-dose diazepam → synergistic seizure suppression (isobologram analysis). This is how pharmaceutical research uses computational models for early-stage screening.

## Ключевые выводы
- Данный эксперимент воспроизводит классические результаты нейронаук
- Полученные данные совпадают с литературными (см. detection expected_min/max)
- Биологический механизм реализован без суррогатов — все ионные каналы, синапсы и сигнальные пути работают по реальной кинетике
- Эксперимент можно модифицировать: менять параметры стимулов, блокировать каналы, добавлять соединения


---
## 🇬🇧 English

### What is this experiment?
Virtual drug screening: how anticonvulsant, anxiolytic, and neuroprotective compounds affect ion channels — dose-response curves, use-dependence, and synergistic drug interactions.

### Biological mechanism
Kv7 openers (retigabine): left-shift activation → hyperpolarization, EC50~0.5µM. Na+ blockers (phenytoin): bind inactivated state → use-dependent block (pathological high-frequency). GABA-A PAMs (diazepam): increase Cl- channel open probability. NMDA blockers (memantine): uncompetitive, voltage-dependent relief → preferential block of excessive activation.

### Verification
1) Retigabine: dose-dependent reduction, EC50~0.5µM. 2) Phenytoin: frequency-dependent block. 3) Diazepam: left-shifts GABA curve. 4) Memantine: voltage-dependent NMDA block. 5) Retigabine+diazepam: synergistic (isobologram).

### How to run
```bash
biocore-daemon --batch --load-bexp pharmacology.bexp --ticks 50000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
虚拟药物筛选：抗惊厥、抗焦虑和神经保护化合物如何影响离子通道——剂量-反应曲线、使用依赖性和协同药物相互作用。

### 生物机制
Kv7开放剂（瑞替加滨）：左移激活→超极化，EC50~0.5µM。Na+阻断剂（苯妥英）：结合失活状态→使用依赖性阻断（病理高频）。GABA-A正变构调节剂（地西泮）：增加Cl-通道开放概率。NMDA阻断剂（美金刚）：非竞争性，电压依赖性解除→优先阻断过度激活。

### 验证
1) 瑞替加滨：剂量依赖性减少，EC50~0.5µM。2) 苯妥英：频率依赖性阻断。3) 地西泮：左移GABA曲线。4) 美金刚：电压依赖性NMDA阻断。5) 瑞替加滨+地西泮：协同（等效线图法）。

### 如何运行
```bash
biocore-daemon --batch --load-bexp pharmacology.bexp --ticks 50000 --bexp-output result.bexp
```
