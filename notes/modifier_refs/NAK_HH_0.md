# NAK_HH (0) — Na/K канал Ходжкина-Хаксли (vertebrate)

**Тип:** MOD_IONOTROPIC_NAK (0). Базовый модификатор — механизм спайка.
**Роль:** потенциал-зависимые Na⁺ (m³h) и K⁺ (n⁴) каналы, генерация ПД.
**Использование:** базовый (в 222-клеточной библиотеке, НЕ в CPG-ядре — dIN использует DIN_NAK 125).

## Литература

- **Hodgkin & Huxley 1952** — классические gNa=120, gK=36 мС/см² (squid).
- **Mainen & Sejnowski 1996**, *Nature* 382:363 — vertebrate (cortical) Na/K, V-зависимые τ.
- Код-комментарий: «Squid HH — призрак, заменён (Mainen & Sejnowski 1996; Li/Roberts/Soffe 2010)».

## Честная модель (одномолекулярная GHK, рефакторинг 2026-09-21)

- **GHK**: I_Na = P_single_Na·flux_Na·(density·area)·m³h, I_K = P_single_K·flux_K·(density·area)·n⁴.
  γ_Na=20.8 пС (Nav1.2 Ben-Shalom 2017), γ_K=18 пС (delayed rectifier, 17–19 mammalian).
  density_Na=86.05/µm², density_K=55.35/µm² (из gNa=120/gK=36 через chord-conductance).
  Ток Na ×2.68, K ×3.7 против Ohm-суррогата.
- **Гейтинг**: Boltzmann/HH α/β m_inf/h_inf/n_inf (V½=−35/−65/−10) — ✅.
- **V-зависимые τ** (HH α/β, Mainen & Sejnowski 1996) — ✅.

## Найденные проблемы (сверка честности)

1. **Объём plugin πr²L/20 vs fallback segment_volume** — рассинхрон (сома 4/3πr³). → segment_volume.
2. **Фикс. τ** (0.2/2.0/2.0 мс) — не V-зависимые. DIN_NAK (125) уже использует V-зависимые
   HH α/β (hh_rate). → V-зависимые τ по Mainen & Sejnowski.
3. **gK**: код 0.05 (50 мС/см²), комментарий «gK=36 мС/см²» (0.036) — расхождение.
   gNa=0.12 (120 мС/см²) — squid-значение (комментарий «заменён»).
