# Direction-Selective Circuit (Barlow-Levick)
**Уровень:** Магистратура

## Что это за эксперимент
Barlow-Levick delay-line детектор направления. R_{i-1}→delay→output (медленный путь), R_i→output (быстрый). Совпадение сигналов происходит только при правостороннем движении с интервалом ~9ms. Классическая модель 1965 года, подтверждённая в сетчатке кролика.

## Биологическая модель
Стимул, движущийся слева направо, сначала активирует R_{i-1}, затем через 9ms R_i. Сигнал от R_{i-1} проходит через медленный путь (длинный дендрит или медленный синапс) и приходит на output одновременно с быстрым сигналом от R_i. Совпадение → output активируется.

При левостороннем движении R_i активируется РАНЬШЕ R_{i-1} → быстрый сигнал от R_i уже затух, а медленный от R_{i-1} ещё не пришёл → совпадения нет → output молчит.

Требуемая задержка ~9ms соответствует расстоянию между рецепторами в 100μm при скорости проведения 10μm/ms.

## Как мы это сделали
**Сеть:** пары рецепторов (R_i, R_{i+1}). R_{i-1} соединяется с output через медленный путь (delay 9ms, AMPA+NMDA). R_i соединяется напрямую.

**Протокол:**
1. Правосторонний стимул: R0 в t=0, R1 в t=9ms → совпадение → output active
2. Левосторонний: R1 в t=0, R0 в t=9ms → нет совпадения
3. Интервал 20ms (вместо 9ms) → нет совпадения (селективность к скорости)

## Как воспроизвести
```bash
biocore-daemon --batch --load-bexp direction_selective.bexp --ticks 1000 --bexp-output result.bexp
```

## Что мы видим (верификация)
1. Δt=9ms правостороннее: output V_m > -35mV → DETECTED
2. Δt=9ms левостороннее: V_m ~ -65mV → нет ответа
3. Δt=20ms: расхождение → нет ответа (скоростная селективность)
4. Блокирование NMDA → чувствительность к интервалу исчезает (NMDA обеспечивает временное окно)

## Ключевые выводы
- Coincidence detection — фундаментальный механизм временного кодирования
- Barlow-Levick модель объясняет direction selectivity в retina без необходимости motion computation
- Тот же принцип (delay-line) используется в auditory brainstem для interaural time difference (Jeffress model)


---
## 🇬🇧 English

### What is this experiment?
Barlow-Levick delay-line: R_{i-1}→delay→output (slow), R_i→output (fast). Coincidence only for rightward motion at ~9ms. Classic 1965 model, confirmed in rabbit retina.

### Biological mechanism
Left→right stimulus: R_{i-1} then R_i 9ms later. Slow path from R_{i-1} arrives simultaneously with fast path from R_i → coincidence → output fires. Leftward: R_i fires before R_{i-1} → fast signal decayed, slow not arrived → no coincidence. 9ms delay ≈ 100μm spacing at 10μm/ms.

### Verification
1) Δt=9ms rightward: detected. 2) Δt=9ms leftward: silent. 3) Δt=20ms: no detection (speed-tuned). 4) Block NMDA→temporal window lost.

### How to run
```bash
biocore-daemon --batch --load-bexp direction_selective.bexp --ticks 1000 --bexp-output result.bexp
```

---
## 🇨🇳 中文

### 这是什么实验？
Barlow-Levick延迟线：R_{i-1}→延迟→输出（慢），R_i→输出（快）。仅~9ms右向运动重合。1965年经典模型，兔视网膜证实。

### 生物机制
左→右刺激：先R_{i-1}，9ms后R_i。R_{i-1}的慢路径与R_i的快路径同时到达→重合→输出放电。左向：R_i先于R_{i-1}放电→快信号衰减，慢未到→无重合。9ms延迟≈100μm间距，10μm/ms传导。

### 验证
1) Δt=9ms右向：检测到。2) Δt=9ms左向：沉默。3) Δt=20ms：无检测（速度调谐）。4) 阻断NMDA→时间窗口消失。

### 如何运行
```bash
biocore-daemon --batch --load-bexp direction_selective.bexp --ticks 1000 --bexp-output result.bexp
```
