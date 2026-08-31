#!/usr/bin/env python3
"""Production run-page builder for the RADiCAL T10 run book.

Every production run (15 and up) gets an IDENTICAL page template:
  (1) Channel integrity      : Integrity_waveforms, Integrity_health
  (2) Selection & calibration: QCv2_MIP, TransferFit, MCPThreshold, XCETCheck, ElectronID
  (3) Results                : QCv2_align, BestTiming
  (4) Run-specific notes     : this run's prose, tables, extra figures, verdict
Each standard plot is full-width, centered, captioned with a bold title and a
one-line description. Stage 4 is preserved verbatim on rebuilds (data-notes).

TO ADD A NEW RUN (e.g. the 11 GeV point):
  1. run the macro pipeline into Output/run_XX (QCv2, ChannelIntegrity,
     MCPThreshold, XCETCheck, ElectronID, TransferFit, BestTiming)
  2. append a dict to RUNS below (key, dir, btn, head, notes) — see the
     commented R11GEV stub at the end of RUNS
  3. python3 build_runbook.py          (add --dry to preview)
  4. python3 build_site.py   (NEVER copy run_summary.html to docs/ — it is the internal store)
Missing plots are skipped with a warning, so a partially analyzed run can be
published early and the page filled in as macros finish.
"""
import base64, os, re, sys
from PIL import Image

WS = os.path.dirname(os.path.abspath(__file__))
os.chdir(WS)
DRY = "--dry" in sys.argv

def enc(path, w=1200, q=78):
    im = Image.open(path).convert("RGB")
    if im.width > w: im = im.resize((w, int(im.height*w/im.width)), Image.LANCZOS)
    import io
    buf = io.BytesIO(); im.save(buf, "JPEG", quality=q, optimize=True)
    return "data:image/jpeg;base64," + base64.b64encode(buf.getvalue()).decode()

# ---- canonical plot sequence: (filename, stage, title, description) ----
STD = [
 ("Integrity_waveforms.png", 1, "Waveform persistence",
  "Every event in the run overlaid per channel (log color scale); teal = mean tagged-electron waveform. "
  "One glance confirms channel identity, polarity, and timing windows, and exposes dead or noisy channels."),
 ("Integrity_health.png", 1, "Channel health",
  "Per-channel baseline RMS, signal fraction, rail (clip) fraction, and median pulse peak-time. "
  "Rails surface here first (event-gap counts live in the summary log); the LG rail fraction is the bias-headroom watch item."),
 ("QCv2_MIP.png", 2, "MIP calibration",
  "Off-coincidence (hadron/muon) HG spectra with Landau fits per capillary — this run's MIP reference. "
  "Caution: cross-run MIP comparisons carry beam-species and noise-estimator systematics (see the LuAG "
  "linearity forensics) and are NOT a gain monitor."),
 ("TransferFit.png", 2, "HG&harr;LG transfer, wall-aware",
  "HG vs LG amplitude with the linear transfer fit restricted to the region below the HG clip wall — "
  "the fit range is drawn explicitly. The line predicts the true amplitude of clipped pulses, so no "
  "clipped high-gain signal is ever discarded."),
 ("MCPThreshold.png", 2, "MCP trigger threshold",
  "TR0 amplitude spectrum and trigger turn-on: THIS run's measured hardware threshold, annotated on the "
  "plot. The campaign working point moved 240 to 137 mV mid-campaign after the species-correlated "
  "pulse-height discovery — each run page records its own value."),
 ("XCETCheck.png", 2, "XCET spectra &amp; threshold",
  "Cherenkov amplitude spectra for the two counters (red line: reference software threshold). The "
  "per-energy tag threshold is chosen on the coincidence plateau from the threshold-scan table in the "
  "run's summary log."),
 ("ElectronID.png", 2, "Electron ID tag-and-probe",
  "Selector variables for XCET-tagged electrons vs off-coincidence hadrons — the offline cross-check of "
  "the hardware tag (which, in the end, reached every energy in this campaign)."),
 ("QCv2_align.png", 3, "&Sigma;LG spectrum &amp; containment",
  "Summed low-gain spectrum of tagged events, and pulse peak-time containment for the key channels "
  "(legend on plot). Row/column balance and miss-fraction numbers live in the run notes below."),
 ("BestTiming.png", 3, "srCFD shower timing",
  "Per-capillary &Delta;t to the MCP using srCFD (threshold 0.15&times; the LG-predicted true amplitude) "
  "and the MEAN-combined shower time shown here; the scan tables quote the adopted per-event MEDIAN "
  "combination, with the mean kept for comparison."),
]
STAGES = {1: ("Channel integrity &amp; identification", "same checks, every production run"),
          2: ("Selection &amp; calibration",            "tag, gain reference, transfer, timing inputs"),
          3: ("Results",                                 "containment and shower timing"),
          4: ("Run-specific notes",                      "everything unique to this run")}

# ---- production runs (15 and up). key = section id, dir = Output subdir ----
RUNS = [
 {"key": "r15",   "dir": "run_15",
  "extras": [("Output/run_15/electron_peak.png",        "Electron response peak",
              "&Sigma;LG for Cherenkov-tagged electrons with iterative Gaussian core fit — the 5 GeV anchor."),
             # ResolutionV2_HGvsLG deliberately excluded: its transfer line is drawn
             # over the full range with no fit-range/wall annotation (fixed LG 30-400
             # fit) — superseded by the wall-aware TransferFit and never used downstream.
             ("Output/run_15/ResolutionV2_timing.png",  "Timing distributions (V2) &mdash; first pass, superseded",
              "Early CFD timing from ResolutionV2 (fixed-range transfer, no wall handling). Kept for "
              "history; the quoted numbers come from the srCFD analysis above."),
             ("Output/run_15/ResolutionV2_energy.png",  "Electron response peak (V2) &mdash; first pass",
              "Bounded iterative fit of the tagged-electron &Sigma;LG peak from the ResolutionV2 analysis.")]},
 {"key": "r24",   "dir": "run_24",   "extras": []},
 {"key": "r2526", "dir": "run_2526",
  "extras": [("Output/run_25/assess.png",               "Run-25 crash assessment",
              "3,200 clean events before the DAQ stop — verified usable and merged with run 26.")]},
 {"key": "r27",   "dir": "run_27",   "extras": []},
 {"key": "r2829", "dir": "run_2829",
  "extras": [("Output/run_2829/DriftStudy.png",         "Overnight drift time series",
              "MIP gain, pe rate, baselines and timing vs wall-clock — thermally stable box; the morning "
              "ramp is ambient light (sunrise/lights), not temperature.")]},
 {"key": "r30", "dir": "run_30", "extras": [],
  "new": {"btnname": "Run 30", "dot": "warn",
          "meta": ["40,000 ev · &minus;11 GeV · Aug 29", "11 GeV · wide beam at T10 limit"],
          "title": "Run 30 — 11 GeV scan point (T10 momentum limit)",
          "sub":   "Aug 29, 11:12&ndash;12:48 CEST · XCETs at 0.062/0.060 bar &mdash; the hardware tag works after all",
          "chips": ["40,000 events", "&minus;11 GeV", "XCET tag, thr floor (40)", "mV format"],
          "pill":  ("warn", "complete — wide-beam caveat"),
          "notes": """<p><b>The tag works at 11 GeV.</b> The XCETs were pressurized to 0.062/0.060 bar
      (&asymp; the 0.44&middot;(5/11)&sup2; scaling) &mdash; &pi;&#8315;/&mu;&#8315; sit safely below Cherenkov
      threshold there, and the two counters in coincidence at the threshold floor (thr 40) give
      <span class="num">867 tags = 2.17%</span> of triggers against an accidental rate of ~0.11%.
      No offline-only selection needed; ElectronID cross-checks the tag instead of replacing it.</p>
      <p><b>Wide beam at the T10 momentum limit.</b> 54% of tagged electrons deposit &Sigma;LG &lt; 400
      (miss the 14&times;14 module, vs 8&ndash;12% miss at 3&ndash;9 GeV) and a partial-containment
      continuum runs up to ~4.5k. The contained-shower peak stands clearly at
      <span class="num">5696 &plusmn; 117</span>. The scan therefore uses a containment floor
      &Sigma;LG &gt; 3800 for this point (drawn from the measured spectrum), and the timing is computed
      on contained showers only. Timing shows why the median combination is the adopted estimator:
      <span class="num">t-MEDIAN 227 &plusmn; 32 ps</span> vs t-MEAN 547 &plusmn; 32 &mdash; edge-hit
      outliers (right column, caps 5/7) destroy the mean, the median holds within ~1.4&sigma; of the
      photostatistics trend (181 ps predicted).</p>
      <p><b>Health.</b> Zero event gaps; TR0 copies correlate at 0.998; all four LG&harr;HG pairings
      verified; MCP threshold measured 133 mV; LG rail 0.00&ndash;0.03% even at 11 GeV &mdash; the bias
      stays frozen. HG clips in ~2% of beam events, handled by the wall-aware transfer. MIP MPVs
      459/440/376/405 ADC-eq (+13% vs run 15, the day-scale gain ladder; absorbed per-run).</p>
      <div class="tblwrap"><table>
      <tr><th>quantity</th><th>value</th></tr>
      <tr><td class="t">events / tagged e&#8315;</td><td class="num">40,000 / 867 (2.17%)</td></tr>
      <tr><td class="t">contained electrons (&Sigma;LG &gt; 3800)</td><td class="num">187</td></tr>
      <tr><td class="t">&Sigma;LG peak</td><td class="num">5696 &plusmn; 117 ADC-eq</td></tr>
      <tr><td class="t">&sigma;/E (position-smeared)</td><td class="num">18.5 &plusmn; 2.6%</td></tr>
      <tr><td class="t">srCFD shower-time &sigma; (median | mean)</td><td class="num">227 &plusmn; 32 | 547 &plusmn; 32 ps</td></tr>
      <tr><td class="t">response vs linear &times; shower-max migration</td><td class="num">&minus;5.5% (SaturationStudy: within the gain ladder)</td></tr>
      </table></div>
      <div class="verdict warn"><b>Verdict.</b> Valid 11 GeV point with a wide-beam caveat: statistics
      limited to 187 contained showers, and the mean-based timing is unusable &mdash; the median rescues
      it. The apparent response flattening was traced (SaturationStudy, scan page) to the
      cross-run SiPM gain ladder plus calculable shower-max migration &mdash; not detector saturation. A tighter momentum slit or converter-out retake would upgrade this point.</div>"""}},
 # ================= DSB1 module (runs 31-37, Aug 29-30) =================
 {"key": "r31", "dir": "run_31", "extras": [],
  "new": {"btnname": "Run 31 · DSB1", "dot": "warn",
          "meta": ["40,000 ev · &minus;11 GeV · Aug 29", "DSB1 · first run of the module"],
          "title": "Run 31 — DSB1 at 11 GeV",
          "sub":   "Aug 29, 13:44&ndash;15:21 CEST · straight after LuAG run 30 on the same &minus;11 GeV beam",
          "chips": ["40,000 events", "&minus;11 GeV", "XCET floor tag (40)", "DSB1"],
          "pill":  ("warn", "complete — wide-beam era"),
          "notes": """<p>First DSB1 run, same beam and tight-slit config as LuAG run 30. Floor-threshold tag:
      <span class="num">948 = 2.37%</span> (LuAG run 30: 2.17% — beam reproducible). Miss fraction 18.7%.
      MIP MPVs 498/489/459/486. Scan point (contained, &Sigma;LG &gt; 6500): peak 10721 &plusmn; 734 — approaching the ~13.6k LG headroom
      ceiling — and <b>t-MEDIAN 181 &plusmn; 17 ps</b> (mean 191; LuAG at 11 GeV: 227).</p>"""}},
 {"key": "r32", "dir": "run_32", "extras": [],
  "new": {"btnname": "Run 32 · DSB1", "dot": "good",
          "meta": ["40,000 ev · &minus;9 GeV · Aug 29", "DSB1 · 9 GeV scan point"],
          "title": "Run 32 — DSB1 at 9 GeV",
          "sub":   "Aug 29, 15:27&ndash;18:12 CEST · daytime run (pre-tape light-leak noise conditions)",
          "chips": ["40,000 events", "&minus;9 GeV", "XCET floor tag (40)", "DSB1"],
          "pill":  ("good", "complete"),
          "notes": """<p>Floor tag <span class="num">1,954 = 4.88%</span> (LuAG run 28: 4.9% — identical).
      Miss fraction 24.3%. MIP MPVs 441/422/382/411. Scan point (&Sigma;LG &gt; 6000): peak 9946 &plusmn; 144, &sigma;/E 26.5%, and <b>t-MEDIAN 133 &plusmn; 5 ps</b> —
      best timing of the campaign; the 516 ps mean vs 133 median is the most extreme daytime-outlier
      rescue yet (diff &minus;383 ps).</p>"""}},
 {"key": "r33", "dir": "run_33", "extras": [],
  "new": {"btnname": "Run 33 · DSB1", "dot": "good",
          "meta": ["20,000 ev · &minus;7 GeV · Aug 29", "DSB1 · 7 GeV scan point"],
          "title": "Run 33 — DSB1 at 7 GeV",
          "sub":   "Aug 29, 18:17&ndash;19:17 CEST",
          "chips": ["20,000 events", "&minus;7 GeV", "XCET floor tag (40)", "DSB1"],
          "pill":  ("good", "complete"),
          "notes": """<p>Tag <span class="num">1,721 = 8.61%</span> &asymp; the known ~9% e&#8315; content of the
      &minus;7 GeV beam. Miss 18.6%; beam a touch low/left (rows 1.27, cols 1.15). MIP MPVs 406/374/265/365.
      Scan point (&Sigma;LG &gt; 4000): peak 9045 &plusmn; 90, &sigma;/E 21.6%, <b>t-MEDIAN 140 &plusmn; 6 ps</b> (LuAG: 191).</p>"""}},
 {"key": "r34", "dir": "run_34", "extras": [],
  "new": {"btnname": "Run 34 · DSB1", "dot": "warn",
          "meta": ["20,000 ev · +5 GeV · Aug 29", "DSB1 · BAD XCET PRESSURE"],
          "title": "Run 34 — DSB1 at 5 GeV (pion-contaminated tag)",
          "sub":   "Aug 29, 19:31&ndash;19:41 CEST · XCETs at 1.5 bar — above BOTH the &mu; (0.52 bar) and &pi; (0.90 bar) thresholds",
          "chips": ["20,000 events", "+5 GeV", "1.5 bar — e/&mu;/&pi; tag", "superseded by run 37"],
          "pill":  ("warn", "tag invalid — redo taken"),
          "notes": """<p>Coincidence <span class="num">66.6&ndash;69.7%</span> = the e+&mu;+&pi; content of the positive
      5 GeV beam (67.5% from the T10 tables) — quantitative proof the 1.5 bar tag radiates on pions.
      The 508 ps &ldquo;shower time&rdquo; of the tagged sample is the hadronic giveaway. As an electron point this
      run is dead (run 37 at 0.400/0.405 bar replaces it); it survives as a <b>&pi;-enriched control sample</b>
      and the fastest MIP set of the campaign (20k in 11 minutes).</p>"""}},
 {"key": "r35", "dir": "run_35", "extras": [],
  "new": {"btnname": "Run 35 · DSB1", "dot": "good",
          "meta": ["20,000 ev · +3 GeV · Aug 29", "DSB1 · 3 GeV scan point"],
          "title": "Run 35 — DSB1 at 3 GeV",
          "sub":   "Aug 29, 20:07&ndash;20:26 CEST · textbook tag plateau",
          "chips": ["20,000 events", "+3 GeV", "XCET tag (100)", "DSB1"],
          "pill":  ("good", "complete"),
          "notes": """<p>Cleanest tag of the DSB1 set: plateau at <span class="num">23.3%</span> (LuAG run 24: 23.5%).
      ElectronID selector holds at 83.2% efficiency. Miss 12.7%. MIP MPVs 277/203/169/271.
      Scan point (&Sigma;LG &gt; 1500): peak 3763 &plusmn; 73, &sigma;/E 56%, <b>t-MEDIAN 202 &plusmn; 4 ps</b> (LuAG: 274).</p>"""}},
 {"key": "r36", "dir": "run_36", "extras": [],
  "new": {"btnname": "Run 36 · DSB1", "dot": "warn",
          "meta": ["20,000 ev · +1 GeV · Aug 29", "DSB1 · config changed mid-run"],
          "title": "Run 36 — DSB1 at 1 GeV (the rate-mystery run)",
          "sub":   "Aug 29, 20:51&ndash;22:50 CEST · XCET40 bled 11&rarr;4 bar and collimators opened mid-run",
          "chips": ["20,000 events", "+1 GeV", "XCET tag (100)", "mixed beam config"],
          "pill":  ("warn", "usable — stable retake planned overnight"),
          "notes": """<p>This is the run diagnosed live in FINDINGS (&ldquo;1 GeV rate mystery&rdquo;): it began in the
      11 GeV tight-slit config with ~17% X&#8320; of XCET gas in the line (33 trig/min, beam wider than the
      profile monitor) and recovered after the mid-run pressure bleed + collimator opening. The tag is immune
      to the change (plateau flat to thr 200 at both pressures): <span class="num">86.6%</span> coincidence = the
      1 GeV e&#8315; content. Beam width and rate differ across the boundary — miss fraction 66.2% overall,
      dominated by the pre-fix hours. A stable overnight 1 GeV retake is being taken; treat this run as
      the backup, sliced at the config boundary if used.</p>"""}},
 {"key": "r37", "dir": "run_37", "extras": [],
  "new": {"btnname": "Run 37 · DSB1", "dot": "good",
          "meta": ["20,000 ev · +5 GeV · Aug 29", "DSB1 · 5 GeV point, correct pressure"],
          "title": "Run 37 — DSB1 at 5 GeV (the clean one)",
          "sub":   "Aug 29, 23:43&ndash;23:55 CEST · XCETs at 0.400/0.405 bar — below the 0.518 bar &mu; threshold",
          "chips": ["20,000 events", "+5 GeV", "XCET tag (100)", "replaces run 34"],
          "pill":  ("good", "complete"),
          "notes": """<p>The 5 GeV point done right: pressures under the muon threshold give a clean e&#8315; tag with a
      textbook plateau at <span class="num">5.7%</span> (the T10 table says 4.1% — in-situ above the tables again).
      1,132 tagged positrons; <b>miss fraction 7.8% — the best containment of the campaign</b>, courtesy of the
      post-fix beam config. MIP MPVs 282/200/168/267. Scan point (&Sigma;LG &gt; 2500): peak
      <span class="num">6769 &plusmn; 138</span>, &sigma;/E 39.7%, <b>t-MEDIAN 156 &plusmn; 8 ps</b>
      (LuAG at 5 GeV: 228) — on the DSB1 trend prediction of 165 ps.</p>"""}},
 {"key": "r38", "dir": "run_38", "extras": [],
  "new": {"btnname": "Run 38 · DSB1", "dot": "good",
          "meta": ["50,000 ev · +1 GeV · Aug 29", "DSB1 · stable 1 GeV retake"],
          "title": "Run 38 — DSB1 at 1 GeV (stable config)",
          "sub":   "Aug 29, 23:57&ndash;00:24 CEST · 50k events in 27 minutes — the fixed beam config at full throttle",
          "chips": ["50,000 events", "+1 GeV", "XCET tag (100)", "replaces run 36"],
          "pill":  ("good", "complete"),
          "notes": """<p>Single stable configuration end to end (unlike run 36): <span class="num">~1,850 events/min</span>,
      ~30&times; run 36&rsquo;s pre-fix rate — the collimator + XCET-gas diagnosis cashed in. Tag plateau
      <span class="num">88.8%</span> = 44,408 tagged electrons; 22,745 on module, 14,582 timed.
      Miss 57.7% is the 1 GeV material-scattering floor, not a config problem. Scan point:
      response ridge-merged at the 1 GeV floor (as for LuAG), <b>t-MEDIAN 366 &plusmn; 4 ps</b> with the
      campaign&rsquo;s largest timed sample.</p>"""}},
 # ================= EJ199 module (runs 39-41+, Aug 30) — WLS-only baseline =================
 {"key": "r39", "dir": "run_39", "extras": [],
  "new": {"btnname": "Run 39 · EJ199", "dot": "good",
          "meta": ["20,000 ev · +1 GeV · Aug 30", "EJ199 · first run of the module"],
          "title": "Run 39 — EJ199 at 1 GeV",
          "sub":   "Aug 30, 12:08&ndash;12:20 CEST · EJ-199 tuned for LuO:Yb — spectrally mismatched to the module&rsquo;s LYSO:Ce tiles by spec",
          "chips": ["20,000 events", "+1 GeV", "XCET tag (100)", "EJ199 WLS-only"],
          "pill":  ("good", "complete"),
          "notes": """<p>First EJ199 beam. Tag plateau <span class="num">88.7%</span> (17,732 e). Miss 69.5%
      (1 GeV material floor). Timing is slow — <b>t-MEDIAN 1367 &plusmn; 24 ps</b> — consistent with fluorescence-like re-emission of the
      LYSO:Ce tile light by the suspected contaminant (EJ-199&rsquo;s spec absorption band excludes 425 nm).</p>"""}},
 {"key": "r40", "dir": "run_40", "extras": [],
  "new": {"btnname": "Run 40 · EJ199", "dot": "good",
          "meta": ["20,000 ev · +3 GeV · Aug 30", "EJ199 · 3 GeV scan point"],
          "title": "Run 40 — EJ199 at 3 GeV",
          "sub":   "Aug 30, 12:29&ndash;12:51 CEST · same ~0.4 bar XCET fill as the whole positive set",
          "chips": ["20,000 events", "+3 GeV", "XCET tag (100)", "EJ199 WLS-only"],
          "pill":  ("good", "complete"),
          "notes": """<p>Tag plateau <span class="num">22.8%</span> (4,557 e), matching DSB1/LuAG — beam reproducible
      across all three modules. Miss 17.4%. Scan point (&Sigma;LG &gt; 500): peak
      <span class="num">1354 &plusmn; 42</span>, <b>t-MEDIAN 626 &plusmn; 13 ps</b>.</p>"""}},
 {"key": "r41", "dir": "run_41", "extras": [],
  "new": {"btnname": "Run 41 · EJ199", "dot": "good",
          "meta": ["20,000 ev · +5 GeV · Aug 30", "EJ199 · 5 GeV scan point"],
          "title": "Run 41 — EJ199 at 5 GeV",
          "sub":   "Aug 30, 12:56&ndash;13:11 CEST",
          "chips": ["20,000 events", "+5 GeV", "XCET tag (100)", "EJ199 WLS-only"],
          "pill":  ("good", "complete"),
          "notes": """<p>Tag plateau <span class="num">5.6%</span> (1,121 e). Miss 12.0%. Scan point
      (&Sigma;LG &gt; 1200): peak <span class="num">2740 &plusmn; 74</span> — ~550 ADC-eq/GeV, the dimmest module
      (LuAG ~620, DSB1 ~1,350) — and <b>t-MEDIAN 380 &plusmn; 18 ps</b> (LuAG 228, DSB1 156). By spec this channel should be nearly blind to the LYSO:Ce tiles&rsquo; 425 nm light — the observed response
      is in-beam support for the suspected Eljen contaminant, and this run quantifies it.</p>"""}},
 {"key": "r42", "dir": "run_42", "extras": [],
  "new": {"btnname": "Run 42 · EJ199", "dot": "good",
          "meta": ["20,000 ev · &minus;7 GeV · Aug 30", "EJ199 · 7 GeV scan point"],
          "title": "Run 42 — EJ199 at 7 GeV",
          "sub":   "Aug 30, 13:13&ndash;13:41 CEST · XCETs 0.21 bar",
          "chips": ["20,000 events", "&minus;7 GeV", "XCET floor tag (40)", "EJ199"],
          "pill":  ("good", "complete"),
          "notes": """<p>Tag 7.77% (1,554 e). Scan point (&Sigma;LG &gt; 1700): peak <span class="num">4658 &plusmn; 97</span>,
      <b>t-MEDIAN 278 &plusmn; 10 ps</b>. This run also supplies the EJ199 curve of the three-channel pulse-shape
      comparison (tail &tau; = 6.5 ns, 25% of light after 8 ns — the fingerprint for the contaminant question).</p>"""}},
 {"key": "r43", "dir": "run_43", "extras": [],
  "new": {"btnname": "Run 43 · EJ199", "dot": "good",
          "meta": ["20,000 ev · &minus;9 GeV · Aug 30", "EJ199 · 9 GeV scan point"],
          "title": "Run 43 — EJ199 at 9 GeV",
          "sub":   "Aug 30, 13:42&ndash;16:37 CEST · 2.9 h wall clock (supercycle-lean afternoon)",
          "chips": ["20,000 events", "&minus;9 GeV", "XCET floor tag (40)", "EJ199"],
          "pill":  ("good", "complete"),
          "notes": """<p>Tag 5.17% (1,034 e). Scan point (&Sigma;LG &gt; 2200): peak <span class="num">5704 &plusmn; 159</span>,
      <b>t-MEDIAN 249 &plusmn; 17 ps</b>.</p>"""}},
 {"key": "r44", "dir": "run_44", "extras": [],
  "new": {"btnname": "Run 44 · EJ199", "dot": "warn",
          "meta": ["20,000 ev · &minus;11 GeV · Aug 30", "EJ199 · 11 GeV, raised pressure"],
          "title": "Run 44 — EJ199 at 11 GeV",
          "sub":   "Aug 30, 16:44&ndash;17:42 CEST · XCETs at the agreed 0.089 bar (raised from 0.060)",
          "chips": ["20,000 events", "&minus;11 GeV", "0.089 bar tag (40)", "11 GeV caveat"],
          "pill":  ("warn", "complete — low stats"),
          "notes": """<p>The raised 0.089 bar fill delivered a modest tag gain: 2.48% vs 2.17&ndash;2.37% at 0.060 bar
      for the other modules. Only 202 on-module electrons; the &Sigma;LG peak fit is unstable
      (8655 &plusmn; 4152) and the standing 11 GeV purity caveat applies. <b>t-MEDIAN 266 &plusmn; 37 ps.</b></p>"""}},
]

def figblock(path, title, desc):
    if not os.path.exists(path):
        print(f"  !! missing {path} — skipped (rerun builder when the macro has produced it)")
        return ""
    src = "B64" if DRY else enc(path)
    return (f'<figure class="stdfig"><img src="{src}" alt="{title}" loading="lazy">'
            f'<figcaption><b>{title}.</b> {desc}</figcaption></figure>')

def stage_html(n, inner):
    t, why = STAGES[n]
    attr = ' data-notes=""' if n == 4 else ''
    return (f'\n  <div class="stage"{attr}>\n    <div class="stagehead"><span class="n">{n}</span>'
            f'<h3>{t}</h3><span class="why">{why}</span></div>\n'
            f'    <div class="card">{inner}</div>\n  </div>')

s = open("run_summary.html").read()

# hero-figure CSS (once)
if ".stdfig" not in s:
    s = s.replace("</style>", """
.stdfig{max-width:940px;margin:20px auto 26px}
.stdfig img{width:100%;display:block;border-radius:8px}
.stdfig figcaption{font-size:13px;color:#5b6570;margin-top:7px;line-height:1.45;max-width:860px}
.stdfig figcaption b{color:inherit;filter:brightness(1.25)}
</style>""")

for run in RUNS:
    key, rdir = run["key"], run["dir"]
    m = re.search(r'(<section data-content="' + key + r'"[^>]*>)(.*?)(</section>)', s, re.S)
    if m:
        body = m.group(2)
        head = body.split('<div class="stage">')[0].rstrip()
        prev = re.search(r'<div class="stage" data-notes="">.*?<div class="card">(.*?)</div>\s*</div>\s*$',
                         body, re.S)
        if prev:                                   # rebuilt before: keep notes verbatim
            notes = prev.group(1)
        else:                                      # first rebuild: harvest legacy prose/tables/verdict
            blocks = re.findall(r'(<p>.*?</p>|<div class="tblwrap">.*?</table></div>)', body, re.S)
            verdict = re.search(r'<div class="verdict[^"]*">.*?</div>', body, re.S)
            notes = "\n".join(blocks)
            for x in run.get("extras", []):
                notes += figblock(*x)
            if verdict: notes += "\n" + verdict.group(0)
    else:
        if "new" not in run:
            print(f"!! section {key} absent and no 'new' template — skipped"); continue
        nw = run["new"]
        chips = "".join(f'<span class="chip">{c}</span>' for c in nw["chips"])
        pill = f'<span class="pill {nw["pill"][0]}">{nw["pill"][1]}</span>'
        head = (f'\n  <div class="runhead">\n    <h1>{nw["title"]}</h1>\n'
                f'    <div class="sub">{nw["sub"]}</div>\n'
                f'    <div class="chips">{chips}{pill}</div>\n  </div>')
        notes = nw["notes"]
        for x in run.get("extras", []):
            notes += figblock(*x)
        btn = (f'<button class="runbtn" data-run="{key}"><span class="rn"><span class="dot {nw["dot"]}">'
               f'</span>{nw["btnname"]}</span>'
               + "".join(f'\n     <div class="rm">{l}</div>' for l in nw["meta"]) + '</button>')
        lastbtn = [b for b in re.finditer(r'<button class="runbtn".*?</button>', s, re.S)][-1]
        s = s[:lastbtn.end()] + "\n" + btn + s[lastbtn.end():]
        s = s.replace("</main>", f'<section data-content="{key}" hidden></section>\n</main>')
        m = re.search(r'(<section data-content="' + key + r'"[^>]*>)(.*?)(</section>)', s, re.S)

    stages = {1: "", 2: "", 3: ""}
    for fn, st, title, desc in STD:
        stages[st] += figblock(os.path.join("Output", rdir, fn), title, desc)
    newbody = head + "".join(stage_html(n, stages[n]) for n in (1, 2, 3)) + stage_html(4, notes) + "\n"
    s = s[:m.start(2)] + newbody + s[m.end(2):]
    print(f"{key}: rebuilt ({'notes kept' if m and 'data-notes' in m.group(2) else 'notes harvested'})")

# invariant: every run key exactly once as section and once as button
for run in RUNS:
    k = run["key"]
    ns = len(re.findall(r'<section data-content="' + k + '"[^>]*>', s))
    nb = len(re.findall(r'data-run="' + k + '"', s))
    assert ns == 1 and nb == 1, f"INVARIANT VIOLATION {k}: sections={ns} buttons={nb} — refusing to write"
if not DRY:
    open("run_summary.html", "w").write(s)
print(("DRY " if DRY else "") + f"SIZE: {len(s)//1024} KB")
