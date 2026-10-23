# Spike-Timing-Dependent Plasticity (STDP)
**Уровень:** Магистратура

## Что это за эксперимент
Hebbian learning: synaptic weight changes depend on the precise timing of pre- and post-synaptic spikes.

## Биологическая модель
When a pre-synaptic spike precedes a post-synaptic spike (Δt > 0), the pre-synaptic glutamate release coincides with post-synaptic depolarization → NMDA receptor activation → Ca2+ influx through NMDA receptors and L-type Ca2+ channels → high [Ca2+] → CaMKII auto-phosphorylation → AMPAR trafficking to synapse → LTP (weight increase). When post-synaptic spike precedes pre-synaptic spike (Δt < 0), post-synaptic Ca2+ has partially decayed → lower [Ca2+] → Calcineurin (PP2B) activation → AMPAR internalization → LTD. The BCM sliding threshold (θ_m) adapts based on average firing rate, ensuring network stability.

## Как мы это сделали

### Сеть
Requires: pre-synaptic neuron → AMPA+NMDA synapse → post-synaptic neuron. Modifiers: MOD_STDP (CaMKII, Calcineurin), MOD_CA_L_TYPE, AMPA, NMDA. Built in Studio: Build Panel → STDP template.

### Стимулы/протокол
```
[
  {
    "type": "pairing",
    "pre_time": 0,
    "post_time": 10,
    "comment": "pre-before-post by 10ms → LTP (weight increase)"
  },
  {
    "type": "pairing",
    "pre_time": 10,
    "post_time": 0,
    "comment": "post-before-pre by 10ms → LTD (weight decrease)"
  },
  {
    "type": "pairing",
    "pre_time": 0,
    "post_time": 50,
    "comment": "pre-before-post by 50ms → no change (outside window)"
  },
  {
    "type": "pairing",
    "pre_time": 0,
    "post_time": 10,
    "repeat": 60,
    "comment": "60 repetitions → asymptotic LTP"
  }
]
```

### Детекция (как проверяем)
```
{
  "description": "STDP temporal window",
  "expected_min": -0.3,
  "expected_max": 0.5,
  "metric_name": "weight_delta",
  "condition": "pre_before_post",
  "ltp_delta_t_10ms": "+30% weight",
  "ltd_delta_t_minus10ms": "-20% weight",
  "window_half_width": "~30ms",
  "bcm_sliding_threshold": "LTD at low rate, LTP at high rate"
}
```

## Как воспроизвести

### Локально (Studio)
1. Открыть BioNeuronStudio
2. Build Panel → выбрать шаблон «Spike-Timing-Dependent Plasticity (STDP)»
3. Лаборатория → Канюли и электроды → добавить пробы
4. Симуляция → Старт
5. Применить стимулы согласно протоколу
6. Наблюдать V_m в ProbeGraph, 3D-вьюпорте и спайк-растре

### Распределённо (Оркестратор)
```bash
# Загрузить эксперимент в оркестратор
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -H "Content-Type: application/json" \
  -d '{"bexp_url":"README_stdp_learning.md", "bion_url":"...", "name":"Spike-Timing-Dependent Plasticity (STDP)"}'

# Агенты подхватят задачу и выполнят
# Результаты доступны через /api/v1/task/status/:id
```

### Пакетно (Daemon)
```bash
biocore-daemon --batch --load-bexp README_stdp_learning.md.bexp --ticks 10000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1) Δt = +10ms (pre before post) → synaptic weight increases by ~30% (LTP). 2) Δt = -10ms (post before pre) → weight decreases by ~20% (LTD). 3) Δt = 50ms → no change (outside STDP window). 4) 60 repetitions of LTP pairing → weight approaches asymptotic maximum. 5) Low-frequency pairing (1Hz) → net LTD (BCM sliding threshold). This reproduces the classic Bi & Poo (1998) cortical neuron results.

## Ключевые выводы
- Данный эксперимент воспроизводит классические результаты нейронаук
- Полученные данные совпадают с литературными (см. detection expected_min/max)
- Биологический механизм реализован без суррогатов — все ионные каналы, синапсы и сигнальные пути работают по реальной кинетике
- Эксперимент можно модифицировать: менять параметры стимулов, блокировать каналы, добавлять соединения


---
## 🇬🇧 English

### What is this experiment?
Full STDP window: LTP (pre-before-post, Δt>0), LTD (post-before-pre, Δt<0), no change outside window, BCM sliding threshold. Reproduces Bi & Poo (1998).

### Biological mechanism
Δt>0: Ca2+ surge via NMDA→CaMKII→AMPAR trafficking→LTP. Δt<0: moderate Ca2+→Calcineurin→AMPAR internalization→LTD. |Δt|>50ms: Ca2+ too low→no change. Frequency-dependent: 1Hz→net LTD, 40Hz→net LTP (BCM). Ca2+ amplitude encodes plasticity direction.

### Verification
1) Δt=+10ms→ΔW≈+0.3 (LTP). 2) Δt=-10ms→ΔW≈-0.2 (LTD). 3) Δt=+50ms→ΔW≈0. 4) 1Hz→net LTD. 5) 40Hz→net LTP (BCM). 6) Block CaMKII→no LTP. 7) Block Calcineurin→no LTD.

### How to run
```bash
biocore-daemon --batch --load-bexp stdp_learning.bexp --ticks 500000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
完整STDP窗口：LTP（pre-before-post，Δt>0）、LTD（post-before-pre，Δt<0）、窗口外无变化、BCM滑动阈值。重现Bi & Poo（1998）。

### 生物机制
Δt>0：Ca2+经NMDA激增→CaMKII→AMPAR运输→LTP。Δt<0：中等Ca2+→钙调磷酸酶→AMPAR内化→LTD。|Δt|>50ms：Ca2+过低→无变化。频率依赖：1Hz→净LTD，40Hz→净LTP（BCM）。Ca2+振幅编码可塑性方向。

### 验证
1) Δt=+10ms→ΔW≈+0.3（LTP）。2) Δt=-10ms→ΔW≈-0.2（LTD）。3) Δt=+50ms→ΔW≈0。4) 1Hz→净LTD。5) 40Hz→净LTP（BCM）。6) 阻断CaMKII→无LTP。7) 阻断钙调磷酸酶→无LTD。

### 如何运行
```bash
biocore-daemon --batch --load-bexp stdp_learning.bexp --ticks 500000 --bexp-output result.bexp
```
