# MOD_KV11 — Kv1.1 (KCNA1) delayed-rectifier K⁺-канал

**Тип:** MOD_KV11 — новая изоформа (AIS-канал, задача #23).

## Литература
- **γ = 9–14 пС** (Grissmer 1994; Table 10 пС). Источник: [PubMed 7816560](https://pubmed.ncbi.nlm.nih.gov/7816560/), [PMC2838721 Table 1](https://pmc.ncbi.nlm.nih.gov/articles/PMC2838721/table/T1/).
- **Активация V½ ≈ −30 мВ** (τ_акт ~5 мс при −34 мВ) — delayed rectifier, быстрая активация.
- **Без быстрой инактивации** — медленная неполная V-зависимая инактивация τ 2–4 с (single time constant).
- Локализация: AIS (Nav1.2+Kv1.1/Kv1.2 — Tian 2014), сома/аксон.

## Молекулярные константы
- γ = 10 пС (средн.); V½ ≈ −30 мВ; k ≈ 6 мВ; τ_m ≈ 5 мс.
- m⁴ (нет быстрой h-инактивации); GHK K⁺ (z=1).
- Источник: Grissmer et al. 1994, Pflügers Arch 428:382, [DOI 10.1007/BF00724522](https://doi.org/10.1007/BF00724522).

## Применение
AIS (вместо generic KV_DR), мозжечок, basket PV (Kv1.x).

## Связанные
[[modifier-audit-method]], [[neuron-density-po]]
