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
  4. cp run_summary.html docs/index.html && git commit && git push
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
    tmp = "/tmp/_rb.jpg"; im.save(tmp, "JPEG", quality=q, optimize=True)
    return "data:image/jpeg;base64," + base64.b64encode(open(tmp,'rb').read()).decode()

# ---- canonical plot sequence: (filename, stage, title, description) ----
STD = [
 ("Integrity_waveforms.png", 1, "Waveform persistence",
  "Every event in the run overlaid per channel (log color scale); teal = mean tagged-electron waveform. "
  "One glance confirms channel identity, polarity, and timing windows, and exposes dead or noisy channels."),
 ("Integrity_health.png", 1, "Channel health",
  "Per-channel baseline RMS, signal fraction, rail (clip) fraction, and median pulse peak-time. "
  "Event gaps and rails surface here first; the LG rail fraction is the bias-headroom watch item."),
 ("QCv2_MIP.png", 2, "MIP calibration",
  "Off-coincidence (hadron/muon) HG spectra with Landau fits per capillary. The MIP MPV is the per-run "
  "gain reference that absorbs day-scale gain drift; run 15 anchors the campaign calibration."),
 ("TransferFit.png", 2, "HG&harr;LG transfer, wall-aware",
  "HG vs LG amplitude with the linear transfer fit restricted to the region below the HG clip wall — "
  "the fit range is drawn explicitly. The line predicts the true amplitude of clipped pulses, so no "
  "clipped high-gain signal is ever discarded."),
 ("MCPThreshold.png", 2, "MCP trigger threshold",
  "TR0 amplitude spectrum and trigger turn-on giving the measured hardware threshold. Final working "
  "point 137 mV — low enough to keep the smaller electron pulses (the old 240 mV rejected them)."),
 ("XCETCheck.png", 2, "XCET spectra &amp; threshold",
  "Cherenkov photoelectron spectra and coincidence rate vs software threshold. The electron-tag "
  "threshold is set on the coincidence plateau, per energy."),
 ("ElectronID.png", 2, "Electron ID tag-and-probe",
  "Selector variables for XCET-tagged electrons vs off-coincidence hadrons. Trained at the tagged "
  "energies, this carries the electron selection to beam energies the XCETs cannot reach."),
 ("QCv2_align.png", 3, "&Sigma;LG spectrum &amp; containment",
  "Summed low-gain spectrum, pulse peak-time containment, and row/column balance — shower containment "
  "and beam alignment on the 2&times;2 capillary module."),
 ("BestTiming.png", 3, "srCFD shower timing",
  "Per-capillary &Delta;t to the MCP using srCFD (threshold 0.15&times; the LG-predicted true amplitude) "
  "and the combined shower time. Median combination adopted; mean kept alongside for comparison."),
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
    m = re.search(r'(<section data-content="' + key + r'">)(.*?)(</section>)', s, re.S)
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

if not DRY:
    open("run_summary.html", "w").write(s)
print(("DRY " if DRY else "") + f"SIZE: {len(s)//1024} KB")
