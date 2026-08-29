# Run 12 QC findings (RUN_LUAG_5GeV, 25,000 events)

Produced by `macros/RunQC.C` and `macros/BeamCheck.C` on 2026-08-28.
Plots: `RunQC.pdf`, `example_waveforms.png`, `BeamCheck_profiles.png`,
`BeamCheck_amplitudes.png`. Numbers: `RunQC_summary.txt`,
`BeamCheck_summary.txt`.

Digitizer is effectively 12-bit (0–4095); SiPM baselines sit at ~2048
mid-range. Channel map per James (2026-08-28):

| ch | role | as seen in run 12 |
|---|---|---|
| 0, 1 | Cherenkov counters (event selection) | Negative pulses ~300 ADC peaking at samples ~460–490, quiet baselines (RMS ~4). Coincidence (both amp > 150): **2,466 / 25,000 events (9.9%)**. |
| 2 | Scintillator counter | 89% occupancy, negative pulses peaking at ~540, spectrum peaks ~700 ADC. **1.3% of events clip the bottom rail** (pile-up spike at amplitude ~2050). |
| 3 | nothing | Quiet, no signal, as expected. |
| 4–7 | Low gain upstream | Slow positive pulse: starts ~sample 90, peaks ~430, still ~45–50% of peak amplitude at the record end. Mean beam amplitude 186–246 ADC, max ~1,400. **Zero saturation.** |
| 8–11 | nothing | Quiet, no signal, as expected. |
| 12–15 | High gain upstream | Fast positive spike at ~sample 100 (mean beam amplitude 630–840 ADC) followed by a shallow undershoot (~−30 to −40 ADC) that persists to the record end. Beam-event saturation: **ch 12 2.6%, ch 13 1.2%, ch 14 12.5%, ch 15 7.5%.** |
| 16, 17 | MCP group 0/1 (TR0, CAEN DT5742) | Sharp negative pulses at ~sample 100, in time with the upstream module, identical in beam and off-beam events (every stored trigger has an MCP pulse — the DAQ really is triggering on TR0). Mean amplitude ~1,490 ADC with a sharp turn-on at ~1,000 ADC (the effective trigger threshold). **Bottom-rail clipping in 4.0% (ch 16) / 2.5% (ch 17) of beam events.** |

## 1. SiPM bias / gain verdict

- **Low gain upstream (4–7): good.** No saturation at all in electron-selected
  events, comfortable amplitudes. No change indicated.
- **High gain upstream (12–15): clipping at 5 GeV.** In Cherenkov-selected
  events, 1–12% of pulses hit the ADC rails (worst: ch 14 at 12.5%, ch 15 at
  7.5%). If the high-gain chain is meant to be usable at 5 GeV, back off the
  bias/gain or DC offsets; if it is intentionally scaled for low-energy
  sensitivity and 5 GeV analysis uses the low-gain chain, this is by design —
  but ch 14/15 clip noticeably more than ch 12/13, so the four channels are
  not gain-matched.
- **Scintillator (ch 2): 1.3% clipping.** Baseline at mid-range 2048 leaves
  only ~2048 counts below for its negative pulses. Raise the DC offset
  (baseline toward ~3300–3500) if you want its amplitude for selection;
  irrelevant if it is only used as a boolean tag.
- Cherenkov 0/1: healthy, no saturation, no change.
- **MCPs (16/17): mild clipping.** Baselines sit at ~3,660, so headroom below
  is ~3,660 counts, and 2.5–4% of beam events hit the bottom rail. Leading-edge
  timing survives a clipped top, but if clean MCP pulse shapes matter, attenuate
  or trim the MCP HV slightly; the DC offset is already near the top rail.
- High-gain baseline caveat for analysis: the pulse arrives at ~sample 100,
  so only ~80 samples of true pre-pulse baseline exist, and a small
  (~30–40 ADC) post-pulse undershoot persists to the record end — estimate
  baselines from samples < ~80. Verified: raw pre-pulse levels agree between
  beam and off-beam events to < 5 ADC counts on every upstream channel, so
  a baseline window that includes the pulse is the only thing that shifts
  beam-event waveforms.

## 2. Trigger-timing verdict

- **All prompt signals are captured.** The upstream module and MCPs fire at
  ~sample 100; the beam counters at ~460–540 (cable delays). Full rising
  edges everywhere, nothing clipped at the record edges.
- **The slow low-gain component is truncated.** Chs 4–7 are still at ~half of
  peak amplitude when the 1024-sample record ends, so a total-charge integral
  loses a chunk of the slow light. If the full slow integral matters, lower
  the DRS sampling rate to stretch the window (record length is fixed at
  1024); note `drs4_frequency` reads 0 in the DAQ metadata — worth checking
  what the board was actually set to.
- The high-gain undershoot (~30–40 ADC) spans the rest of the record
  (shaping/AC-coupling artifact), which is another reason late-window charge
  sums on 12–15 need care.

## Selection note

Cherenkov coincidence (ch 0 & ch 1, amp > 150 ADC) tags 9.9% of triggers.
Off-coincidence events still show upstream-module activity at a lower level
(the non-electron beam component), so the coincidence is doing real particle
selection, not just removing empty triggers.

## First-pass resolution measurements (`macros/Resolution.C`, 2026-08-28)

Method: Cherenkov-selected electrons (2,466 events); clipped HG pulses kept
via LG→HG amplitude inference (transfer lines fitted in the unclipped
region; 587 clipped pulses recovered); CFD at 30% of (inferred) amplitude on
the intact rising edge; DRS4 per-event time calibration from `times[2][1024]`
(5 GS/s, 200 ps/sample). LG↔HG pairing measured from amplitude correlations:
**4↔13, 5↔12, 6↔15, 7↔14** (not diagonal).

### Timing

| quantity | σ [ps] | N |
|---|---|---|
| TR0(grp0) − TR0(grp1): same MCP digitized twice | 104.5 ± 2.2 = pure DRS inter-group jitter | 2,466 |
| HG 12 − MCP1 | 251 ± 9 | 1,415 |
| HG 13 − MCP1 | 292 ± 11 | 1,429 |
| HG 14 − MCP1 | 356 ± 14 | 1,479 |
| HG 15 − MCP1 | 355 ± 13 | 1,409 |
| HG12 − HG13 (MCP-free) | 325 ± 14 → 230/capillary | 1,072 |

Amplitude dependence (all HG vs MCP1): 507 ± 23 ps (300–600 ADC),
323 ± 10 ps (600–1200), 287 ± 8 ps (1200–2400) — photostatistics scaling.
The top bin (2400–8000, mostly clip-recovered) gives 414 ± 88 ps after
restricting the transfer fit to the truly linear region (LG < 400 ADC) —
still above the 265 ps trend, so the clip-recovery CFD retains a residual
walk to correct before quoting a direct-hit timing number. Split-half values agree within ~2σ everywhere.
Cross-check: capillary-only 230 ps (from HG12−HG13) is consistent with
√(251² − 74²) ≈ 240 ps from the MCP-referenced fit.

### Energy response

**No Gaussian peak exists and no amount of statistics creates one**: 42.9%
of tagged electrons miss the module, and the rest form a position-driven
continuum ending in a sharp direct-hit edge. Erfc fit to the ΣLG edge:
position 2757 ± 54 ADC, width 289 ± 45 ADC → **edge width / position
= 10.5% ± 1.6%**, an upper bound on the resolution at full response (still
contains position falloff and the T10 momentum bite). A true energy
resolution measurement needs impact-position tagging (tracker, smaller
collimation, or a beam spot much smaller than the module) — not more events.

### MIP calibration (off-coincidence sample) and gains

MIP MPV per HG channel: ch 12 142.5 ± 0.4, ch 13 164.9 ± 0.4,
ch 14 159.8 ± 0.4, ch 15 120.9 ± 0.5 ADC — a 36% channel-to-channel gain
spread, measured to 0.3%. HG/LG gain ratios from the transfer fits:
2.58–3.17 (fitted in LG < 400 ADC, below the HG rolloff).

### Is 25,000 events enough?

For timing: yes — every σ above carries a 3–4% statistical error and stable
split-half values; the residual uncertainty is systematic (CFD fraction,
clip-recovery walk), which statistics does not fix. For MIP calibration:
yes, sub-percent. For energy resolution: statistics is not the limiting
factor at all — the measurement is blocked by the beam-spot/position
degeneracy, which a million events would not resolve. The only quantity that
clearly gains from more events is the direct-hit edge width (currently ±16%
relative), where ~10× more electrons would help — but position tagging
helps far more.

## Minimum event count (subsample scan, 2026-08-28)

Full analysis re-run on the first 10k / 15k / 20k events vs all 25k:

| measurement | 10k (985 e) | 15k (1,493 e) | 20k (1,976 e) | 25k (2,466 e) |
|---|---|---|---|---|
| MIP MPVs | ±0.4%, stable | ±0.35% | ±0.3% | ±0.3% |
| MCP0−MCP1 floor | 100 ± 4 ps | 100 ± 3 ps | 102 ± 2 ps | 105 ± 2 ps |
| capillary σ_t error | ±12–28 ps (6–7%), fits fragile | ±11–21 ps, occasional fit failure | ±10–16 ps (4%), all stable | ±9–14 ps (3–4%) |
| ΣLG edge fit | collapses (1.9 ± 1.2%) | collapses (4.1 ± 2.1%) | converges (9.6 ± 1.9%) | 10.5 ± 1.6% |

Guidance: **~10k events** (≈1.4 h at 2 Hz) suffices for MIP calibration +
MCP floor + capillary timing at ~7% precision; **~20k** is the floor at
which every fit (including split-half checks and the energy edge) is stable;
the edge fit does not converge below ~2,000 electrons. Recommended per
configuration when time-limited: 20k; 10k for timing-only points.

## Module vs MCP alignment

43% of electrons that traversed the MCP + both Cherenkovs leave <300 ADC in
the module, so the MCP trigger acceptance is NOT contained within the module
face. MIP-calibrated mean beam response per capillary (LG amp / LG-equivalent
MIP) rises monotonically 2.9 → 4.2 → 6.1 → 4.7 across capillaries 4/5/6/7:
the beam centroid sits toward the capillary-6/7 side, favoring a transverse
offset of the module relative to the MCP acceptance (though a beam spot
simply larger than the module face contributes too). Worth re-centering the
module on the MCP axis at the next access; the miss fraction is a live
alignment monitor.

**Alignment prescription (module 14x14 mm, MCP DIAMETER 10 mm; channel
layout: 4 top-left, 5 top-right, 6 bottom-left, 7 bottom-right):** the
5 mm-radius MCP disk fits entirely inside the module face, so perfect
centering gives a geometric miss fraction of ~0 - the observed 42.9% means
much of the MCP acceptance hangs off one module edge. MIP-calibrated row
sums: top (4+5) = 7.1 vs bottom (6+7) = 10.8 MIP-eq (40% asymmetry);
column sums: left 9.0 vs right 8.9 (balanced). The displacement is
therefore essentially pure vertical, toward the ch 6/7 row. Matching the
42.9% miss with an along-a-side shift requires **~6.4 mm**; shower leakage
across the edge masks some geometric misses, so treat it as a slight
underestimate. **Move the module ~6.4 mm vertically toward the ch 6/7
row; horizontal stays.** The opposite within-row imbalances (6>7 but 5>4)
are inconsistent with any translation and are attributed to per-channel
calibration systematics (~20-30% via the transfer-line MIP equivalents).
In-situ monitors: miss fraction falls steeply toward ~0 at center (residual
few % from edge leakage/halo); equalize top/bottom row sums (vertical) and
keep left/right column sums balanced (horizontal).

Normalization cross-check: HG amplitudes divided by each channel's OWN MIP
MPV (no LG transfer) give per-capillary means 3.82 / 4.62 / 6.38 / 5.51
(chs 4/5/6/7), i.e. bottom/top = 1.41, left/right = 1.01 — identical to the
LG-based ratios (1.40 / 1.01). The row asymmetry is robust against the
SiPM-response normalization; individual-channel differences within a row
remain calibration-limited (MIP-to-electron-scale linearity per channel). Standard config going
forward: 20,000 events per point.

## Run 14 alignment check (1,000 events, 2026-08-28)

Run 14 uses a NEW array layout (per James, matches historical data; verified
in-data): arrays 0-7 = DT5742 ch 0-7; array 8 = MCP TR0 copy in group 0;
arrays 9-16 = DT5742 ch 8-15; array 17 = MCP TR0 copy in group 1. The two
TR0 arrays are the SAME MCP pulse (corr 0.998) - which also means run 12's
"MCP0-MCP1" 104.5 ps is the pure DRS inter-group timing jitter (the MCP
cancels), not a two-MCP measurement. DT5742-native HG pairing is unchanged
(4-13, 5-12, 6-15, 7-14). Waveforms are in mV; post-trigger 74% puts prompt
pulses at sample ~52 (baselines from samples 0-39; tight - consider
restoring ~70%); HG gains genuinely ~2x up (MIP MPVs 255-325 ADC-eq).

After the ~6.4 mm move toward the ch 6/7 row: **miss fraction 7.9% +/- 3.1%
(was 42.9%)** - the move worked. MIP-normalized capillary means (run-14
self-calibrated): TL 4.92, TR 5.89, BL 6.97, BR 6.49 ->
**left/right = 0.96 +/- 0.06 (centered), bottom/top = 1.25 +/- 0.07**
(was 1.41). The residual row asymmetry is ~3 sigma statistically but within
the per-channel MIP-to-electron calibration systematics; if real it is
~1-2 mm. Optionally nudge ~1 mm further down, or verify with the first few
thousand events of the next 20k run before touching the stage.

## Run 15 QC (first 20k production run, 2026-08-28)

Complete and clean — see `Output/run_15/QCv2_summary.txt` (macro `QCv2.C`,
written for the run-14+ format). Highlights: zero event gaps; HG beam-event
clipping down to 0.2–0.6% (offset fix worked, even with the higher gains);
9.08% Cherenkov coincidence (1,816 electrons); MIP MPVs 329–404 ADC-eq
(+25% vs run 14: SiPM bias was raised +1 V on all LG and HG channels —
this is the final operating point for the campaign, so run 15's MIPs are
the reference calibration; runs 12/14 gains do not carry forward). Alignment identical to run 14 but now precise:
**bottom/top = 1.255 ± 0.014, left/right = 0.909 ± 0.010** → beam ~2–3 mm
low and ~1 mm right of module center. **Centering produced a real electron
peak in ΣLG** (mean 2992 ± 36, σ/E = 33.6 ± 1.4%, position-smearing
dominated) — every energy-scan point now yields a fittable response peak.

## Run 15 resolution analysis (`ResolutionV2.C`, final bias)

Transfer ratios 2.09/1.95/2.56/2.62 (caps 4/5/6/7); only 25 clipped HG
pulses in 20k events. Timing: inter-group jitter 107.6 ± 2.6 ps (stable vs
run 12's 104.5); capillary vs TR0 = 332/328/310/299 ps (uniform to ~10%,
vs 251–356 in run 12); MCP-free pair 520 ± 19 → 368 ps/capillary;
amplitude plateau ~305 ps above 1,200 ADC-eq. Electron ΣLG peak
2992 ± 36, σ/E 33.6 ± 1.4% (position-smeared). All in the run book
(run_summary.html) and Output/run_15/ResolutionV2_summary.txt.

## srCFD adopted from github.com/jwwetzel/radical (2026-08-28)

`BestTiming.C` now implements the collaboration's saturation-recovered CFD
(reduce/Reducer.C): threshold = 0.15 × (LG-predicted true HG peak), guarded
> 20 mV and < 0.9 × wall, leading-edge crossing with linear interpolation,
width via the repo's tebSigma robust estimator. Transfer calibration is
wall-aware (`TransferFit.C`): the clip wall is measured per capillary from
the HG spectrum (run 15: 3,143–3,186 ADC-eq) and the line is fit only below
0.72 × wall (LG 30–695…815; ratios 2.39/2.50/2.66/2.83). Run-15 srCFD:
per-capillary 312–409 ps; **4-capillary mean shower time 248 ± 8 ps** —
agrees with plain CFD at 5 GeV; srCFD's advantage appears where HG clips
(high-energy scan points). A ~−1.2 ns satellite shoulder in the per-cap Δt
distributions is visible (few % of events) — worth understanding before the
scan analysis. All plots now use `macros/radStyle.h` (house style).

## Uniform channel integrity (`ChannelIntegrity.C`, 2026-08-28)

One format-aware macro now produces the identical integrity treatment for
every run (both data formats): 18-channel waveform grid (baseline-subtracted,
common ADC-eq scale), per-channel health canvas (baseline RMS / signal
fraction / rail fraction / median peak sample), and identity checks. All
three runs pass: TR0 copy-copy correlation 0.998-0.999, LG<->HG pairing
convention verified per run (r 0.88-0.96). Cross-run trend: HG baseline RMS
24-38 (run 12) -> 75-81 (run 14) -> 81-111 ADC-eq (run 15), tracking the
bias raises — amplified dark counts. Outputs in Output/run_N/Integrity_*.
Run this for every new scan point alongside QCv2.C.

## MCP trigger threshold (run 15, `MCPThreshold.C`)

Both TR0 copies show a hard turn-on at 976-990 ADC-eq = ~240 mV (edge width
~7 mV): the TR0 discriminator threshold. Mean MCP pulse ~365 mV, so the cut
sits at ~65% of the mean - the low side of the true MCP spectrum is
trigger-sculpted; lower the threshold if unbiased MCP spectra / trigger
efficiency matter. Re-run per scan point to verify the setting is unchanged.

## Run 22 MCP threshold (1,263 events, short run)

TR0 turn-on 672 ADC-eq = 164 mV on both copies (was ~240 mV in run 15).
The whole MCP amplitude spectrum also moved down by a similar factor
(peak ~950 vs ~1,100+, tail to ~2,000 vs ~3,600 ADC-eq) — consistent with
lowered MCP HV plus a matching threshold retune; the threshold-to-mean
ratio (~0.7) is roughly unchanged, so trigger acceptance is comparable.

## Run 23 MCP threshold (~1,000 events)

TR0 turn-on 560 ADC-eq = 137 mV (both copies). Threshold sequence:
240 (r15) -> 164 (r22) -> 137 mV (r23). Revised interpretation with three
points: the MCP gain/HV drop happened between runs 15 and 22 (upper tail
3,600 -> ~2,000 ADC-eq); runs 22 -> 23 is a threshold-only change (upper
end unchanged, peak follows the threshold down as more low-amplitude
pulses are revealed). At 137 mV the spectrum is still rising into the cut
- not yet at full trigger efficiency for the weakest MCP pulses.

## MCP threshold vs trigger composition (runs 15/22/23)

Electron coincidence fraction per trigger: 9.1% at 240 mV (r15) ->
25.9% at 164 mV (r22) -> 21.8% at 137 mV (r23); scint/module occupancy
roughly unchanged. The MCP pulse height is species-correlated (electrons
smaller), so the 240 mV trigger was preferentially REJECTING electrons.
Consequence: at the new threshold 20k events contain ~4,400-5,200
electrons (was 1,816) in ~30 min (was 2h07m) — electrons per beam-minute
up ~10x. Keep 20k/point. For timing, cut offline on MCP amplitude
(e.g. > 240 mV-eq) to keep the reference clean; energy/alignment keep all.
Caveat: confirm runs 22/23 were 5 GeV (if energies differed, composition
comparison is partly confounded).

## Energy-scan plan + offline electron ID (2026-08-28)

Plan: 1/3/5 GeV with XCET pressures set (hardware e-tag, XCET data saved);
7/9/11 GeV with OFFLINE electron selection (XCET tagging fails above ~7 GeV:
muon-threshold pressure ~1/p^2 starves the light yield, and e+ content is
unmeasurable above ~6 GeV — run the high points in NEGATIVE polarity,
e- = 2-3%). ElectronID.C, trained tag-and-probe on run 15: prompt/slow
window (SumHG/SumLG in [2.0,3.9]) 90.9% eff / x3 rejection; + energy window
(SumLG scaled to beam E) 73.4% eff / x19 rejection. Shower sharing is NOT
discriminating; prompt/slow is. Requests per high-E point: ~2k-event
XCON021 lead-converter-IN control run (electron-depleted -> measures hadron
leakage); one stopper-closed muon run overall; at 7 GeV set ~0.22 bar
anyway as a marginal training point. Watch LG clipping at 11 GeV.

Note: the MCP amplitude (species-correlated: electrons smaller) is a third,
module-independent discriminant for the 7/9/11 GeV electron selection —
train its e/hadron shapes on the 1/3/5 GeV tagged runs, use it both in the
selection and as an unbiased cross-check (selected "electrons" at high E
must reproduce the tagged electrons' MCP-amplitude shape, not the hadrons').

## Energy scan v1: 1/3/5/7 GeV (2026-08-29, EnergyScan.C)

Datasets: 25+26 (1 GeV, 10,264 ev), 24 (3 GeV, 20k), 15 (5 GeV, 20k),
27 (-7 GeV, 20k, XCETs at 0.21 bar marginal tag). Results:
peaks ~380(ridge-merged) / 1742+/-30 / 3002+/-43 / 4400+/-97 ADC-eq;
shower-time sigma 440/300/251/242 ps -> sigma_t ~ 404/sqrt(E) (+) 174 ps
(3 GeV within 3% of prediction). Response linear above 3 GeV (~660/GeV);
sub-linear low E as expected for shower-max sampling. 1 GeV energy point
compromised by beam spot (peak merges with miss ridge). Gains drifted ~25%
over the day (temperature) - always use per-run MIPs. 7 GeV marginal tag
works: 347 electrons, both trends continued.
BUG POSTMORTEM: EnergyScan's crash/livelock chain was a use-after-free -
histograms created while a TFile is open are owned by it and deleted at
Close(); fix = SetDirectory(nullptr). The "Minuit segfault" hypothesis was
wrong; hard exit kept as belt-and-suspenders.

## 7 GeV recount (XCETCheck.C, 2026-08-29)

The 285-electron figure was a threshold artifact: at 0.21 bar the XCETs make
~5 pe and the electron spectrum sits at 40-200 ADC-eq; the standard 150 cut
bisected it. Threshold scan: coincidence 1.74% @150 -> 8.77% @40, with
singles 9.2%/9.0% (near-total correlation = real radiating particles; pi/mu
below threshold at 0.21 bar). CONCLUSION: the -7 GeV beam is ~9% electrons
(supersedes the T10 table's 2.3+/-1.6%). With cthr=50 in EnergyScan:
7 GeV = 1,200 e on module, peak 4874+/-48, sigma/E 20.4+/-1.1% (best width
point), shower-time 223+/-9 ps. No additional 7 GeV data needed. Per-energy
XCET thresholds now in EnergyScan.C (150/150/150/50).

## XCET threshold audit across the scan (XCETCheck.C, 2026-08-29)

Coincidence-vs-threshold plateaus: 1 GeV flat from 60 (85.9%); 3 GeV flat
80-200 (23.5%); 5 GeV plateau 80-100 at 10.0% -> the common 150 cut was
clipping ~9% of 5 GeV electrons (benign: random, XCET light uncorrelated
with module response - peak/timing unchanged within errors, N 1638->1790);
7 GeV needed 50 (the big recount). EnergyScan now uses per-energy plateau
thresholds {100,100,100,50}. Lesson: set tag thresholds from each run's own
XCET spectra, never carry one number across pressure settings.

## 9 GeV point + overnight drift (runs 28/29, 2026-08-29)

XCETs at 0.150/0.156 bar (~3 pe): no threshold plateau; tag at floor
(40 ADC-eq) gives 4.86% coincidence with 5.5%/5.3% singles => -9 GeV beam
is >=5% electrons (table said ~2%). Merged timing first read 548 ps —
chronological slicing of the overnight run 29 exposed intermittent drift
(stable 330-420 ps vs 536-1218 ps periods, center jumps +300 ps, worst at
morning). DQ selection (run 28 + run 29 slices 0/1/4/6 = 26,028 events,
run_9001.root): peak 5782+/-160, sigma/E 31.9+/-3.8%, shower-time
212+/-11 ps — the trend (406/sqrt(E) (+) 171) predicts 218. LG rail still
0.0000 at 9 GeV. TO-DO: time-dependent calibration recovers run 29's 20k
drift-period events; for 11 GeV overnight running, add environmental
monitoring or periodic calibration triggers.

## Overnight stability investigation (DriftStudy.C, runs 28+29, 2026-08-29)

James's challenge ("insulated box shouldn't feel ambient") CONFIRMED for the
night itself: pooled MIP MPV flat +/-1% for 8.5 h (closed-box thermal
equilibrium from self-heating). The degradation splits in two: (1) a sharp
MORNING event ~09:00-11:00 (gain +60%, dark noise x2, LG baselines +40 mV,
MCP untouched) - environment reaching the box at morning, not night cooling;
the day-scale gain ladder (404->327->281->216 across runs 15/24/27/28) is
slow relaxation to closed-lid equilibrium after daytime accesses. (2) The
intermittent night-time timing volatility correlates with NOTHING (TR0
copy-copy 102-115 ps every slice = digitizer stable; MCP-free pair broadens
= not the MCP; gain/noise/rate/tag flat; all four caps fluctuate
independently; survives purity cuts) -> attributed to sporadic near-
threshold srCFD outliers dragging the per-event MEAN at the scan's lowest
per-capillary light; fix = robust per-event combination (median /
amplitude-weighted), an analysis to-do, hardware not implicated.
Recommendations: never chase gain with bias (per-run MIPs absorb it); end
overnight runs before ~08:30 or log box temperature; XCETScan.png added to
the scan page (tag plateaus vs threshold across all energies).

## Morning event = LIGHT LEAK (James's hypothesis, confirmed 2026-08-29)

Three-way discriminator (DriftLeak in DriftStudy.C): tail-pulse rate
3.5 -> 8.4/event (x2.4) while tail-pulse amplitude +14% only and the
tagged-electron SumLG response scale is FLAT -> more photons, same
detector = ambient light entering the box (morning lights/sun/activity).
Corroboration: LG DC baselines shift 40 mV in the pulse direction
(photocurrent, DC-coupled); HG baselines unmoved (AC-coupled); MCP (sealed
housing) untouched. The earlier "gain +60%" was a peak-amplitude MIP
estimator artifact under doubled noise. Campaign-wide: daytime runs have
~2x overnight noise (run 15 HG RMS 81-111 vs 50-55 overnight) -> the
day-scale "gain ladder" is likely part lighting leak, part thermal.
ACTIONS: lights-on/off baseline-RMS test at next access; tape the leak
(may halve daytime noise); MIP estimator should use charge integrals or
noise-robust fits when baseline RMS is elevated; per-run MIPs remain the
operative calibration either way.

## Sunrise-event recovery (2026-08-29)

The leak-period events are recoverable in software: per-event MEDIAN
capillary combination (= repo's eventDWUP consistency logic, which the
simplified mean had dropped) gives sunrise slices 278 ps, night control
208 ps, and the FULL uncut 46k dataset 253 ps (N=924) with zero DQ cuts.
5-sample edge smoothing does NOT help (softens the slope more than it
tames the noise) - skip it. Decision: median adopted as the standard
per-event combination for all timing (also dissolves the "intermittent
night volatility" = mean dragged by near-threshold outliers); official
9 GeV point remains the clean-conditions DQ sample (212+/-11); morning
events rejoin the pool for energy/alignment/selection training. TO-DO:
switch EnergyScan/BestTiming to median combination and re-derive the
scan timing numbers (expect small improvements at every energy).
