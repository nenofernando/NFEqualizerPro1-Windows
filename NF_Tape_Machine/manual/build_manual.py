import base64
import pathlib

here = pathlib.Path(__file__).parent

def b64(name):
    return base64.b64encode((here / name).read_bytes()).decode("ascii")

COVER = b64("cover_clean.png")
IMG_TOPBAR = b64("sec_topbar.png")
IMG_REELS_VU = b64("sec_reels_vu.png")
IMG_INPUT = b64("sec_input.png")
IMG_TAPETYPE = b64("sec_tapetype.png")
IMG_DRIVEBIAS = b64("sec_drivebias.png")
IMG_WOWFLUTTER = b64("sec_wowflutter.png")
IMG_NOISE = b64("sec_noise.png")
IMG_DROPOUTS = b64("sec_dropouts.png")
IMG_EQLINK = b64("sec_eq_link.png")
IMG_OUTPUT = b64("sec_output.png")
IMG_SPEEDHEAD = b64("sec_speedhead.png")
IMG_AGEMIX = b64("sec_agemix.png")
IMG_METERBYPASS = b64("sec_meterbypass.png")

def fig(b64data, caption, width="260px"):
    return f'''<div class="fig"><img src="data:image/png;base64,{b64data}" style="max-width:{width}"><div class="fig-cap">{caption}</div></div>'''

template = """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>NF Tape Machine — User Manual</title>
<style>
  @page { size: A4; margin: 22mm 18mm 22mm 18mm; }
  * { box-sizing: border-box; }
  body {
    font-family: 'Helvetica Neue', Arial, sans-serif;
    color: #2a2420;
    font-size: 10.5pt;
    line-height: 1.5;
  }
  .cover {
    page-break-after: always;
    text-align: center;
    padding-top: 30mm;
  }
  .cover img {
    width: 100%;
    max-width: 620px;
    border-radius: 6px;
    box-shadow: 0 6px 24px rgba(0,0,0,0.25);
  }
  .cover h1 {
    font-size: 30pt;
    letter-spacing: 2px;
    color: #b9822f;
    margin: 26px 0 4px 0;
  }
  .cover .sub {
    font-size: 12pt;
    color: #6b5a3e;
    letter-spacing: 3px;
    text-transform: uppercase;
  }
  .cover .ver {
    margin-top: 40px;
    font-size: 10pt;
    color: #8a8a8a;
  }
  h1.section {
    font-size: 17pt;
    color: #a97324;
    border-bottom: 2px solid #d8a34a;
    padding-bottom: 6px;
    margin-top: 0;
    page-break-before: always;
  }
  h2 {
    font-size: 12.5pt;
    color: #7a3f10;
    margin-top: 22px;
    margin-bottom: 6px;
  }
  h3 {
    font-size: 11pt;
    color: #3a2f22;
    margin-top: 14px;
    margin-bottom: 4px;
  }
  p { margin: 4px 0 8px 0; }
  table {
    border-collapse: collapse;
    width: 100%;
    margin: 8px 0 16px 0;
    font-size: 9.5pt;
  }
  th, td {
    border: 1px solid #d9cdb8;
    padding: 5px 8px;
    text-align: left;
    vertical-align: top;
  }
  th {
    background: #f3e6cc;
    color: #5c3d16;
  }
  tr:nth-child(even) td { background: #faf6ec; }
  .toc { page-break-after: always; }
  .toc ol { font-size: 11pt; line-height: 2.0; }
  .toc a { color: #7a3f10; text-decoration: none; }
  .note {
    background: #f5efe0;
    border-left: 3px solid #d8a34a;
    padding: 8px 12px;
    margin: 10px 0;
    font-size: 9.7pt;
  }
  code {
    background: #f0ece2;
    padding: 1px 5px;
    border-radius: 3px;
    font-size: 9.5pt;
  }
  .fig {
    display: inline-block;
    text-align: center;
    background: #1b1a18;
    border-radius: 6px;
    padding: 8px;
    margin: 6px 10px 10px 0;
    vertical-align: top;
  }
  .fig img { display: block; border-radius: 3px; }
  .fig-cap {
    color: #cbb98a;
    font-size: 8pt;
    margin-top: 5px;
    letter-spacing: 0.5px;
    text-transform: uppercase;
  }
  .figrow { margin: 6px 0 4px 0; }
  .footer-tag {
    text-align: center;
    color: #a09a8c;
    font-size: 8.5pt;
    margin-top: 30px;
  }
</style>
</head>
<body>

<div class="cover">
  <img src="data:image/png;base64,__COVER__" alt="NF Tape Machine">
  <h1>NF TAPE MACHINE</h1>
  <div class="sub">Analog Tape Emulator — User Manual</div>
  <div class="ver">Version 0.1 &nbsp;·&nbsp; NF Audio Tools &nbsp;·&nbsp; VST3 / AU / Standalone</div>
</div>

<div class="toc">
  <h1 class="section" style="page-break-before: avoid;">Contents</h1>
  <ol>
    <li><a href="#overview">Overview</a></li>
    <li><a href="#install">Installation</a></li>
    <li><a href="#signal">Signal Flow</a></li>
    <li><a href="#controls">Controls Reference</a></li>
    <li><a href="#reels">Reels, Tube Lamps &amp; Metering</a></li>
    <li><a href="#presets">Factory Presets</a></li>
    <li><a href="#tips">Usage Tips</a></li>
    <li><a href="#credits">Credits &amp; Version</a></li>
  </ol>
</div>

<h1 class="section" id="overview" style="page-break-before: avoid;">1. Overview</h1>
<p>
NF Tape Machine is a clean-room analog tape emulator built around four
classic tape-formula profiles, three transport speeds and two reproduce-head
curves. It reproduces the character of a real reel-to-reel deck — tape
saturation, head-bump low end, high-frequency roll-off, wow &amp; flutter,
hiss and dropouts — all under direct control rather than baked into a
single "vibe" knob.
</p>
<p>
The plugin is designed to be usable at two extremes: completely transparent
(drive low, character sources switched out) for mix-bus glue, and
deliberately worn and characterful for creative sound design — with the
Default preset sitting deliberately close to the transparent end, so
inserting the plugin doesn't surprise you.
</p>
<div class="figrow">__FIG_TOPBAR__</div>

<h1 class="section" id="install">2. Installation</h1>
<h2>macOS</h2>
<ol>
  <li>Open <code>NF Tape Machine - Mac Installer.dmg</code>.</li>
  <li>Double-click <code>NF Tape Machine Installer.pkg</code> and follow the prompts.</li>
  <li>The installer places the plugin in:
    <ul>
      <li><code>/Library/Audio/Plug-Ins/VST3/NF Tape Machine.vst3</code></li>
      <li><code>/Library/Audio/Plug-Ins/Components/NF Tape Machine.component</code> (Audio Unit)</li>
    </ul>
  </li>
  <li>Rescan plugins in your DAW if it doesn't appear automatically.</li>
</ol>
<h2>Windows</h2>
<ol>
  <li>Run <code>NF Tape Machine Installer.exe</code>.</li>
  <li>The installer places the VST3 in <code>C:\\Program Files\\Common Files\\VST3\\NF Tape Machine.vst3</code>.</li>
  <li>Rescan plugins in your DAW if it doesn't appear automatically.</li>
</ol>
<div class="note">
An uninstaller is included on both platforms (macOS: re-run the package
and choose remove where offered; Windows: "Uninstall NF Tape Machine"
from Control Panel / Apps).
</div>

<h1 class="section" id="signal">3. Signal Flow</h1>
<p>
Input Trim → High-Pass Filter → Tape Saturation (Drive / Bias) → Head Bump
&amp; HF Roll-off (set by Tape Type, Speed &amp; Repro Head) → Wow &amp;
Flutter → Noise / Dropouts → Tone (EQ Low/High) → Low-Pass Filter → Output
Trim → Dry/Wet Mix.
</p>
<p>
Gain Link, when engaged, couples the Input and Output trims so raising one
attenuates the other by the same amount in dB — useful for driving the
saturation stage harder without changing the overall level hitting your
meters.
</p>

<h1 class="section" id="controls">4. Controls Reference</h1>

<h2>Input Section</h2>
<div class="figrow">__FIG_INPUT__</div>
<table>
<tr><th>Control</th><th>Range</th><th>Description</th></tr>
<tr><td>INPUT</td><td>-24 to +24 dB</td><td>Trim applied before the tape stage. Higher settings drive the saturation harder.</td></tr>
<tr><td>HPF</td><td>20 Hz – 200 Hz</td><td>High-pass filter ahead of the tape stage, useful for tightening low end before it hits saturation.</td></tr>
</table>

<h2>Tape Type</h2>
<div class="figrow">__FIG_TAPETYPE__</div>
<table>
<tr><th>Formula</th><th>Character</th></tr>
<tr><td>GP9</td><td>Modern, high-headroom formula — the most neutral, extended top end.</td></tr>
<tr><td>456</td><td>Classic studio formula — warm mids, gentle compression.</td></tr>
<tr><td>499</td><td>Punchy, slightly brighter formula with a firmer head bump.</td></tr>
<tr><td>250</td><td>Vintage/low-output formula — softer highs, more visible wow &amp; flutter and noise floor.</td></tr>
</table>

<h2>Drive / Saturation</h2>
<div class="figrow">__FIG_DRIVEBIAS__</div>
<table>
<tr><th>Control</th><th>Range</th><th>Description</th></tr>
<tr><td>DRIVE</td><td>0 – 10</td><td>Amount of tape saturation. Also drives the tube lamps' glow in the interface.</td></tr>
<tr><td>SAT (IN)</td><td>On/Off</td><td>Enables/disables the saturation stage entirely.</td></tr>
<tr><td>BIAS</td><td>0 – 10</td><td>Simulated tape bias — shifts saturation symmetry and harmonic balance.</td></tr>
<tr><td>CAL</td><td>On/Off</td><td>Bias Calibrated — engages the factory-calibrated bias curve for the selected Tape Type.</td></tr>
</table>

<h2>Wow &amp; Flutter</h2>
<div class="figrow">__FIG_WOWFLUTTER__</div>
<table>
<tr><th>Control</th><th>Range</th><th>Description</th></tr>
<tr><td>RATE</td><td>0.1 – 10 Hz</td><td>Modulation speed of the pitch instability.</td></tr>
<tr><td>DEPTH</td><td>0 – 100</td><td>Modulation depth. Kept low by default for a subtle "alive" quality rather than audible detuning.</td></tr>
<tr><td>IN</td><td>On/Off</td><td>Enables/disables wow &amp; flutter. On by default, subtle.</td></tr>
</table>

<h2>Noise &amp; Dropouts</h2>
<div class="figrow">__FIG_NOISE__ __FIG_DROPOUTS__</div>
<table>
<tr><th>Control</th><th>Range</th><th>Description</th></tr>
<tr><td>NOISE</td><td>0 – 10</td><td>Tape hiss level.</td></tr>
<tr><td>IN (Noise)</td><td>On/Off</td><td>Enables/disables hiss. Off by default.</td></tr>
<tr><td>DROPOUTS</td><td>0 – 10</td><td>Frequency/severity of momentary level dropouts.</td></tr>
<tr><td>IN (Dropouts)</td><td>On/Off</td><td>Enables/disables dropouts. Off by default.</td></tr>
</table>

<h2>EQ &amp; Gain Link</h2>
<div class="figrow">__FIG_EQLINK__</div>
<table>
<tr><th>Control</th><th>Range</th><th>Description</th></tr>
<tr><td>LF</td><td>-12 to +12 dB</td><td>Low-shelf tone control.</td></tr>
<tr><td>HF</td><td>-12 to +12 dB</td><td>High-shelf tone control.</td></tr>
</table>

<h2>Output Section</h2>
<div class="figrow">__FIG_OUTPUT__</div>
<table>
<tr><th>Control</th><th>Range</th><th>Description</th></tr>
<tr><td>OUTPUT</td><td>-24 to +24 dB</td><td>Trim applied after the tape stage.</td></tr>
<tr><td>LPF</td><td>2 kHz – 20 kHz</td><td>Low-pass filter after the tape stage, for extra high-frequency taming.</td></tr>
<tr><td>LINK</td><td>On/Off</td><td>Gain Link — see below.</td></tr>
</table>

<div class="note">
<b>Gain Link</b> couples INPUT and OUTPUT so the two move inversely, dB for
dB, keeping overall level roughly constant while you push the saturation
harder or gentler. While linked, a single click (no drag) on either the
INPUT or OUTPUT knob resets both to 0 dB. Turning Link off frees both knobs
to move independently again.
</div>

<h2>Tape Speed &amp; Repro Head</h2>
<div class="figrow">__FIG_SPEEDHEAD__</div>
<table>
<tr><th>Control</th><th>Options</th><th>Description</th></tr>
<tr><td>TAPE SPEED</td><td>7.5 / 15 / 30 IPS</td><td>Higher speeds extend high-frequency response and reduce wow &amp; flutter/noise; lower speeds are darker and more colored.</td></tr>
<tr><td>REPRO HEAD</td><td>NAB / IEC</td><td>Playback equalization standard — changes the tonal balance of the head-bump/roll-off curve.</td></tr>
</table>

<h2>Tape Age &amp; Mix</h2>
<div class="figrow">__FIG_AGEMIX__</div>
<table>
<tr><th>Control</th><th>Range</th><th>Description</th></tr>
<tr><td>TAPE AGE</td><td>0 (New) – 100 (Worn)</td><td>Global "wear" control — gradually softens highs and increases noticeable imperfections as it's raised.</td></tr>
<tr><td>MIX</td><td>0 (Dry) – 100 (Wet)</td><td>Blends the processed signal with the untouched input. 100% by default for traditional insert use.</td></tr>
</table>

<h2>Global</h2>
<div class="figrow">__FIG_METERBYPASS__</div>
<table>
<tr><th>Control</th><th>Description</th></tr>
<tr><td>BYPASS</td><td>Bypasses the entire tape engine, passing audio through unprocessed.</td></tr>
</table>

<h1 class="section" id="reels">5. Reels, Tube Lamps &amp; Metering</h1>
<div class="figrow">__FIG_REELSVU__</div>
<h3>Reel-to-reel transport</h3>
<p>
The two reels are cosmetic but interactive — clicking either one starts or
stops the spinning transport for both (they're threaded on the same tape).
Each Tape Type recolors the reel finish (gold for GP9, silver for 456, red
for 499, black for 250) so you always know which formula is loaded at a
glance.
</p>
<h3>Tube lamps</h3>
<p>
The two lamps flanking the reels brighten and intensify in color as DRIVE
is raised, and hold a fixed, vivid resting glow when SAT is switched off or
the plugin is bypassed — a visual read on how hard the tape stage is
working.
</p>
<h3>VU meters &amp; output meter</h3>
<p>
The twin VU dials show the processed signal's level on a standard
-20…+3 dB analog scale. The LED-style bar meter below the control rows
shows the same left/right output levels in a more modern peak-reading
format.
</p>

<h1 class="section" id="presets">6. Factory Presets</h1>
<table>
<tr><th>Preset</th><th>Character</th></tr>
<tr><td>Default</td><td>Subtle, transparent-leaning starting point — saturation and wow &amp; flutter present but restrained, noise and dropouts off.</td></tr>
<tr><td>Warm Bus Glue</td><td>Gentle 456-formula warmth and compression for mix-bus duty.</td></tr>
<tr><td>Vintage Slap</td><td>499 formula pushed harder, with noise and dropouts engaged for an overtly vintage character.</td></tr>
<tr><td>Mastering Sheen</td><td>Very light touch on 499 formula with IEC repro head — for mastering-chain use.</td></tr>
<tr><td>Lo-Fi Worn</td><td>250 formula driven hard, high tape age, dropouts and noise all engaged — heavily degraded, lo-fi character.</td></tr>
</table>
<p>Use the <b>&lt;</b> / <b>&gt;</b> arrows either side of the preset name to step through the factory bank.</p>

<h1 class="section" id="tips">7. Usage Tips</h1>
<ul>
<li>For mix-bus glue, keep DRIVE low (2–4), MIX around 100%, and leave NOISE/DROPOUTS off.</li>
<li>Engage GAIN LINK before dialing in DRIVE so you can judge the saturation's tone without a level jump confusing the comparison — click either knob to snap back to 0/0 and start again.</li>
<li>Raising TAPE AGE is a fast way to dial in "worn" character without touching every individual parameter by hand.</li>
<li>Switch TAPE SPEED to 7.5 IPS for a noticeably darker, more colored result; 30 IPS for the cleanest, most extended top end.</li>
<li>WOW &amp; FLUTTER's DEPTH is intentionally subtle by default — raise it further only when you want an obviously wobbly, lo-fi pitch effect.</li>
</ul>

<h1 class="section" id="credits">8. Credits &amp; Version</h1>
<p>
NF Tape Machine v0.1 — NF Audio Tools.<br>
Built with JUCE. VST3, AU and Standalone formats.
</p>
<div class="footer-tag">NF Audio Tools &nbsp;·&nbsp; NF Tape Machine &nbsp;·&nbsp; v0.1</div>

</body>
</html>
"""

html = template
html = html.replace("__COVER__", COVER)
html = html.replace("__FIG_TOPBAR__", fig(IMG_TOPBAR, "Preset bar &amp; power", "220px"))
html = html.replace("__FIG_REELSVU__", fig(IMG_REELS_VU, "Reels, VU meters &amp; nameplate", "100%"))
html = html.replace("__FIG_INPUT__", fig(IMG_INPUT, "Input + HPF", "150px"))
html = html.replace("__FIG_TAPETYPE__", fig(IMG_TAPETYPE, "Tape Type", "140px"))
html = html.replace("__FIG_DRIVEBIAS__", fig(IMG_DRIVEBIAS, "Drive/Sat + Bias/Cal", "220px"))
html = html.replace("__FIG_WOWFLUTTER__", fig(IMG_WOWFLUTTER, "Wow & Flutter", "230px"))
html = html.replace("__FIG_NOISE__", fig(IMG_NOISE, "Noise", "150px"))
html = html.replace("__FIG_DROPOUTS__", fig(IMG_DROPOUTS, "Dropouts", "130px"))
html = html.replace("__FIG_EQLINK__", fig(IMG_EQLINK, "EQ + Gain Link", "230px"))
html = html.replace("__FIG_OUTPUT__", fig(IMG_OUTPUT, "Output + LPF", "150px"))
html = html.replace("__FIG_SPEEDHEAD__", fig(IMG_SPEEDHEAD, "Tape Speed & Repro Head", "420px"))
html = html.replace("__FIG_AGEMIX__", fig(IMG_AGEMIX, "Tape Age, Dropouts & Mix", "380px"))
html = html.replace("__FIG_METERBYPASS__", fig(IMG_METERBYPASS, "Output Meter & Bypass", "380px"))

(here / "NF_Tape_Machine_Manual.html").write_text(html)
print("wrote", (here / "NF_Tape_Machine_Manual.html").stat().st_size, "bytes")
