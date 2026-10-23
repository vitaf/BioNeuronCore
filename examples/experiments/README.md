# MindCopy Educational Experiments

## Структура

```
experiments/
├── undergraduate/     # Бакалавриат — базовые концепции
├── graduate/          # Магистратура — системная нейронаука
├── doctoral/          # Аспирантура — продвинутые темы
└── README.md          # Этот файл
```

## Бакалавриат (4 эксперимента)

| # | Эксперимент | Концепция | Ключевой механизм |
|---|------------|----------|-------------------|
| 1 | action_potential | Потенциал действия | HH Na+/K+ каналы, порог, рефрактерность |
| 2 | passive_cable | Пассивный кабель | λ и τ, затухание сигнала |
| 3 | synaptic_integration | Синаптическая интеграция | EPSP/IPSP, временная и пространственная суммация |
| 4 | lateral_inhibition | Латеральное торможение | GABA-A, контрастное усиление |

## Магистратура (8 экспериментов)

| # | Эксперимент | Концепция | Ключевой механизм |
|---|------------|----------|-------------------|
| 5 | motion_detection | Детектор движения | Асимметричное латеральное торможение, GABA-A |
| 6 | direction_selective | Direction-Selective (Barlow-Levick) | Delay-line, coincidence detection, NMDA |
| 7 | cpg_rhythm | Центральный генератор ритма | Полуцентровый осциллятор, SK/HCN, ребаунд |
| 8 | working_memory | Рабочая память | Аттракторная динамика, NMDA, рекуррентное возбуждение |
| 9 | receptive_field | Рецептивное поле | Center-surround, горизонтальные клетки |
| 10 | stdp_ltp | STDP LTP (Pre-before-Post) | CaMKII, NMDA, AMPAR trafficking |
| 11 | stdp_ltd | STDP LTD (Post-before-Pre) | Calcineurin, backpropagating AP, AMPAR endocytosis |
| 12 | stdp_learning | STDP — полное окно | LTP+LTD+BCM, Ca2+-зависимое переключение |

## Аспирантура (4 эксперимента)

| # | Эксперимент | Концепция | Ключевой механизм |
|---|------------|----------|-------------------|
| 13 | epileptiform | Эпилептиформная активность | E/I дисбаланс, PDS, KCC2, Kv7 |
| 14 | pharmacology | Фармакология каналов | Drug screening, dose-response, use-dependence |
| 15 | homeostatic_plasticity | Гомеостатическая пластичность | Synaptic scaling, TNFα, Arc, mTORC1 |
| 16 | bifurcations | Бифуркации и хаос | Hopf, SNIC, period-doubling, attractors |

## Как использовать

### В браузере
Дашборд оркестратора: `http://orchestrator:8090/`

### В Studio
Файл → Загрузить BION → загрузить сеть из шаблона.
Эксперимент → Загрузить → выбрать .bexp файл → ▶ Воспроизвести.

### Пакетно (Daemon)
```bash
biocore-daemon --batch --load-bexp <experiment>.bexp --ticks 10000 --bexp-output result.bexp
```

### Распределённо (Оркестратор)
```bash
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -d '{"bexp_url":"<path>", "name":"<name>", "duration_ticks":10000}'
```

## Формат .bexp
Каждый эксперимент — файл в формате BION Experiment (.bexp):
- JSON-заголовок: описание, пробы, стимулы, детекция
- Бинарные кадры: запись V_m во время симуляции

## STDP: индивидуально и вместе
- **stdp_ltp** — только LTP-фаза (pre-before-post, 60 пар)
- **stdp_ltd** — только LTD-фаза (post-before-pre, 60 пар)
- **stdp_learning** — полное окно: LTP + LTD + no-change + BCM threshold
Для углублённого изучения — индивидуальные. Для общей картины — объединённый.

## Биологические гарантии
- Все ионные каналы — полная кинетика Ходжкина-Хаксли/GHK
- Синапсы — реалистичная кинетика выброса/связывания/клиренса
- Пластичность — CaMKII/Calcineurin сигнальные пути
- Без суррогатов — все вычисления через полный биологический тик (core_run_tick_impl)


---
## 🇬🇧 English

# MindCopy Educational Experiments

16 biologically-realistic neural experiments for undergraduate, graduate, and doctoral neuroscience education.

### Undergraduate (4) — Basic Concepts
1. **Action Potential** — HH channels, threshold, all-or-none, refractory period
2. **Passive Cable** — λ, τ, signal decay along dendrites
3. **Synaptic Integration** — EPSP/IPSP, temporal/spatial summation
4. **Lateral Inhibition** — GABA-A, contrast enhancement

### Graduate (8) — Systems Neuroscience
5. **Motion Detection** — asymmetric lateral inhibition
6. **Direction-Selective** — Barlow-Levick delay-line
7. **CPG Rhythm** — half-center oscillator, post-inhibitory rebound
8. **Working Memory** — attractor dynamics, persistent activity
9. **Receptive Field** — center-surround, horizontal cells
10. **STDP LTP** — pre-before-post, CaMKII
11. **STDP LTD** — post-before-pre, Calcineurin
12. **STDP Learning** — complete window (LTP+LTD+BCM)

### Doctoral (4) — Advanced Topics
13. **Epileptiform** — E/I imbalance, PDS, anticonvulsant action
14. **Pharmacology** — drug screening, dose-response, use-dependence
15. **Homeostatic Plasticity** — synaptic scaling, TNFα, mTORC1
16. **Bifurcations** — Hopf, SNIC, chaos in neurons

All experiments: full biological ion channel kinetics (HH/GHK), no surrogates.

### Usage
```bash
biocore-daemon --batch --load-bexp <experiment>.bexp --ticks N --bexp-output result.bexp
```

---
## 🇨🇳 中文

# MindCopy 教育实验

16个生物学真实的神经实验，适用于本科、研究生和博士的神经科学教育。

### 本科 (4) — 基本概念
1. **动作电位** — HH通道，阈值，全或无，不应期
2. **被动电缆** — λ，τ，树突信号衰减
3. **突触整合** — EPSP/IPSP，时间/空间总和
4. **侧抑制** — GABA-A，对比度增强

### 研究生 (8) — 系统神经科学
5-12. 运动检测、方向选择、CPG节律、工作记忆、感受野、STDP LTP/LTD/完整窗口

### 博士 (4) — 高级主题
13-16. 癫痫样、药理学、稳态可塑性、分岔与混沌

所有实验：全生物离子通道动力学（HH/GHK），无替代模型。
