# STDP — Long-Term Depression (Post-before-Pre)
**Уровень:** Магистратура

## Что это за эксперимент
Anti-Hebbian long-term depression: post-synaptic spike за 10ms ДО pre-synaptic spike → ослабление синапса на 20%. Индивидуальный эксперимент для глубокого изучения LTD-фазы STDP.

## Биологическая модель
Post-спайк → обратное распространение (backpropagating AP) → вход Ca2+ через voltage-gated Ca2+ каналы. Через 10ms — pre-спайк → выброс глутамата. Но Ca2+ уже частично выведен из дендрита → концентрация Ca2+ НИЖЕ порога CaMKII, но ВЫШЕ порога Calcineurin (PP2B, низкопороговая фосфатаза). Calcineurin активируется → дефосфорилирование AMPAR → эндоцитоз AMPAR из синапса → LTD.

Ключевой параметр: Δt = t_post - t_pre отрицательный, |Δt| < 30ms. При Δt=-10ms Ca2+ снижается до ~0.5μM — оптимум для Calcineurin, минимум для CaMKII.

## Как мы это сделали
**Сеть:** та же что в stdp_ltp: pre-нейрон → AMPA+NMDA синапс → post-нейрон. Разница только в порядке спайков.

**Протокол:**
1. Baseline: 10 мин (измерение веса)
2. Pairing: 60 пар с Δt=-10ms (post-first, pre-second) на 1Hz
3. Post-induction: 30 мин (вес должен остаться сниженным)

## Как воспроизвести
```bash
biocore-daemon --batch --load-bexp stdp_ltd.bexp --ticks 200000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1. Baseline: syn.weight стабилен
2. После pairing: syn.weight снижен на 20%+ (ΔW < -0.3 от начального)
3. Пост-induction: вес остаётся сниженным (стабильность LTD)
4. Без Calcineurin (cyclosporin A): LTD не индуцируется
5. Без backpropagating AP (блок L-type Ca2+ каналов): LTD не индуцируется

## Ключевые выводы
- LTD — не "слабость", а активный процесс забывания/переписывания памяти
- Ca2+ амплитуда кодирует направление пластичности: высокая → LTP, средняя → LTD, низкая → нет изменений
- Это BCM-подобное правило: порог θ_m разделяет LTP и LTD режимы
- Вместе LTP+LTD образуют полное STDP-окно (см. stdp_learning эксперимент для объединённой версии)


---
## 🇬🇧 English

### What is this experiment?
Anti-Hebbian LTD: post-spike 10ms BEFORE pre-spike → 20% synaptic depression. Moderate Ca2+ activates Calcineurin → AMPAR internalization.

### Biological mechanism
Post→bAP→Ca2+ via VGCC. Pre 10ms later→glutamate. Ca2+ partially decayed→below CaMKII but above Calcineurin threshold→Calcineurin→AMPAR dephosphorylation→endocytosis→LTD. Δt=-10ms: Ca2+~0.5μM — Calcineurin optimum.

### Verification
1) Baseline stable. 2) 60 pairs: ΔW<-0.2. 3) Persistent. 4) No Calcineurin→no LTD. 5) No bAP (L-type block)→no LTD.

### How to run
```bash
biocore-daemon --batch --load-bexp stdp_ltd.bexp --ticks 200000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
反Hebbian LTD：post锋电位比pre早10ms→突触抑制20%。中等Ca2+激活钙调磷酸酶→AMPAR内化。

### 生物机制
Post→bAP→Ca2+经VGCC。10ms后Pre→谷氨酸。Ca2+部分衰减→低于CaMKII但高于钙调磷酸酶阈值→钙调磷酸酶→AMPAR去磷酸化→内吞→LTD。Δt=-10ms：Ca2+~0.5μM——钙调磷酸酶最优。

### 验证
1) 基线稳定。2) 60对：ΔW<-0.2。3) 持久。4) 无钙调磷酸酶→无LTD。5) 无bAP→无LTD。

### 如何运行
```bash
biocore-daemon --batch --load-bexp stdp_ltd.bexp --ticks 200000 --bexp-output result.bexp
```
