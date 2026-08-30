#!/usr/bin/env python3
"""Site builder for the RADiCAL T10 run book.

run_summary.html is the CONTENT STORE (maintained by build_runbook.py; never
published directly). This script splits it into the published site:

  index.html   SUMMARY  — campaign home: at-a-glance, hero timing figure,
                          how-it-works, module comparison, beam characterization
  luag.html    LuAG     — module scan page + all LuAG run pages
  dsb1.html    DSB1     — module scan page + all DSB1 run pages
  ej199.html   EJ199    — skeleton, fills tomorrow

Sidebar: module headings (links across pages) + the current page's runs
(section switching, deep-linkable via #rNN). Everything copied to docs/.

Pipeline per new run: build_runbook.py (updates the store) -> build_site.py
-> cp *.html docs/ -> commit/push.
"""
import base64, os, re, sys
from PIL import Image

WS = os.path.dirname(os.path.abspath(__file__))
os.chdir(WS)

def enc(path, w=1400, q=80):
    im = Image.open(path).convert("RGB")
    if im.width > w: im = im.resize((w, int(im.height*w/im.width)), Image.LANCZOS)
    tmp = "/tmp/_st.jpg"; im.save(tmp, "JPEG", quality=q, optimize=True)
    return "data:image/jpeg;base64," + base64.b64encode(open(tmp,'rb').read()).decode()

store = open("run_summary.html").read()
STYLE = re.search(r'<style>.*?</style>', store, re.S).group(0)
HEAD_LINK = re.search(r'<link rel="stylesheet"[^>]*>', store).group(0)

def section(key):
    m = re.search(r'<section data-content="' + key + r'"[^>]*>(.*?)</section>', store, re.S)
    assert m, f"section {key} missing from store"
    return m.group(1)

def runbtn(key):
    m = re.search(r'<button class="runbtn" data-run="' + key + r'">.*?</button>', store, re.S)
    assert m, f"button {key} missing from store"
    return m.group(0)

MODULES = {
  "luag":  {"title": "LuAG",  "page": "luag.html",
            "tag": "LuAG:Ce crystal fibers", "runs":
            ["r12","r14","r15","r24","r2526","r27","r2829","r30"]},
  "dsb1":  {"title": "DSB1",  "page": "dsb1.html",
            "tag": "DSB:Ce glass fibers", "runs":
            ["r31","r32","r33","r34","r35","r36","r37","r38"]},
  "ej199": {"title": "EJ199", "page": "ej199.html",
            "tag": "EJ-199 plastic + WLS", "runs": []},
}

EXTRA_CSS = """
.modnav a{display:block;padding:9px 12px;margin:2px 0;border-radius:8px;color:inherit;
  text-decoration:none;font-weight:600;font-size:14.5px}
.modnav a small{display:block;font-weight:400;font-size:11.5px;opacity:.65}
.modnav a[aria-current="true"]{background:rgba(14,124,134,.16);color:#0E7C86}
.modnav a:hover{background:rgba(14,124,134,.08)}
.sumgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:14px 18px}
"""

SCRIPT = """
<script>
(function(){
  const btns = document.querySelectorAll('.runbtn');
  const secs = document.querySelectorAll('section[data-content]');
  function show(run, push){
    if (!document.querySelector(`section[data-content="${run}"]`)) return;
    secs.forEach(s => s.hidden = (s.dataset.content !== run));
    btns.forEach(b => b.setAttribute('aria-current', b.dataset.run === run ? 'true' : 'false'));
    if (push) history.replaceState(null, '', '#' + run);
    window.scrollTo(0, 0);
  }
  btns.forEach(b => b.addEventListener('click', () => show(b.dataset.run, true)));
  window.addEventListener('hashchange', () => show(location.hash.slice(1), false));
  const start = location.hash.slice(1);
  show(start && document.querySelector(`section[data-content="${start}"]`) ? start : secs[0].dataset.content, false);
})();
</script>"""

def page(fname, title, brand_small, nav_current, run_keys, sections_html, scan_btn=None):
    nav = ['<div class="brand">RADiCAL T10 Run Book<small>' + brand_small + '</small></div>']
    nav.append('<h2>Modules</h2><div class="modnav">')
    nav.append(f'<a href="index.html" aria-current="{"true" if nav_current=="summary" else "false"}">Summary<small>campaign overview</small></a>')
    for mk, m in MODULES.items():
        cur = "true" if nav_current == mk else "false"
        nav.append(f'<a href="{m["page"]}" aria-current="{cur}">{m["title"]}<small>{m["tag"]}</small></a>')
    nav.append('</div>')
    if scan_btn: nav.append('<h2>Overview</h2>' + scan_btn)
    if run_keys:
        nav.append('<h2>Runs</h2>')
        for rk in run_keys: nav.append(runbtn(rk))
    html = f"""<meta charset="utf-8">
<title>{title}</title>
{HEAD_LINK}
{STYLE.replace('</style>', EXTRA_CSS + '</style>')}
<div class="wrap">
<nav class="rail" aria-label="Navigation">
{chr(10).join(nav)}
</nav>
<main>
{sections_html}
</main>
</div>
{SCRIPT}"""
    open(fname, "w").write(html)
    print(f"{fname}: {len(html)//1024} KB")

# ---------------- LuAG page ----------------
luag_secs = [f'<section data-content="scan">{section("scan")}</section>']
for rk in MODULES["luag"]["runs"]:
    luag_secs.append(f'<section data-content="{rk}" hidden>{section(rk)}</section>')
scan_btn_luag = runbtn("scan")
page("luag.html", "LuAG — RADiCAL T10", "LuAG:Ce · CERN PS T10 · Aug 2026",
     "luag", MODULES["luag"]["runs"], "\n".join(luag_secs), scan_btn_luag)

# ---------------- DSB1 page ----------------
dsb1_scan = f'''  <div class="runhead">
    <h1>DSB1 energy scan — first trends</h1>
    <div class="sub">Runs 31&ndash;38, Aug 29 &middot; 1&ndash;11 GeV, all hardware-tagged &middot; complete: 5 GeV (run 37) and stable 1 GeV (run 38) folded in</div>
    <div class="chips"><span class="chip">6 energies</span><span class="chip">~230k events</span>
    <span class="chip">~2.1&times; the LuAG light</span><span class="pill good">fastest timing of the campaign</span></div>
  </div>
  <div class="card">
    <figure><img src="{enc('Output/scan_dsb1/EnergyScanDSB1.png', 1600, 82)}" alt="DSB1 energy scan trends"><figcaption>Tagged-electron spectra, response, width, and srCFD shower-time trend for the DSB1 module.</figcaption></figure>
  </div>
  <div class="card"><h4>Scan table</h4>
    <div class="tblwrap"><table>
      <tr><th>E [GeV]</th><th>run</th><th>events</th><th>e&#8315; on module</th><th>&Sigma;LG peak</th><th>&sigma;/E</th><th>&sigma;_t mean [ps]</th><th><b>&sigma;_t median [ps]</b></th></tr>
      <tr><td>1</td><td class="t">38</td><td>50,000</td><td>22,745</td><td>ridge-merged (1 GeV material floor)</td><td>&mdash;</td><td class="num">416 &plusmn; 4</td><td class="num"><b>366 &plusmn; 4</b></td></tr>
      <tr><td>3</td><td class="t">35</td><td>20,000</td><td>3,319</td><td class="num">3763 &plusmn; 73</td><td>56.4%</td><td class="num">254 &plusmn; 6</td><td class="num"><b>202 &plusmn; 4</b></td></tr>
      <tr><td>5</td><td class="t">37</td><td>20,000</td><td>856</td><td class="num">6769 &plusmn; 138</td><td>39.7%</td><td class="num">182 &plusmn; 8</td><td class="num"><b>156 &plusmn; 8</b></td></tr>
      <tr><td>7</td><td class="t">33</td><td>20,000</td><td>1,001</td><td class="num">9045 &plusmn; 90</td><td class="num">21.6%</td><td class="num">176 &plusmn; 8</td><td class="num"><b>140 &plusmn; 6</b></td></tr>
      <tr><td>9</td><td class="t">32</td><td>40,000</td><td>847</td><td class="num">9946 &plusmn; 144</td><td>26.5%</td><td class="num">516 &plusmn; 14</td><td class="num"><b>133 &plusmn; 5</b></td></tr>
      <tr><td>11*</td><td class="t">31</td><td>40,000</td><td>303</td><td class="num">10721 &plusmn; 734</td><td>38.6%</td><td class="num">191 &plusmn; 18</td><td class="num"><b>181 &plusmn; 17</b></td></tr>
    </table></div>
    <p>Timing trend (median, contained showers, 3&amp;9 GeV anchors): <span class="num">&sigma;_t &asymp; 323/&radic;E &oplus; 79 ps</span> —
    the constant term nearly half of LuAG&rsquo;s 138 ps, references included unsubtracted. The 9 GeV point
    (<span class="num">133 &plusmn; 5 ps</span>) is the best timing of the campaign; its 516 ps <em>mean</em> against the 133 ps
    <em>median</em> is the strongest daytime-outlier rescue yet. DSB1 delivers ~2.1&times; the LuAG light
    (~1,250&ndash;1,390 ADC-eq/GeV): the scan histograms extend to 16k and at 11 GeV the summed response approaches the
    ~13.6k LG headroom ceiling (rails still &lt;0.02%). Containment floors 0/1500/2500/4000/6000/6500 from the measured spectra. Runs 34 (pion-contaminated tag) and 36 (mixed beam config) are superseded by 37/38 and kept as control/backup datasets. *The 11 GeV point is drawn OPEN in all trend figures: the T10 composition measurements show the electron fraction falling to zero above ~10 GeV/c, so the (real, low-accidental) tags there are likely soft tertiary electrons travelling with the beam — consistent with the depressed peak and width in BOTH modules at 11 GeV. The off-coincidence sample at &minus;11 GeV is ~97% pions: the campaign&rsquo;s purest MIP dataset (runs 30/31).
    High-energy response flattening mirrors LuAG — the cross-run gain ladder and shower-max migration
    (see the LuAG page&rsquo;s linearity forensics), plus the LG ceiling at 11 GeV.</p>
  </div>'''
dsb1_secs = [f'<section data-content="scan">{dsb1_scan}</section>']
for rk in MODULES["dsb1"]["runs"]:
    dsb1_secs.append(f'<section data-content="{rk}" hidden>{section(rk)}</section>')
scan_btn_dsb1 = ('<button class="runbtn" data-run="scan"><span class="rn"><span class="dot good"></span>Energy scan</span>'
                 '<div class="rm">1&ndash;11 GeV trends</div><div class="rm">timing &middot; response &middot; light</div></button>')
page("dsb1.html", "DSB1 — RADiCAL T10", "DSB:Ce · CERN PS T10 · Aug 2026",
     "dsb1", MODULES["dsb1"]["runs"], "\n".join(dsb1_secs), scan_btn_dsb1)

# ---------------- EJ199 skeleton ----------------
ej_sec = '''<section data-content="scan">
  <div class="runhead">
    <h1>EJ199 — awaiting beam</h1>
    <div class="sub">EJ-199 plastic + wavelength shifter &middot; scheduled Aug 30</div>
    <div class="chips"><span class="pill info">runs pending</span></div>
  </div>
  <div class="card"><p>The EJ199 module takes the same 1&ndash;11 GeV scan today. Pages appear here run by run:
  the pipeline (QC &rarr; integrity &rarr; thresholds &rarr; transfer &rarr; ElectronID &rarr; timing), the scan macro, and the page
  template are ready — each run is one registry entry once its file lands in <code>data/download/</code>.</p></div>
</section>'''
page("ej199.html", "EJ199 — RADiCAL T10", "EJ-199 + WLS · CERN PS T10 · Aug 2026",
     "ej199", [], ej_sec, None)

# ---------------- SUMMARY (index) ----------------
hero  = enc("Output/summary/Hero_timing.png", 1500, 84)
how   = enc("Output/summary/HowItWorks.png", 1500, 82)
comp  = enc("Output/summary/ModuleCompare.png", 1500, 82)
pulse = enc("Output/summary/PulseShapes.png", 1400, 82)

index_sec = f'''<section data-content="summary">
  <div class="runhead">
    <h1>RADiCAL at the CERN PS — August 2026</h1>
    <div class="sub">Shower-max sampling modules with three capillary types &middot; T10 beamline, 1&ndash;11 GeV &middot; Aug 27&ndash;30</div>
    <div class="chips"><span class="chip">3 modules (LuAG &middot; DSB1 &middot; EJ199)</span><span class="chip">~300k events</span>
    <span class="chip">6 beam energies</span><span class="chip">hardware e&#8315; tag at every energy</span>
    <span class="pill good">two modules fully analyzed</span></div>
  </div>

  <div class="card"><h4>What we tested</h4>
    <p>RADiCAL modules sample an electromagnetic shower at its maximum with a 2&times;2 array of scintillating
    capillaries (14&times;14 mm face) read out by dual-gain SiPMs into a CAEN DT5742 (DRS4, 5 GS/s). A micro-channel
    plate upstream provides the ~10 ps time reference; two threshold Cherenkov counters tag electrons in hardware
    at every energy. Three capillary materials share identical geometry and readout — only the material changes.</p>
    <div class="tblwrap"><table>
      <tr><th>module</th><th>capillary material</th><th>runs</th><th>events</th><th>status</th></tr>
      <tr><td class="t">LuAG</td><td>LuAG:Ce crystal fibers</td><td>12&ndash;30</td><td class="num">~168k</td><td>6-point scan complete</td></tr>
      <tr><td class="t">DSB1</td><td>DSB:Ce glass fibers</td><td>31&ndash;38</td><td class="num">~230k</td><td>6-point scan complete</td></tr>
      <tr><td class="t">EJ199</td><td>EJ-199 plastic + WLS</td><td>&mdash;</td><td class="num">&mdash;</td><td>beam scheduled Aug 30</td></tr>
    </table></div></div>

  <div class="card">
    <figure><img src="{hero}" alt="Shower timing versus energy for both modules"><figcaption><b>The headline.</b>
    Shower-time resolution vs beam energy for tagged electrons. DSB1 reaches <b>133 &plusmn; 5 ps at 9 GeV</b> and 156 &plusmn; 8 at 5 GeV with the
    MCP and digitizer reference jitter still included — the trend&rsquo;s constant term is 79 ps vs LuAG&rsquo;s 138 ps.
    Median 4-capillary combination; no reference subtraction. Open 11 GeV points: electron purity uncertain above
    ~10 GeV/c (T10 composition) — kept, not trusted.</figcaption></figure>
  </div>

  <div class="card"><h4>How the measurement works — one real event</h4>
    <figure><img src="{how}" alt="Annotated tagged-electron event in four steps"><figcaption>A single 5 GeV electron
    from LuAG run 15, in four steps: the two Cherenkov counters identify it, the MCP timestamps it, the four
    low-gain channels measure the shower energy, and the high-gain copy&rsquo;s threshold crossing gives the
    per-capillary time. The shower time is the median of the four capillaries.</figcaption></figure></div>

  <div class="card"><h4>Materials, compared</h4>
    <figure><img src="{comp}" alt="Timing and response for both modules"><figcaption><b>Timing and response.</b>
    DSB1 is faster at every energy and delivers ~2.1&times; the light (~1,250&ndash;1,390 vs ~660 ADC-eq/GeV).
    Response linearity for both modules is established to the &plusmn;15% cross-run gain envelope (see the LuAG
    page&rsquo;s linearity forensics: the apparent high-energy &ldquo;saturation&rdquo; is a SiPM temperature-gain ladder
    plus calculable shower-max migration, not the detectors).</figcaption></figure>
    <figure><img src="{pulse}" alt="Mean pulse shape per material"><figcaption><b>Why DSB1 is faster.</b>
    Mean tagged-electron pulse on the same &minus;7 GeV beam: identical readout-limited 4.4 ns rise, but LuAG&rsquo;s
    scintillation tail (&tau; &asymp; 12.5 ns, 32% of light after 8 ns) is nearly three times longer than DSB1&rsquo;s
    (&tau; &asymp; 4.9 ns, 16%). Less late light &rarr; a smaller timing constant term.</figcaption></figure></div>

  <div class="card"><h4>What we measured about the T10 beam itself</h4>
    <p>Running two threshold Cherenkovs in coincidence at every momentum makes the experiment a beam monitor.
    In-situ electron fractions (coincidence at the per-energy working points):</p>
    <div class="tblwrap"><table>
      <tr><th>p [GeV/c]</th><th>+1</th><th>+3</th><th>+5</th><th>&minus;7</th><th>&minus;9</th><th>&minus;11</th></tr>
      <tr><td class="t">measured e&#8315; coincidence</td><td class="num">85.9&ndash;88.8%</td><td class="num">23.3&ndash;23.5%</td><td class="num">5.7&ndash;9.1%&dagger;</td><td class="num">8.6&ndash;8.8%</td><td class="num">&ge;4.9%</td><td class="num">2.2&ndash;2.4%&Dagger;</td></tr>
      <tr><td class="t">T10 guide tables</td><td class="num">80.6%</td><td class="num">18.0%</td><td class="num">4.1%</td><td class="num">2.3%</td><td class="num">&mdash;</td><td class="num">&mdash;</td></tr>
    </table></div>
    <p>&dagger;The +5 GeV fraction depends on the collimation configuration: 9.1% with the early tight-acceptance settings (run 15), 5.7% with acceptance collimators open (run 37) — composition is not a constant of the momentum setting alone. &Dagger;At &minus;11 GeV the published composition has e&#8315;&rarr;0; our (real) coincidences there are likely soft tertiary electrons, so we quote a tag rate, not a beam e&#8315; fraction. Above ~5 GeV we otherwise consistently find <b>more electrons than the published tables</b> — reproducible day-to-day
    at the few-percent level across two modules. Other beam findings: MCP pulse height is species-correlated
    (electrons pulse smaller — a high trigger threshold silently rejects them); ~48 triggers/spill at 1 GeV with
    run-rate differences driven by supercycle cadence, not intensity; and XCET radiator gas at low-energy pressures
    is beamline material the machine bookkeeping assumes away — 11 bar of CO&#8322; is ~17% X&#8320; and visibly widens
    the beam. A 1.5 bar setting at +5 GeV radiates on pions too: coincidence 69.7% = the beam&rsquo;s combined
    e+&mu;+&pi; content, measured to a percent.</p></div>

  <div class="card"><h4>Campaign log — the short version</h4>
    <p class="t">Aug 27 &middot; LuAG first beam (run 12), channel map verified, clipped-pulse recovery adopted (never
    discard clipped high-gain: predict the true peak from the low-gain transfer line, fit only below the clip wall).<br>
    Aug 28 &middot; module centered; bias +1 V (final); MCP threshold lowered 240&rarr;137 mV after discovering the
    species-correlated pulse height (~10&times; more electrons/minute); LuAG scan 1&ndash;7 GeV in one day.<br>
    Aug 28&ndash;29 &middot; overnight 9 GeV; morning &ldquo;drift&rdquo; traced to an ambient light leak, not temperature;
    the per-event <b>median</b> capillary combination adopted after rescuing the noisy periods.<br>
    Aug 29 &middot; 11 GeV point — the hardware tag works even at 0.06 bar; wide beam at the T10 momentum limit;
    apparent high-energy saturation investigated and retired (gain ladder + shower-max migration).
    DSB1 module: full 1&ndash;11 GeV scan in one day, fastest timing of the campaign.<br>
    Aug 29&ndash;30 &middot; 1 GeV rate mystery solved at the collimators + XCET gas; stable retakes overnight.
    EJ199 up next.</p>
    <p>Every number above is reproducible from the repo: <code>runs/runs.json</code> (provenance),
    <code>Output/run_12/FINDINGS.md</code> (the full log, including the mistakes), and <code>macros/</code>
    (every plot&rsquo;s source). Run pages carry the complete standard plot set for every run.</p></div>

  <div class="card"><h4>Next</h4>
    <p>EJ199 scan (today); run 37 + stable 1 GeV into the DSB1 scan; connection of all three capillary types to the
    earlier high-energy campaigns once that data is staged — cross-campaign comparisons will use shape-normalized
    quantities (timing trends, &sigma;/E) pending a per-run gain reference (planned: temperature logging + pulser,
    muon-stopper MIP anchor).</p></div>
</section>'''
page("index.html", "RADiCAL T10 Run Book", "CERN PS T10 · Aug 2026 · LuAG / DSB1 / EJ199",
     "summary", [], index_sec, None)

# ---------------- copy to docs ----------------
os.system("cp index.html luag.html dsb1.html ej199.html docs/")
print("copied to docs/")

# ---------------- artifact variant: absolute links for the claude.ai copy ----
PAGES = "https://jwwetzel.github.io/radical-t10-2026/"
art = open("index.html").read()
for p in ("index.html", "luag.html", "dsb1.html", "ej199.html"):
    art = art.replace(f'href="{p}"', f'href="{PAGES}{p}" target="_blank"')
open("/private/tmp/claude-501/-Users-jameswetzel-Documents/7538d0d0-b27d-467b-8254-3b3949a82e69/scratchpad/artifact_index.html", "w").write(art)
print("artifact variant written")
