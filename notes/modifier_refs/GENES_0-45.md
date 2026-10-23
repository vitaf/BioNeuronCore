# Гены (генный пайплайн) — GENE_0..45 (46 генов)

Аудит #15. Дата: 2026-08-21. Метод: [[modifier-audit-method]].

## Природа

Гены — генетический уровень экспрессии (мРНК → белок → density модификатора).
Пайплайн в `consolidation.cpp`:
1. **Транскрипция (GRN)**: TF (LHX3/ISL1/PAX2/BRN3A/MNX1/LMX1B) + pCREB → `gene_activity[46]`.
2. **Трансляция**: `gene_activity → protein_level[46]` (через protein_synthesis_rate).
3. **Экспрессия**: `protein_level → density модификатора` (gene_to_mod[46]).

## Маппинг ген → модификатор (gene_to_mod)

| Гены | Белок | Модификатор | Статус |
|------|-------|-------------|--------|
| 26 SCN1A | Nav1.1 | 199 (MOD_NAV11) | ✓ (обновлено 2026-10-22: был generic HH) |
| 27 KCND2 | Kv4.2 | 85 (MOD_KV42) | ✓ |
| 28 KCNQ2 | Kv7.2 | 112 (MOD_KV7_MCURRENT) | ✓ |
| 29 KCNQ3 | Kv7.3 | 112 | ✓ |
| 30 CACNA1G | Cav3.1 | 200 (MOD_CAV31) | ✓ (обновлено 2026-10-22: был 96 PIR T-Ca) |
| 31 HCN1 | Ih (HCN1) | 129 (MOD_HCN1) | ✓ (ИСПРАВЛЕНО: было 97=α7) |
| 32 HCN2 | Ih (HCN2) | 130 (MOD_HCN2) | ✓ (ИСПРАВЛЕНО: было 97=α7) |
| 33 KCNJ2 | Kir2.1 | 106 (MOD_KIR_CHANNEL) | ✓ |
| 34 KCNK2 | TREK-1 | 107 (MOD_K2P_LEAK) | ✓ |
| 35 KCNMA1 | BK | 104 (MOD_BK_CHANNEL) | ✓ |
| 45 KCNT1 | Slack | 121 (MOD_KNA_CHANNEL) | ✓ |
| 0-25, 36-44 | plasticity/TF/vesicular | -1 (нет прямого маппинга) | ✓ (через RRP/структурные события) |

## Диагноз

1. **БАГ (исправлен 2026-08-21)**: HCN1 (31) и HCN2 (32) маппились на 97
   (`MOD_NICOTINIC_ALPHA7`, бывш. PIR_HCN). Партия #5 переиспользовала ID 97 под α7,
   но `gene_to_mod` не обновился. → 129 (MOD_HCN1) / 130 (MOD_HCN2).
2. **SCN1A (26) → 199 (MOD_NAV11)**: обновлено 2026-10-22 (#8) — Nav1.1 теперь
   собственный модификатор (γ=17 пС, V½ −30/−65, repetitive), не generic HH.
3. **Веса GRN** (0.4/0.3/0.15, скорости 0.005/0.003) — mean-field относительные
   веса регуляции (не литературные константы), задокументированы.

## Границы

Меняется ТОЛЬКО `gene_to_mod` (баг) + справочный .md + тесты. Структуры `Nucleus`,
`gene_activity/protein_level`, BION — НЕ трогаются.
