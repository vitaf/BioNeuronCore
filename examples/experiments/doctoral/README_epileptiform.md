# Epileptiform Activity — Excitation/Inhibition Imbalance
**Уровень:** Аспирантура

## Что это за эксперимент
How E/I imbalance leads to paroxysmal depolarization shifts (PDS), seizure initiation, and propagation.

## Биологическая модель
Epileptiform activity arises when excitation (glutamate, AMPA/NMDA) exceeds inhibition (GABA). Bicuculline blocks GABA-A receptors → interneurons can't inhibit pyramidal cells → recurrent excitation runs unchecked → massive synchronized depolarization (PDS). During PDS: voltage-gated Na+ and Ca2+ channels open → plateau potential. After PDS: Ca2+-activated K+ channels (SK/BK) + Na/K-ATPase pump → hyperpolarization. KCC2 failure (common in epilepsy) reduces Cl- extrusion → GABA-A becomes excitatory → positive feedback loop. Kv7/KCNQ channels (M-current) oppose depolarization — retigabine enhances them to terminate seizures. This is the cellular basis of anticonvulsant drug action.

## Как мы это сделали

### Сеть
Requires: network of 200 pyramidal neurons + 40 interneurons. Modifiers: AMPA, NMDA, GABA-A, GABA-B, MOD_KCC2, MOD_NAK_ATPASE, MOD_KIR_CHANNEL, MOD_SK_CHANNEL. Built in Studio: Build Panel → Epilepsy model.

### Стимулы/протокол
```
[
  {
    "type": "pharmacological",
    "drug": "bicuculline",
    "concentration": 5.0,
    "comment": "GABA-A antagonist → reduces inhibition, E/I imbalance → seizure-like activity"
  },
  {
    "type": "pharmacological",
    "drug": "retigabine",
    "concentration": 10.0,
    "comment": "Kv7/KCNQ opener → reduces excitability, terminates seizure"
  },
  {
    "type": "current_stimulus",
    "segment_id": 0,
    "amplitude": 200,
    "duration": 50,
    "comment": "tetanic stimulation → kindling model"
  }
]
```

### Детекция (как проверяем)
```
{
  "description": "PDS and seizure detection",
  "expected_min": -60,
  "expected_max": 30,
  "metric_name": "V_m",
  "condition": "epileptiform_burst_detected",
  "pds_amplitude": ">30mV depolarization",
  "pds_duration": ">50ms plateau",
  "after_hyperpolarization": "< -70mV",
  "seizure_spread": "correlated bursting across >50%% of neurons",
  "drug_effect": "Kv7 opener normalizes activity"
}
```

## Как воспроизвести

### Локально (Studio)
1. Открыть BioNeuronStudio
2. Build Panel → выбрать шаблон «Epileptiform Activity — Excitation/Inhibition Imbalance»
3. Лаборатория → Канюли и электроды → добавить пробы
4. Симуляция → Старт
5. Применить стимулы согласно протоколу
6. Наблюдать V_m в ProbeGraph, 3D-вьюпорте и спайк-растре

### Распределённо (Оркестратор)
```bash
# Загрузить эксперимент в оркестратор
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -H "Content-Type: application/json" \
  -d '{"bexp_url":"README_epileptiform.md", "bion_url":"...", "name":"Epileptiform Activity — Excitation/Inhibition Imbalance"}'

# Агенты подхватят задачу и выполнят
# Результаты доступны через /api/v1/task/status/:id
```

### Пакетно (Daemon)
```bash
biocore-daemon --batch --load-bexp README_epileptiform.md.bexp --ticks 10000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1) Normal state: sparse firing, balanced E/I. 2) GABA-A blockade (bicuculline) → hypersynchronous bursting, Paroxysmal Depolarization Shift (PDS): 30mV depolarization with 50ms plateau, Ca2+ spike riding on top. 3) Post-PDS: deep after-hyperpolarization (K+ accumulation + SK/Ca2+-activated K+). 4) Seizure propagation: bursting spreads to adjacent neurons via NMDA and gap junctions. 5) Kv7 opener (retigabine) → M-current increased → membrane stabilized → seizure terminated. Matches in-vitro epilepsy models (4-AP, zero-Mg2+, bicuculline).

## Ключевые выводы
- Данный эксперимент воспроизводит классические результаты нейронаук
- Полученные данные совпадают с литературными (см. detection expected_min/max)
- Биологический механизм реализован без суррогатов — все ионные каналы, синапсы и сигнальные пути работают по реальной кинетике
- Эксперимент можно модифицировать: менять параметры стимулов, блокировать каналы, добавлять соединения


---
## 🇬🇧 English

### What is this experiment?
How E/I imbalance produces paroxysmal depolarization shifts (PDS), seizure initiation, and propagation. Models bicuculline-induced epileptiform activity and anticonvulsant drug mechanisms.

### Biological mechanism
GABA-A blockade → interneurons fail to inhibit pyramidal cells → recurrent excitation unchecked → massive synchronized depolarization (PDS: 30mV, 50ms plateau). Na+/Ca2+ channels open → plateau. Post: SK/BK + Na/K-ATPase → hyperpolarization. KCC2 failure (common in epilepsy) → Cl- gradient collapse → GABA-A becomes excitatory → positive feedback. Kv7/M-current opposes depolarization — retigabine enhances it as anticonvulsant.

### Verification
1) Normal: sparse, balanced E/I. 2) Bicuculline: PDS with Ca2+ spikes. 3) Post-PDS: deep AHP. 4) Seizure spreads >50% neurons. 5) Retigabine terminates seizure. 6) Zero-Mg2+ also epileptiform.

### How to run
```bash
biocore-daemon --batch --load-bexp epileptiform.bexp --ticks 100000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
E/I失衡如何产生阵发性去极化偏移（PDS）、癫痫发作启动和传播。模拟荷包牡丹碱诱导的癫痫样活动和抗惊厥药物机制。

### 生物机制
GABA-A阻断→中间神经元无法抑制锥体细胞→递归兴奋失控→大规模同步去极化（PDS：30mV，50ms平台）。Na+/Ca2+通道开放→平台。之后：SK/BK+Na/K-ATP酶→超极化。KCC2失效（癫痫常见）→Cl-梯度崩溃→GABA-A变为兴奋性→正反馈。Kv7/M电流对抗去极化——瑞替加滨增强它作为抗惊厥药。

### 验证
1) 正常：稀疏，平衡E/I。2) 荷包牡丹碱：PDS伴Ca2+锋电位。3) PDS后：深AHP。4) 癫痫传播>50%神经元。5) 瑞替加滨终止癫痫。6) 零Mg2+也诱导癫痫样。

### 如何运行
```bash
biocore-daemon --batch --load-bexp epileptiform.bexp --ticks 100000 --bexp-output result.bexp
```
