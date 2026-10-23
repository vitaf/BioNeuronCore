# NEURON RB — Rohon-Beard (первичный сенсорный нейрон головастика)

## Источник (литература, свежая)
- **Winlove & Roberts (2012)** "The firing patterns of spinal neurons: in situ patch-clamp recordings reveal a key role for potassium currents" — Eur J Neurosci 36:2926-2940. doi:10.1111/j.1460-9568.2012.08208.x.
- **Baccaglini & Spitzer (1977)** "Developmental changes in the inward current of the action potential of Rohon-Beard neurones" — J Physiol 271:93-117. (PMC1353609).
- **2011 J Neurophysiol** — "Pharmacology of currents underlying the different firing patterns of spinal sensory neurons and interneurons identified in vivo using multivariate analysis."

## Биология (электрофизиология)
- Первичный тактильный сенсор. **Single-spike** (не repetitive), в отличие от DL-интернейронов (repetitive ~10-20 Гц).
- **Na⁺ ток** (TTX-чувствительный, транзиентный): **2.5 pA/µm²** (vs DL 0.8 pA/µm²). НЕТ персистентного Na⁺.
- **K⁺ ток**: доминирует **I_Ks** (TEA-чувствительный медленный) — **~77%**; I_Kf (4-AP-чувствительный быстрый) = 23 ± 10%. Высокий I_Ks предотвращает repetitive firing.
- **HVA Ca²⁺**: ~1.0 pA/µm². НЕТ LVA Ca²⁺ (T-type), НЕТ Ih (HCN).
- Развитие: стадии 20-25 ток Ca²⁺, 25-40 Na⁺+Ca²⁺, 40-51 преимущественно Na⁺.

## Наша реализация (neuron_library.h — СЕЙЧАС, устаревшая)
- IONOTROPIC_NAK (HH squid) 1.0 — **НЕВЕРНО**: RB single-spike (I_Ks), не HH repetitive.
- K2P_LEAK 2.0 (CPG fix), KIR 0.4 (CPG fix) — подгон.
- PUMP 0.3, KCC2 0.5, CL 0.3, PMCA/NCX/Ca-leak/Ca-buffer.

## Сверка → реализовано (изоформа #8)
RB_NAK (тип 143) создан: та же кинетика, что dIN (din_nak_* Table 3 «dIN/RB»), проводимости RB (RB_G_NA=30 мС/см², RB_G_KFAST=0.375, RB_G_KSLOW=2.0). Ключевое отличие от DIN_NAK — доминирующий Kslow (single-spike через I_Ks, а не глубокая Na-инактивация).

## Статус
✅ RB_NAK (143) реализован (Core плагин + fallback + dispatch + Studio enum + RB в neuron_library.h). Тест test_rb_nak_sautois 9/9. (2026-08-31)
