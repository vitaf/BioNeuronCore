# Homeostatic Synaptic Plasticity
**Уровень:** Аспирантура

## Что это за эксперимент
Synaptic scaling: how neurons maintain stable firing rates despite chronic activity changes.

## Биологическая модель
Neurons sense their activity level through Ca2+ signals (L-type channels, NMDA receptors). Low activity → low [Ca2+] → CaMKIV/CREB activation reduced → retinoic acid synthesis → TNFα release from glia → AMPAR insertion at synapses (upscaling). High activity → high [Ca2+] → Arc/Arg3.1 expression → AMPAR endocytosis (downscaling). mTORC1 pathway regulates protein synthesis needed for scaling. Unlike STDP (which changes specific synapses based on timing), scaling is global and multiplicative — it preserves the relative strengths of synapses. This provides stability: if the network is too quiet, scaling excites it; if too active, scaling inhibits it. Homeostatic plasticity prevents epileptiform runaway and maintains network function over years.

## Как мы это сделали

### Сеть
Requires: 20 pyramidal neurons with recurrent connections. Modifiers: AMPA, NMDA, MOD_CA_L_TYPE, MOD_CREB, MOD_MAPK_ERK, MOD_MTORC1, MOD_TNF_ALPHA. Built in Studio: Build Panel → Homeostasis template.

### Стимулы/протокол
```
[
  {
    "type": "chronic_blockade",
    "drug": "TTX",
    "duration": "24h_simulated",
    "comment": "block Na+ channels → silence network → triggers synaptic upscaling"
  },
  {
    "type": "chronic_blockade",
    "drug": "bicuculline",
    "duration": "24h_simulated",
    "comment": "block inhibition → hyperactivity → triggers synaptic downscaling"
  },
  {
    "type": "molecular",
    "pathway": "TNFα_block",
    "comment": "block TNFα signaling → prevents upscaling (molecular mechanism)"
  }
]
```

### Детекция (как проверяем)
```
{
  "description": "Synaptic scaling up/down",
  "expected_min": 0.01,
  "expected_max": 0.5,
  "metric_name": "synaptic_weight_mean",
  "condition": "scaling_detected",
  "ttx_48h": "AMPA mEPSC amplitude +40%",
  "bic_48h": "AMPA mEPSC amplitude -30%",
  "tnf_alpha_block": "no upscaling with TTX+anti-TNF",
  "multiplicative": "weight distribution scaled, not shifted",
  "stdp_intact": "individual synapse STDP still works on top of scaling"
}
```

## Как воспроизвести

### Локально (Studio)
1. Открыть BioNeuronStudio
2. Build Panel → выбрать шаблон «Homeostatic Synaptic Plasticity»
3. Лаборатория → Канюли и электроды → добавить пробы
4. Симуляция → Старт
5. Применить стимулы согласно протоколу
6. Наблюдать V_m в ProbeGraph, 3D-вьюпорте и спайк-растре

### Распределённо (Оркестратор)
```bash
# Загрузить эксперимент в оркестратор
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -H "Content-Type: application/json" \
  -d '{"bexp_url":"README_homeostatic_plasticity.md", "bion_url":"...", "name":"Homeostatic Synaptic Plasticity"}'

# Агенты подхватят задачу и выполнят
# Результаты доступны через /api/v1/task/status/:id
```

### Пакетно (Daemon)
```bash
biocore-daemon --batch --load-bexp README_homeostatic_plasticity.md.bexp --ticks 10000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1) 24h TTX blockade → mean mEPSC amplitude increases by 40% (synaptic upscaling). 2) 24h bicuculline → mean mEPSC amplitude decreases by 30% (synaptic downscaling). 3) TTX + anti-TNFα → no upscaling (TNFα is a key glial signal). 4) Scaling is multiplicative: the entire weight distribution is scaled, preserving relative weight differences. 5) STDP still operates on individual synapses — scaling sets the 'operating point', STDP provides specificity. This reproduces Turrigiano & Nelson (2004) synaptic scaling results.

## Ключевые выводы
- Данный эксперимент воспроизводит классические результаты нейронаук
- Полученные данные совпадают с литературными (см. detection expected_min/max)
- Биологический механизм реализован без суррогатов — все ионные каналы, синапсы и сигнальные пути работают по реальной кинетике
- Эксперимент можно модифицировать: менять параметры стимулов, блокировать каналы, добавлять соединения


---
## 🇬🇧 English

### What is this experiment?
Synaptic scaling: how neurons maintain stable firing through global multiplicative AMPAR trafficking. TNFα, Arc/Arg3.1, mTORC1 signaling. Reproduces Turrigiano & Nelson (2004).

### Biological mechanism
Low activity→low Ca2+→reduced CaMKIV/CREB→retinoic acid→TNFα from glia→AMPAR insertion (upscaling). High activity→high Ca2+→Arc/Arg3.1→AMPAR endocytosis (downscaling). mTORC1 regulates protein synthesis. Scaling is multiplicative: preserves relative weights unlike STDP. Provides stability — prevents epileptiform runaway over years.

### Verification
1) TTX 24h→mEPSC +40% (upscaling). 2) Bicuculline 24h→-30% (downscaling). 3) TTX+anti-TNFα→no upscaling. 4) Multiplicative scaling, not shift. 5) STDP still works on top — scaling sets operating point.

### How to run
```bash
biocore-daemon --batch --load-bexp homeostatic_plasticity.bexp --ticks 200000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
突触缩放：神经元如何通过全局乘法性AMPAR运输维持稳定放电。TNFα、Arc/Arg3.1、mTORC1信号。重现Turrigiano & Nelson（2004）。

### 生物机制
低活动→低Ca2+→减少CaMKIV/CREB→视黄酸→胶质细胞TNFα→AMPAR插入（上调）。高活动→高Ca2+→Arc/Arg3.1→AMPAR内吞（下调）。mTORC1调节蛋白质合成。缩放是乘法性的：保留相对权重不像STDP。提供稳定性——防止癫痫样失控，维持多年。

### 验证
1) TTX 24h→mEPSC+40%（上调）。2) 荷包牡丹碱24h→-30%（下调）。3) TTX+抗TNFα→无上调。4) 乘法缩放，非平移。5) STDP仍在上层工作——缩放设定操作点。

### 如何运行
```bash
biocore-daemon --batch --load-bexp homeostatic_plasticity.bexp --ticks 200000 --bexp-output result.bexp
```
