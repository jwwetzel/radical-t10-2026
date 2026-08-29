# RADiCAL TB — CERN T10, August 2026

Analysis workspace for the August 2026 RADiCAL module beam test at CERN T10.

## Layout

- `data/` — symlink to `01_Data/2026-08-CERN_T10/` (raw run files + DAQ metadata)
- `runs/runs.json` — run registry: one entry per run with conditions and notes
- `macros/` — ROOT analysis macros
- `Output/run_<N>/` — per-run outputs (histograms, PDFs, summaries)

## Data format

Tree `pulse` (DRS-style digitizer, ROOT output from the DAQ):

| branch | type | meaning |
|---|---|---|
| `event` | `int` | event number |
| `trigger_time_tag` | `uint` | board trigger time tag |
| `channel` | `float[18][1024]` | raw waveforms, ADC counts (0–65535) |
| `times` | `float[2][1024]` | per-group sample times |
| `tc` | `ushort[2]` | per-group trigger cell |

Record length 1024 samples, post-trigger 70%. Per-channel DC offsets are in
`data/run_metadata.json`. ADC range is effectively 12-bit (0–4095), SiPM
baselines at ~2048.

### Channel map (Aug 2026 T10)

| ch | role |
|---|---|
| 0, 1 | Cherenkov counters — event selection |
| 2 | Scintillator counter (optional selection with Cherenkov + MCP) |
| 3, 8–11 | nothing |
| 4–7 | Low gain upstream |
| 12–15 | High gain upstream |
| 16, 17 | MCP group 0 / group 1 (TR0 inputs, CAEN DT5742) |

In run 12 the upstream module and MCPs fire at ~sample 100; the beam
counters at ~460–540 (cable delays). Chs 16/17 in run 12 are the two
digitized copies of the SAME MCP TR0 (one per DRS group).

### Format change from run 14 on

Waveforms are stored in **mV** (×4.096 → ADC-eq), and the array layout
matches historical data: arrays 0–7 = DT5742 ch 0–7, array 8 = MCP TR0
copy (group 0), arrays 9–16 = DT5742 ch 8–15, array 17 = MCP TR0 copy
(group 1). DT5742-native channel roles and HG pairing are unchanged.

## Runs

| run | name | events | notes |
|---|---|---|---|
| 12 | RUN_LUAG_5GeV-2026-08-27-195743 | 25,000 | LuAG, 5 GeV, T10. Downstream card dead for this run. |

## Macros

- `macros/RunQC.C` — first-look run QC. Per channel: baseline mean/RMS,
  amplitude spectrum, ADC-saturation fraction (bias sanity), and pulse-peak
  sample distribution incl. record-edge fractions (does the trigger window
  capture the full signal?). Run from the workspace root:

  ```
  root -l -b -q 'macros/RunQC.C+(12)'
  ```

  Writes `Output/run_<N>/RunQC.root`, `RunQC.pdf`, `RunQC_summary.txt`.

- `macros/BeamCheck.C` — beam-selected check of the upstream channels.
  Selects events with a Cherenkov coincidence (ch 0 & ch 1) and compares
  average waveforms and amplitude spectra of the low/high-gain upstream
  channels in beam vs off-beam events, incl. high-gain saturation fractions.

  ```
  root -l -b -q 'macros/BeamCheck.C+(12)'
  ```

- `macros/Resolution.C` — first-pass resolution measurements: LG→HG transfer
  fits (clipped-pulse recovery), CFD timing of each capillary vs MCP1 and
  MCP0−MCP1 floor, amplitude-binned timing, electron ΣLG response with
  direct-hit edge fit, MIP calibration from the off-coincidence sample,
  split-half statistics checks. Results in `Output/run_<N>/Resolution_summary.txt`
  and `FINDINGS.md`.

  ```
  root -l -b -q 'macros/Resolution.C+(12)'
  ```
