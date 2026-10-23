# STDP — Long-Term Potentiation (Pre-before-Post)
**Уровень:** Магистратура

## Что это за эксперимент
Hebbian long-term potentiation: pre-synaptic spike за 10ms до post-synaptic spike → усиление синапса на 30%+. Индивидуальный эксперимент для глубокого изучения LTP-фазы STDP.

## Биологическая модель
Pre-спайк → выброс глутамата → активация AMPA/NMDA рецепторов. Post-спайк через 10ms → деполяризация пост-нейрона → снятие Mg2+-блока с NMDA → массивный вход Ca2+ → активация CaMKII (высокопороговая киназа) → автофосфорилирование CaMKII (долговременная активация) → фосфорилирование AMPAR (увеличение проводимости) + экзоцитоз новых AMPAR в синапс → LTP.

Ключевой временной параметр: Δt = t_post - t_pre должен быть положительным и < 30ms (STDP-окно). При Δt=10ms Ca2+ достигает пика точно в момент максимальной чувствительности CaMKII.

## Как мы это сделали
**Сеть:** pre-нейрон → AMPA+NMDA синапс → post-нейрон. Модификаторы: MOD_CAMKII (82), MOD_CALCINEURIN (83), MOD_CA_L_TYPE (84), STDP-weight update в Core.

**Протокол:**
1. Baseline: 10 мин спонтанной активности (измерение начального веса)
2. Pairing: 60 пар pre-post с Δt=10ms на частоте 1Hz
3. Post-induction: 30 мин наблюдения (вес должен остаться повышенным)

## Как воспроизвести
```bash
biocore-daemon --batch --load-bexp stdp_ltp.bexp --ticks 200000 --bexp-output result.bexp
```
(200K тиков ≈ 20 сек симуляции при dt=0.1ms, покрывает baseline+pairing+post)

## Что мы видим (верификация)
1. Baseline: syn.weight стабилен
2. После pairing: syn.weight увеличен на 30%+ (ΔW > 0.5 от начального)
3. Пост-induction: вес остаётся повышенным > 30 мин (стабильность LTP)
4. Без NMDA (AP5): LTP не индуцируется → NMDA обязателен
5. Без CaMKII (KN-93): потенциация слабая и временная → CaMKII обязателен для долговременности

## Ключевые выводы
- LTP — клеточный коррелят памяти (Hebb, 1949: "neurons that fire together, wire together")
- Ca2+ через NMDA — coincidence detector
- CaMKII — молекулярный переключатель памяти (автофосфорилирование = bistable switch)
- Это одна фаза STDP; вторая (LTD) — см. stdp_ltd эксперимент


---
## 🇬🇧 English

### What is this experiment?
Hebbian LTP: pre-spike 10ms before post-spike → 30%+ synaptic potentiation. CaMKII auto-phosphorylation as bistable molecular memory switch.

### Biological mechanism
Pre→glutamate→AMPA/NMDA. Post 10ms later→depolarization→Mg2+ unblock→Ca2+ entry→CaMKII→auto-phosphorylation→AMPAR phosphorylation+exocytosis→LTP. Δt=+10ms optimal: Ca2+ peak matches CaMKII sensitivity.

### Verification
1) Baseline stable. 2) 60 pairs: ΔW>+0.3. 3) >30min persistence. 4) No NMDA→no LTP. 5) No CaMKII→weak transient only.

### How to run
```bash
biocore-daemon --batch --load-bexp stdp_ltp.bexp --ticks 200000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
Hebbian LTP：pre锋电位比post早10ms→突触增强30%+。CaMKII自身磷酸化作为双稳态分子记忆开关。

### 生物机制
Pre→谷氨酸→AMPA/NMDA。10ms后Post→去极化→Mg2+解除→Ca2+进入→CaMKII→自身磷酸化→AMPAR磷酸化+外排→LTP。Δt=+10ms最优：Ca2+峰值匹配CaMKII敏感性。

### 验证
1) 基线稳定。2) 60对：ΔW>+0.3。3) >30分钟持续。4) 无NMDA→无LTP。5) 无CaMKII→仅弱短暂。

### 如何运行
```bash
biocore-daemon --batch --load-bexp stdp_ltp.bexp --ticks 200000 --bexp-output result.bexp
```
