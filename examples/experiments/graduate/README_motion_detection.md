# Motion Detection
**Уровень:** Магистратура

## Что это за эксперимент
Асимметричное латеральное торможение: каждый рецептор R_i ингибирует левого соседа R_{i-1} через GABA-A. Правостороннее движение (R0→R9) активирует output-нейрон, левостороннее — блокируется. Это простейший нейросетевой детектор направления.

## Биологическая модель
В сетчатке позвоночных и у беспозвоночных (limulus, дрозофила) асимметричное латеральное торможение создаёт чувствительность к направлению движения. Каждый рецептор R_i возбуждает output-нейрон И одновременно ингибирует соседа R_{i-1} через GABAergic интернейрон.

При правостороннем движении (стимул движется R0→R1→R2→...): когда стимул приходит на R_i, сосед R_{i-1} уже был активирован ранее и НЕ ингибирует R_i (его активность затухла). R_i свободно возбуждает output.

При левостороннем движении (...→R2→R1→R0): когда стимул приходит на R_i, сосед R_{i-1} АКТИВЕН (стимул только что был на нём) → он ингибирует R_i → output НЕ активируется.

## Как мы это сделали
**Сеть:** 10 рецепторных нейронов (R0-R9) в линейном массиве, каждый соединён с GABA-A синапсом на левого соседа. Output-нейрон собирает возбуждение со всех R_i.

**Протокол:**
1. Правосторонний стимул: последовательная активация R0→R9 с задержкой 10ms → output спайкует
2. Левосторонний стимул: R9→R0 → output молчит
3. Стационарный стимул: одновременно все R_i → output слабый (симметричное торможение)

## Как воспроизвести
```bash
# Локально
biocore-daemon --batch --load-bexp motion_detection.bexp --ticks 1000 --bexp-output result.bexp

# Оркестратор
curl -X POST http://orchestrator:8090/api/v1/task/create_experiment \
  -d '{"bexp_url":"motion_detection.bexp","name":"Motion Detection","duration_ticks":1000}'
```

## Что мы видим (верификация)
1. Правостороннее движение: V_m output > -40mV → спайк (DETECTED)
2. Левостороннее: V_m output ~ -65mV → нет спайка
3. Стационарный: слабая активация, недостаточная для спайка
4. Блокирование GABA-A (бикукуллин) → направленная чувствительность исчезает

## Ключевые выводы
- Асимметричное ингибирование создаёт direction selectivity без сложных вычислений
- Это один из простейших вычислительных примитивов нейронных сетей
- Тот же принцип используется в зрительной коре для detection oriented edges


---
## 🇬🇧 English

### What is this experiment?
Asymmetric lateral inhibition: each receptor R_i inhibits left neighbor R_{i-1} via GABA-A. Rightward motion activates output; leftward is blocked. Simplest neural direction detector.

### Biological mechanism
R_i excites output AND inhibits R_{i-1} via GABAergic interneuron. Rightward: when stimulus reaches R_i, R_{i-1} activity has decayed → no inhibition → output fires. Leftward: R_{i-1} still active → inhibits R_i → output silent.

### Verification
1) Rightward: V_m>-40mV→spike. 2) Leftward: V_m≈-65mV→silent. 3) Stationary: weak. 4) Block GABA-A→selectivity lost.

### How to run
```bash
biocore-daemon --batch --load-bexp motion_detection.bexp --ticks 1000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
不对称侧抑制：每个受体R_i通过GABA-A抑制左邻R_{i-1}。右向运动激活输出；左向被阻断。最简单的神经方向检测器。

### 生物机制
R_i兴奋输出并通过GABA能中间神经元抑制R_{i-1}。右向：刺激到达R_i时R_{i-1}活性已衰减→无抑制→输出放电。左向：R_{i-1}仍活跃→抑制R_i→输出沉默。

### 验证
1) 右向：V_m>-40mV→锋电位。2) 左向：V_m≈-65mV→沉默。3) 静止：弱响应。4) 阻断GABA-A→选择性丧失。

### 如何运行
```bash
biocore-daemon --batch --load-bexp motion_detection.bexp --ticks 1000 --bexp-output result.bexp
```
