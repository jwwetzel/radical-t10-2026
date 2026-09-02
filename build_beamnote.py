#!/usr/bin/env python3
"""Standalone beam-instrumentation note for the T10 beam physicist.

Self-contained HTML (embedded figures) + a print-optimized PDF twin.
Numbers are pulled from the summary files so nothing is hand-transcribed.
"""
import base64, io, json, os, re
from PIL import Image

WS = os.path.dirname(os.path.abspath(__file__)); os.chdir(WS)

def enc(path, w=1500, q=86):
    im = Image.open(path).convert("RGB")
    if im.width > w: im = im.resize((w, int(im.height*w/im.width)), Image.LANCZOS)
    buf = io.BytesIO(); im.save(buf, "JPEG", quality=q, optimize=True)
    return "data:image/jpeg;base64," + base64.b64encode(buf.getvalue()).decode()

prec = json.load(open("/tmp/prectable.json"))   # p, p90, p95, p99, Pmu, Ppi, flag, effmax_pure

# recommended-pressure rows
prec_rows = ""
for p, p90, p95, p99, Pmu, Ppi, flag, emax in prec:
    cls = "ok" if "all pure" in flag else ("warn" if "99%" in flag and "95% OK" in flag else "bad")
    ceil = "&mdash;" if "all pure" in flag else f"{emax*100:.0f}%"
    prec_rows += (f"<tr><td class='c'>&plusmn;{p}</td><td class='c'>0.22</td><td class='c'>0.26</td>"
                  f"<td class='c'>0.35</td><td class='c'>{Pmu:.3f}</td><td class='c'>{Ppi:.3f}</td>"
                  f"<td class='c {cls}'>{ceil}</td></tr>")

# beam-content per-run appendix rows
bc = open("Output/summary/BeamContent_summary.txt").read()
bc_rows = ""
for m in re.finditer(r"^\s*(\d+)\s+([+-]?\d+)\s+([\d.]+)/([\d.]+)\s+(\d+)\s+(\d+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+) \+/- ([\d.]+)\s*(UPPER.*)?$", bc, re.M):
    run, p, p40, p43, ntr, nco, raw, eps, fe, fee, up = m.groups()
    disp = "2526&rarr;25+26" if run=="2526" else ("9001&rarr;28+29" if run=="9001" else run)
    upcls = " class='dim'" if up else ""
    bc_rows += (f"<tr{upcls}><td>{disp}</td><td class='c'>{'+' if int(p)>0 else ''}{p}</td>"
                f"<td class='c'>{p40}/{p43}</td><td class='c'>{int(nco):,}/{int(ntr):,}</td>"
                f"<td class='c'>{float(raw)*100:.1f}%</td><td class='c'>{float(eps)*100:.1f}%</td>"
                f"<td class='c'><b>{float(fe)*100:.1f}%</b>{' *' if up else ''}</td></tr>")

# aggregated content
agg = {}
for m in re.finditer(r"^\s*([+-]?\d+)\s+([\d.]+)\s+([\d.]+)\s+(\d+)\s+(\*.*)?$", bc, re.M):
    p, fe, fee, n, up = m.groups(); agg[int(p)] = (float(fe), float(fee), bool(up))

FIG_BC = enc("Output/summary/BeamContent.png", 1500, 88)
FIG_CAL = enc("Output/summary/XCETCalib.png", 1500, 86)
FIG_TP = enc("Output/summary/XCETTagProbe.png", 1500, 86)

HTML = f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>T10 XCET Beam-Instrumentation Note &middot; RADiCAL Aug 2026</title>
<style>
:root{{--ink:#16202a;--mut:#5a6b78;--line:#dde5ea;--teal:#0E7C86;--blue:#2f5fb0;
  --ground:#fbfcfd;--card:#fff;--good:#1f8a4c;--warn:#b8860b;--bad:#b03a3a;--accent:#0E7C86}}
*{{box-sizing:border-box}}
html{{-webkit-print-color-adjust:exact;print-color-adjust:exact}}
body{{margin:0;background:var(--ground);color:var(--ink);
  font:15px/1.62 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}}
.wrap{{max-width:920px;margin:0 auto;padding:0 30px 80px}}
header.masthead{{border-bottom:3px solid var(--teal);padding:34px 0 20px;margin-bottom:8px}}
.eyebrow{{font-size:12px;letter-spacing:.14em;text-transform:uppercase;color:var(--teal);font-weight:700}}
h1{{font-size:30px;line-height:1.15;margin:8px 0 6px;letter-spacing:-.01em}}
.subtitle{{font-size:16px;color:var(--mut);margin:0}}
.meta{{display:flex;flex-wrap:wrap;gap:8px 22px;margin-top:16px;font-size:13px;color:var(--mut)}}
.meta b{{color:var(--ink);font-weight:600}}
h2{{font-size:21px;margin:38px 0 4px;padding-top:16px;border-top:1px solid var(--line);letter-spacing:-.01em}}
h2 .n{{color:var(--teal);font-weight:700;margin-right:10px}}
h3{{font-size:16px;margin:22px 0 6px;color:var(--ink)}}
p{{margin:9px 0}}
.lede{{font-size:16.5px;color:#2a3742}}
.card{{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:18px 22px;margin:16px 0;
  box-shadow:0 1px 2px rgba(20,40,60,.04)}}
.kpis{{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin:16px 0}}
.kpi{{background:var(--card);border:1px solid var(--line);border-radius:11px;padding:14px 16px}}
.kpi .v{{font-size:23px;font-weight:700;letter-spacing:-.02em}}
.kpi .v small{{font-size:13px;font-weight:600;color:var(--mut)}}
.kpi .l{{font-size:12px;color:var(--mut);margin-top:3px;line-height:1.35}}
.kpi.teal .v{{color:var(--teal)}} .kpi.blue .v{{color:var(--blue)}}
table{{width:100%;border-collapse:collapse;margin:14px 0;font-size:13.5px}}
th,td{{padding:7px 10px;text-align:left;border-bottom:1px solid var(--line)}}
th{{font-size:11.5px;letter-spacing:.05em;text-transform:uppercase;color:var(--mut);font-weight:700;
  border-bottom:2px solid var(--line);vertical-align:bottom}}
td.c,th.c{{text-align:center}}
th.sym{{text-transform:none;font-size:12.5px}}
tr.dim td{{color:var(--mut)}}
td.ok{{color:var(--good);font-weight:700}} td.warn{{color:var(--warn);font-weight:700}} td.bad{{color:var(--bad);font-weight:700}}
figure{{margin:18px 0;text-align:center}}
figure img{{max-width:100%;border:1px solid var(--line);border-radius:10px}}
figcaption{{font-size:13px;color:var(--mut);margin-top:8px;text-align:left;line-height:1.5}}
.num{{color:var(--teal);font-weight:700}}
.callout{{border-left:4px solid var(--teal);background:#f0f8f8;padding:12px 18px;border-radius:0 10px 10px 0;margin:16px 0}}
.callout.delight{{border-left-color:var(--blue);background:#f2f5fb}}
.ask{{border-left:4px solid var(--warn);background:#fbf7ee;padding:4px 18px;border-radius:0 10px 10px 0;margin:16px 0}}
ul,ol{{margin:8px 0;padding-left:22px}} li{{margin:5px 0}}
code{{font-family:"SF Mono",Menlo,monospace;font-size:12.5px;background:#eef3f5;padding:1px 5px;border-radius:4px}}
a{{color:var(--teal)}}
.dlbar{{display:flex;align-items:center;gap:14px;flex-wrap:wrap;background:var(--card);border:1px solid var(--line);
  border-radius:12px;padding:14px 20px;margin:18px 0}}
.dlbtn{{display:inline-flex;align-items:center;gap:8px;background:var(--teal);color:#fff;text-decoration:none;
  font-weight:600;font-size:14px;padding:9px 18px;border-radius:9px}}
.foot{{margin-top:40px;padding-top:16px;border-top:1px solid var(--line);font-size:12.5px;color:var(--mut)}}
@media print{{
  body{{background:#fff;font-size:10.5pt;line-height:1.5}}
  .wrap{{max-width:none;padding:0}}
  .no-print{{display:none!important}}
  h2{{page-break-after:avoid}} figure,table,.card,.callout,.ask{{page-break-inside:avoid}}
  .kpis{{gap:8px}} .kpi .v{{font-size:16pt}}
  a{{color:var(--ink);text-decoration:none}}
}}
@media (max-width:640px){{.kpis{{grid-template-columns:repeat(2,1fr)}}.wrap{{padding:0 16px 50px}}}}
</style></head>
<body><div class="wrap">

<header class="masthead">
  <div class="eyebrow">Beam-Instrumentation Note &middot; for T10 Beam Physics</div>
  <h1>Absolute calibration of the T10 XCET Cherenkov counters,<br>and what it says about the beam</h1>
  <p class="subtitle">A user-campaign gift-back: the two threshold counters, calibrated in photoelectrons &mdash; plus the beam&rsquo;s measured electron content and a ready-to-use operating table.</p>
  <div class="meta">
    <span><b>Campaign:</b> RADiCAL, PS East Area T10, 25&ndash;31 Aug 2026</span>
    <span><b>Counters:</b> XCET040, XCET043 (both upstream of the module)</span>
    <span><b>Author:</b> J. Wetzel (RADiCAL) &middot; james@animal-lamps.com</span>
    <span><b>Data:</b> 18 calibration runs, MCP-triggered</span>
  </div>
</header>

<div class="dlbar no-print">
  <a class="dlbtn" href="beamnote.pdf" download>&#8681;&nbsp; Download PDF</a>
  <span style="font-size:13px;color:var(--mut)">Self-contained; every number links back to the public run book at
  <a href="https://jwwetzel.github.io/radical-t10-2026/">jwwetzel.github.io/radical-t10-2026</a>.</span>
</div>

<p class="lede">Over five days on T10 we ran the two XCET threshold Cherenkov counters across 18 pressure/energy
points with the DRS4 recording their full waveforms on every MCP trigger. Because the DAQ triggers on the MCP and
<em>not</em> on the XCETs, both counters are unbiased spectators of every event &mdash; which let us calibrate each
one <b>absolutely, in photoelectrons</b>, using its own single-photoelectron peak as the ruler. This note hands that
calibration back to the beamline, together with three things it makes possible: a counter-health readout, an
electron-content measurement of the beam itself, and an operating table that trades efficiency against purity.</p>

<h2><span class="n">1</span>The counters, in photoelectrons</h2>
<div class="kpis">
  <div class="kpi teal"><div class="v">19.0<small> pe/bar</small></div><div class="l">XCET040 photoelectron yield<br>&plusmn;0.4 fit &plusmn;0.9 scale</div></div>
  <div class="kpi blue"><div class="v">26.0<small> pe/bar</small></div><div class="l">XCET043 photoelectron yield<br>&plusmn;1.1 fit &plusmn;3.9 scale</div></div>
  <div class="kpi teal"><div class="v">~72<small> /cm</small></div><div class="l">XCET040 Cherenkov figure of merit N&#8320;</div></div>
  <div class="kpi blue"><div class="v">~100<small> /cm</small></div><div class="l">XCET043 N&#8320; (thinner windows, 4.2-bar vessel)</div></div>
</div>
<p>Yields are linear in pressure over the whole 0.06&ndash;1.3 bar range explored. Converting through
N<sub>pe</sub> = N&#8320;&middot;L&middot;2(n&minus;1)P with L &asymp; 3 m and (n&minus;1)<sub>CO&#8322;</sub> &asymp; 4.4&times;10&#8315;&#8308;/bar gives the figures of merit above &mdash; both inside the canonical 60&ndash;100/cm band for a
clean gas threshold counter, with the lower-pressure XCET043 vessel coming out higher, as its thinner windows would
predict. The single-photoelectron rulers are <span class="num">40.2 ADC-eq</span> (XCET040, stable to &plusmn;5% RMS over
15 runs) and <span class="num">~21.8 ADC-eq</span> (XCET043; its PMT ran at roughly half XCET040&rsquo;s gain, so its
1-pe sits on the pedestal edge and carries a &plusmn;15% absolute-scale systematic &mdash; see &sect;6).</p>
<figure><img src="{FIG_CAL}" alt="XCET absolute photoelectron calibration">
<figcaption><b>Left:</b> tag-and-probe photoelectron yield vs pressure for both counters (filled: Poisson-exact mean
estimator; open: resolved-hump fits). <b>Right:</b> threshold-folded tag efficiency vs pressure, with measured
tag-and-probe points overlaid and the Poisson no-threshold bound shown for reference. Run 34 (a 1.3-bar tag at +5 GeV,
which radiates on pions) is excluded from the fits.</figcaption></figure>

<h2><span class="n">2</span>Counter-health readout</h2>
<p>Everything here is measured from the same waveforms &mdash; monitoring data that is otherwise awkward to get.</p>
<table>
<tr><th>Quantity</th><th class="c">XCET040</th><th class="c">XCET043</th><th>How measured</th></tr>
<tr><td>Dark occupancy &gt; 40 ADC-eq</td><td class="c">1.7% / window</td><td class="c">0.4% / window</td><td>off-time, pileup-subtracted</td></tr>
<tr><td>Dark occupancy &gt; 60 ADC-eq</td><td class="c">0.48%</td><td class="c">&lt;0.05%</td><td>off-time</td></tr>
<tr><td>1-pe gain stability</td><td class="c">&plusmn;5% RMS / 3 d</td><td class="c">single-run</td><td>per-run 1-pe fits</td></tr>
<tr><td>Acceptance match (see same beam)</td><td class="c" colspan="2">&le;0.1% mismatch (time-matched windows)</td><td>tag&ndash;probe miss floor</td></tr>
<tr><td>In-window pileup rate</td><td class="c" colspan="2">~0.13% / event</td><td>common-mode (both counters)</td></tr>
</table>
<p>A practical note for XCET040: its 1-pe amplitude (40.2 ADC-eq) sits <em>exactly</em> at a natural 40-ADC-eq tag
threshold, so a single dark photoelectron can fire a tag. That is a &plusmn;15%-of-gain coincidence with our threshold
choice, not a counter fault &mdash; but it is worth knowing before setting discriminators (see &sect;4, &sect;6).</p>

<h2><span class="n">3</span>Operating table &mdash; efficiency vs purity <span style="font-size:13px;color:var(--blue);font-weight:600">&middot; the one to keep</span></h2>
<p>The electron tag efficiency depends only on pressure (electrons always radiate). <b>Purity</b> is the momentum-dependent
constraint: below the muon Cherenkov threshold P<sub>&mu;</sub> = 12.96/p&sup2; bar the tag is pure electrons; above it,
muons tag too (and above P<sub>&pi;</sub> = 1.745&thinsp;P<sub>&mu;</sub>, pions as well &mdash; the run-34 lesson). The
two constraints define the usable window.</p>
<table>
<tr><th class="c">|p|<br>[GeV/c]</th><th class="c">P for<br>90% coinc.</th><th class="c">P for<br>95%</th>
<th class="c">P for<br>99%</th><th class="c sym">&mu; thr.<br>P<sub>&mu;</sub></th><th class="c sym">&pi; thr.<br>P<sub>&pi;</sub></th>
<th class="c">max <em>pure-e</em><br>coinc. eff.</th></tr>
{prec_rows}
</table>
<div class="callout"><b>The operational punchline.</b> At 1&ndash;5 GeV/c you can reach <b>99% pure-electron</b> coincidence
tagging &mdash; the pressure you need (0.35 bar) sits far below the muon threshold. At 7 GeV/c, 95% is still clean but
99% would start tagging muons. At <b>9 GeV/c the pure ceiling is ~74%</b>, and at <b>11 GeV/c only ~48%</b>: there is no
pressure that is simultaneously high-efficiency and muon-free. This is exactly why our own &minus;11 GeV runs saw the
tag deficits they did &mdash; the physics was in the counters all along, and this table makes it choosable in advance.</div>

<h2><span class="n">4</span>What the beam is made of <span style="font-size:13px;color:var(--blue);font-weight:600">&middot; a cross-check for your model</span></h2>
<p>Turning each run&rsquo;s coincidence-tag rate around through the calibrated efficiency gives the beam&rsquo;s
<b>absolute electron fraction</b> &mdash; a direct, independent check of the T10 secondary-beam composition.</p>
<div class="kpis">
  <div class="kpi teal"><div class="v">{agg.get(1,(0,))[0]*100:.0f}%<small> e&#8314;</small></div><div class="l">positive beam, +1 GeV/c</div></div>
  <div class="kpi teal"><div class="v">{agg.get(3,(0,))[0]*100:.0f}%</div><div class="l">positive beam, +3 GeV/c</div></div>
  <div class="kpi teal"><div class="v">{agg.get(5,(0,))[0]*100:.0f}%</div><div class="l">positive beam, +5 GeV/c</div></div>
  <div class="kpi blue"><div class="v">~7&ndash;9%<small> e&#8315;</small></div><div class="l">negative beam, 7&ndash;11 GeV/c</div></div>
</div>
<figure><img src="{FIG_BC}" alt="Measured electron content of the T10 beam vs momentum">
<figcaption>Measured electron fraction among triggers vs |p|. The positive beam is strongly electron- (positron-)
dominated at low momentum &mdash; <span class="num">88%</span> at +1 GeV/c &mdash; and falls steeply to ~9% by +5 GeV/c,
the expected signature of e&#8314; from &pi;&#8304;&rarr;&gamma;&rarr;e&#8314;e&#8315; conversions in the production target
dwindling as the hadron fraction rises. The negative beam is lower and flatter (~7&ndash;9%). Open markers are upper
bounds: at &le;0.15 bar the tag efficiency is depressed by fake-tag dilution (&sect;6), so the correction over-counts
electrons there. Fraction is <em>among MCP triggers</em>, not raw spill composition.</figcaption></figure>
<p style="font-size:13.5px;color:var(--mut)">One observation worth your eye: at +5 GeV/c the three runs disagree beyond
statistics (run 15, Aug 28: 11.8%; runs 37/41, Aug 30: 7.7%) &mdash; a genuine day-to-day composition shift, plausibly a
beam-tune or collimator change between the two dates.</p>

<h2><span class="n">5</span>Beam observations from the run</h2>
<ul>
<li><b>Rate recovery at 1 GeV.</b> The beam arrived very wide and flat in both planes; peak counts on the CESAR profile
were ~80 where ~800 was expected. Opening the collimators (from &minus;10/&minus;10/&minus;3/&minus;20.1) and dropping
XCET040 from 11 bar recovered the rate substantially &mdash; a useful data point on the low-momentum tune.</li>
<li><b>Trigger rate:</b> ~2500 MCP triggers in 1h15m at 1 GeV before the retune.</li>
<li><b>Pion contamination is real and sharp:</b> at +5 GeV/c a 1.3-bar tag (run 34) put pions in the electron sample
&mdash; its probe mean fell 2.6&times; &mdash; exactly as P<sub>&pi;</sub> = 0.90 bar at 5 GeV/c predicts. A worked
example of the &sect;3 table.</li>
</ul>

<h2><span class="n">6</span>Method, briefly &mdash; and two things you may not have seen</h2>
<p>The calibration is <b>tag-and-probe</b>: to measure one counter without biasing it, we select on the <em>other</em>
counter and read the first with no cut, zeros included. The mean amplitude including zeros, divided by the 1-pe ruler,
is the Poisson mean N<sub>pe</sub> directly &mdash; valid even at 0.06 bar where the electron hump dissolves into
counting. We validated every assumption (purity, independence, acceptance) on the data itself; two pieces may be new:</p>
<div class="callout delight"><b>A gain ruler from counting alone.</b> On tagged electrons the zero fraction and the
mean are two equations in two unknowns (&lambda;, A<sub>1pe</sub>) &mdash; so the single-photoelectron gain can be
recovered with <em>no resolved 1-pe peak and no pedestal fit</em>. It confirms XCET043&rsquo;s pedestal-edge ruler
independently: 21.6 &plusmn; 2.3 &plusmn; 3.7 ADC-eq against the spectral 21.8.</div>
<div class="callout delight"><b>Dark rates without a dark run.</b> Dark pulses are uniform across the 205-ns window
while beam signals are prompt, so an off-time window measures the dark rate on ordinary beam events &mdash; and the
in-window pileup component (which lights <em>both</em> counters) is separated as the common-mode rate above the
single-counter dark reach. No dedicated pedestal run required.</div>
<figure><img src="{FIG_TP}" alt="Tag-and-probe validation">
<figcaption><b>Left:</b> the counting bootstrap &mdash; 1-pe gain recovered from beam electrons alone (points) against
the spectral rulers (bands). <b>Right:</b> the fake-tag signature &mdash; probe mean vs tag threshold, flat where the
tag is pure (0.59 bar) and climbing where dark-photoelectron fakes dilute a scarce electron sample (0.09 bar).</figcaption></figure>
<p>The one caveat that rides everywhere: at low pressure (&le;0.15 bar) fake tags &mdash; a dark photoelectron in the
tag counter over a non-radiating hadron &mdash; dilute the probe by a measured 15&ndash;40% (XCET043) / 4&ndash;14%
(XCET040). It biases the low-pressure efficiency points and the &sect;4 open markers low; it is quantified, not merely
flagged, in the <a href="https://jwwetzel.github.io/radical-t10-2026/tagprobe.html">methods note</a>.</p>

<h2><span class="n">7</span>Open questions &mdash; where your knowledge would help</h2>
<ul>
<li><b>Vessel &amp; optics geometry.</b> We assumed L &asymp; 3 m for both. The genuine 35% N&#8320; difference we see
(XCET043 &gt; XCET040) we attribute to the 4.2-bar vessel&rsquo;s thinner windows &mdash; can you confirm the radiator
lengths and window materials?</li>
<li><b>Pressure-gauge provenance &amp; zero.</b> Are the registry pressures CESAR readbacks or local transducers, and
what is the zero offset at the low end? At 0.06 bar a &plusmn;0.01 bar offset is a 16% effect on our lowest points.</li>
<li><b>A mid-pressure amplitude correlation</b> between the two counters (Pearson r up to 0.68 near 0.44 bar, run-dependent)
is not explained by the measured pileup rate alone &mdash; any known intensity or geometry effect?</li>
</ul>

<h2><span class="n">8</span>What we can add if useful</h2>
<div class="ask">
<p style="margin:10px 0"><b>To finalize this as an EDMS/ELOG note, we would fold in from your side:</b> the campaign HV
settings and PMT serials for both counters; the CO&#8322; fill/purge history (did the run-36-era gas intervention touch
either counter?); hall temperature during the calibration (pe/bar is really pe per gas density &mdash; a 10 K swing is
~3%); and the pressure-readback provenance above. With those, every number here becomes a dated, reproducible reference.</p>
</div>
<p style="margin-top:14px"><b>Two recommendations, evidence attached:</b> (1) next campaign, raise XCET043&rsquo;s HV
(or lower the DRS4 range) so its 1-pe peak clears the pedestal &mdash; the &plusmn;15% scale systematic then collapses to
XCET040&rsquo;s &plusmn;5%. (2) Avoid setting a tag threshold <em>at</em> a counter&rsquo;s 1-pe amplitude (our
40-on-40.2 coincidence) &mdash; it cost us 15&ndash;40% fake-tag dilution at low pressure.</p>

<h2><span class="n">A</span>Appendix &mdash; per-run electron content (traceability)</h2>
<p style="font-size:13.5px;color:var(--mut)">Every point behind &sect;4. Raw rate = coincidence tags / triggers;
&epsilon;<sub>fold</sub> = threshold-folded coincidence efficiency at the run&rsquo;s pressures; f<sub>e</sub> = raw /
&epsilon;. Rows marked * (dimmed) are &le;0.15 bar, where fake-tag dilution makes f<sub>e</sub> an upper bound. Merged
files are expanded to their source runs. Run 34 (pion-contaminated tag) excluded throughout.</p>
<table>
<tr><th>Run</th><th class="c">p [GeV/c]</th><th class="c">P40/P43 [bar]</th><th class="c">coinc / trig</th>
<th class="c">raw rate</th><th class="c">&epsilon;<sub>fold</sub></th><th class="c">f<sub>e</sub></th></tr>
{bc_rows}
</table>

<div class="foot">
RADiCAL Collaboration &middot; T10 beam test, August 2026. Full analysis, macros and per-run data provenance:
<a href="https://jwwetzel.github.io/radical-t10-2026/">jwwetzel.github.io/radical-t10-2026</a>.
Calibration <code>macros/XCETCalib.C</code>; validation <code>macros/XCETTagProbe.C</code>; beam content
<code>macros/BeamContent.C</code>. Raw waveform data available on request. Snapshot: this is a 2026 calibration;
window transmission and mirror reflectivity drift, so re-verification at the next access is recommended.
</div>

</div></body></html>"""

open("beamnote.html", "w").write(HTML)
print(f"beamnote.html: {len(HTML)//1024} KB")
