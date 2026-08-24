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

VERSION = "0.1.0"

def fig(b64data, caption, width="260px"):
    return f'''<div class="fig"><img src="data:image/png;base64,{b64data}" style="max-width:{width}"><div class="fig-cap">{caption}</div></div>'''

CSS = """
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
    position: relative;
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
  .cover .rights {
    margin-top: 6px;
    font-size: 8.5pt;
    color: #a89a80;
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
"""


def build(lang: str) -> str:
    t = TEXT[lang]

    fig_topbar = fig(IMG_TOPBAR, t["cap_topbar"], "220px")
    fig_reelsvu = fig(IMG_REELS_VU, t["cap_reelsvu"], "100%")
    fig_input = fig(IMG_INPUT, t["cap_input"], "150px")
    fig_tapetype = fig(IMG_TAPETYPE, t["cap_tapetype"], "140px")
    fig_drivebias = fig(IMG_DRIVEBIAS, t["cap_drivebias"], "220px")
    fig_wowflutter = fig(IMG_WOWFLUTTER, t["cap_wowflutter"], "230px")
    fig_noise = fig(IMG_NOISE, t["cap_noise"], "150px")
    fig_dropouts = fig(IMG_DROPOUTS, t["cap_dropouts"], "130px")
    fig_eqlink = fig(IMG_EQLINK, t["cap_eqlink"], "230px")
    fig_output = fig(IMG_OUTPUT, t["cap_output"], "150px")
    fig_speedhead = fig(IMG_SPEEDHEAD, t["cap_speedhead"], "420px")
    fig_agemix = fig(IMG_AGEMIX, t["cap_agemix"], "380px")
    fig_meterbypass = fig(IMG_METERBYPASS, t["cap_meterbypass"], "380px")

    return f"""<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>NF Tape Machine &mdash; {t['title_suffix']}</title>
<style>{CSS}</style>
</head>
<body>

<div class="cover">
  <img src="data:image/png;base64,{COVER}" alt="NF Tape Machine">
  <h1>NF TAPE MACHINE</h1>
  <div class="sub">{t['tagline']}</div>
  <div class="ver">{t['version_line'].format(v=VERSION)}</div>
  <div class="rights">NENNO FERNANDO AUDIO TOOLS&reg; &mdash; {t['allrights']}</div>
</div>

<div class="toc">
  <h1 class="section" style="page-break-before: avoid;">{t['toc_h']}</h1>
  <ol>
    <li><a href="#overview">{t['toc_overview']}</a></li>
    <li><a href="#install">{t['toc_install']}</a></li>
    <li><a href="#signal">{t['toc_signal']}</a></li>
    <li><a href="#controls">{t['toc_controls']}</a></li>
    <li><a href="#reels">{t['toc_reels']}</a></li>
    <li><a href="#window">{t['toc_window']}</a></li>
    <li><a href="#presets">{t['toc_presets']}</a></li>
    <li><a href="#tips">{t['toc_tips']}</a></li>
    <li><a href="#credits">{t['toc_credits']}</a></li>
  </ol>
</div>

<h1 class="section" id="overview" style="page-break-before: avoid;">1. {t['overview_h']}</h1>
<p>{t['overview_p1']}</p>
<p>{t['overview_p2']}</p>
<div class="figrow">{fig_topbar}</div>

<h1 class="section" id="install">2. {t['install_h']}</h1>
<h2>macOS</h2>
<ol>
  <li>{t['mac_step1']}</li>
  <li>{t['mac_step2']}</li>
  <li>{t['mac_step3']}
    <ul>
      <li><code>/Library/Audio/Plug-Ins/VST3/NF Tape Machine.vst3</code></li>
      <li><code>/Library/Audio/Plug-Ins/Components/NF Tape Machine.component</code> ({t['audio_unit']})</li>
    </ul>
  </li>
  <li>{t['mac_step4']}</li>
</ol>
<h2>Windows</h2>
<ol>
  <li>{t['win_step1']}</li>
  <li>{t['win_step2']}</li>
  <li>{t['win_step3']}</li>
</ol>
<div class="note">{t['install_note']}</div>

<h1 class="section" id="signal">3. {t['signal_h']}</h1>
<p>{t['signal_p1']}</p>
<p>{t['signal_p2']}</p>

<h1 class="section" id="controls">4. {t['controls_h']}</h1>

<h2>{t['sec_input_h']}</h2>
<div class="figrow">{fig_input}</div>
<table>
<tr><th>{t['th_control']}</th><th>{t['th_range']}</th><th>{t['th_description']}</th></tr>
<tr><td>INPUT</td><td>-24 {t['to']} +24 dB</td><td>{t['c_input']}</td></tr>
<tr><td>HPF</td><td>20 Hz &ndash; 200 Hz</td><td>{t['c_hpf']}</td></tr>
</table>

<h2>{t['sec_tapetype_h']}</h2>
<div class="figrow">{fig_tapetype}</div>
<table>
<tr><th>{t['th_formula']}</th><th>{t['th_character']}</th></tr>
<tr><td>GP9</td><td>{t['tt_gp9']}</td></tr>
<tr><td>456</td><td>{t['tt_456']}</td></tr>
<tr><td>499</td><td>{t['tt_499']}</td></tr>
<tr><td>250</td><td>{t['tt_250']}</td></tr>
</table>

<h2>{t['sec_drive_h']}</h2>
<div class="figrow">{fig_drivebias}</div>
<table>
<tr><th>{t['th_control']}</th><th>{t['th_range']}</th><th>{t['th_description']}</th></tr>
<tr><td>DRIVE</td><td>0 &ndash; 10</td><td>{t['c_drive']}</td></tr>
<tr><td>SAT (IN)</td><td>{t['onoff']}</td><td>{t['c_sat']}</td></tr>
<tr><td>BIAS</td><td>0 &ndash; 10</td><td>{t['c_bias']}</td></tr>
<tr><td>CAL</td><td>{t['onoff']}</td><td>{t['c_cal']}</td></tr>
</table>

<h2>{t['sec_wow_h']}</h2>
<div class="figrow">{fig_wowflutter}</div>
<table>
<tr><th>{t['th_control']}</th><th>{t['th_range']}</th><th>{t['th_description']}</th></tr>
<tr><td>RATE</td><td>0.1 &ndash; 10 Hz</td><td>{t['c_rate']}</td></tr>
<tr><td>DEPTH</td><td>0 &ndash; 100</td><td>{t['c_depth']}</td></tr>
<tr><td>IN</td><td>{t['onoff']}</td><td>{t['c_wowin']}</td></tr>
</table>

<h2>{t['sec_noise_h']}</h2>
<div class="figrow">{fig_noise} {fig_dropouts}</div>
<table>
<tr><th>{t['th_control']}</th><th>{t['th_range']}</th><th>{t['th_description']}</th></tr>
<tr><td>NOISE</td><td>0 &ndash; 10</td><td>{t['c_noise']}</td></tr>
<tr><td>IN ({t['noise_word']})</td><td>{t['onoff']}</td><td>{t['c_noisein']}</td></tr>
<tr><td>DROPOUTS</td><td>0 &ndash; 10</td><td>{t['c_dropouts']}</td></tr>
<tr><td>IN ({t['dropouts_word']})</td><td>{t['onoff']}</td><td>{t['c_dropoutsin']}</td></tr>
</table>

<h2>{t['sec_eq_h']}</h2>
<div class="figrow">{fig_eqlink}</div>
<table>
<tr><th>{t['th_control']}</th><th>{t['th_range']}</th><th>{t['th_description']}</th></tr>
<tr><td>LF</td><td>-12 {t['to']} +12 dB</td><td>{t['c_lf']}</td></tr>
<tr><td>HF</td><td>-12 {t['to']} +12 dB</td><td>{t['c_hf']}</td></tr>
</table>

<h2>{t['sec_output_h']}</h2>
<div class="figrow">{fig_output}</div>
<table>
<tr><th>{t['th_control']}</th><th>{t['th_range']}</th><th>{t['th_description']}</th></tr>
<tr><td>OUTPUT</td><td>-24 {t['to']} +24 dB</td><td>{t['c_output']}</td></tr>
<tr><td>LPF</td><td>2 kHz &ndash; 20 kHz</td><td>{t['c_lpf']}</td></tr>
<tr><td>LINK</td><td>{t['onoff']}</td><td>{t['c_link']}</td></tr>
</table>

<div class="note">{t['gainlink_note']}</div>

<h2>{t['sec_speedhead_h']}</h2>
<div class="figrow">{fig_speedhead}</div>
<table>
<tr><th>{t['th_control']}</th><th>{t['th_options']}</th><th>{t['th_description']}</th></tr>
<tr><td>TAPE SPEED</td><td>7.5 / 15 / 30 IPS</td><td>{t['c_speed']}</td></tr>
<tr><td>REPRO HEAD</td><td>NAB / IEC</td><td>{t['c_reprohead']}</td></tr>
</table>

<h2>{t['sec_agemix_h']}</h2>
<div class="figrow">{fig_agemix}</div>
<table>
<tr><th>{t['th_control']}</th><th>{t['th_range']}</th><th>{t['th_description']}</th></tr>
<tr><td>TAPE AGE</td><td>{t['age_range']}</td><td>{t['c_age']}</td></tr>
<tr><td>MIX</td><td>{t['mix_range']}</td><td>{t['c_mix']}</td></tr>
</table>

<h2>{t['sec_global_h']}</h2>
<div class="figrow">{fig_meterbypass}</div>
<table>
<tr><th>{t['th_control']}</th><th>{t['th_description']}</th></tr>
<tr><td>BYPASS</td><td>{t['c_bypass']}</td></tr>
</table>

<h1 class="section" id="reels">5. {t['reels_h']}</h1>
<div class="figrow">{fig_reelsvu}</div>
<h3>{t['reels_h3']}</h3>
<p>{t['reels_p']}</p>
<h3>{t['tubes_h3']}</h3>
<p>{t['tubes_p']}</p>
<h3>{t['vu_h3']}</h3>
<p>{t['vu_p']}</p>

<h1 class="section" id="window">6. {t['window_h']}</h1>
<p>{t['window_p1']}</p>
<div class="note">{t['window_note']}</div>

<h1 class="section" id="presets">7. {t['presets_h']}</h1>
<table>
<tr><th>{t['th_preset']}</th><th>{t['th_character']}</th></tr>
<tr><td>Default</td><td>{t['p_default']}</td></tr>
<tr><td>Warm Bus Glue</td><td>{t['p_warmbus']}</td></tr>
<tr><td>Vintage Slap</td><td>{t['p_vintage']}</td></tr>
<tr><td>Mastering Sheen</td><td>{t['p_mastering']}</td></tr>
<tr><td>Lo-Fi Worn</td><td>{t['p_lofi']}</td></tr>
</table>
<p>{t['presets_p']}</p>

<h1 class="section" id="tips">8. {t['tips_h']}</h1>
<ul>
<li>{t['tip1']}</li>
<li>{t['tip2']}</li>
<li>{t['tip3']}</li>
<li>{t['tip4']}</li>
<li>{t['tip5']}</li>
<li>{t['tip6']}</li>
</ul>

<h1 class="section" id="credits">9. {t['credits_h']}</h1>
<p>{t['credits_p']}</p>
<table>
<tr><th>{t['th_format']}</th><th>{t['th_version']}</th></tr>
<tr><td>VST3</td><td>{VERSION}</td></tr>
<tr><td>{t['audio_unit']} (AU)</td><td>{VERSION}</td></tr>
<tr><td>Standalone</td><td>{VERSION}</td></tr>
</table>
<p style="margin-top:18px;color:#8a8a8a;font-size:9pt">NENNO FERNANDO AUDIO TOOLS&reg; &mdash; {t['allrights']}</p>
<div class="footer-tag">NF Audio Tools &nbsp;&middot;&nbsp; NF Tape Machine &nbsp;&middot;&nbsp; v{VERSION}</div>

</body>
</html>
"""


TEXT = {
"en": dict(
    title_suffix="User Manual",
    tagline="Analog Tape Emulator &mdash; User Manual",
    version_line="Version {v} &nbsp;&middot;&nbsp; NF Audio Tools &nbsp;&middot;&nbsp; VST3 / AU / Standalone",
    allrights="All rights reserved.",
    toc_h="Contents",
    toc_overview="Overview",
    toc_install="Installation",
    toc_signal="Signal Flow",
    toc_controls="Controls Reference",
    toc_reels="Reels, Tube Lamps &amp; Metering",
    toc_window="Window &amp; Logo",
    toc_presets="Factory Presets",
    toc_tips="Usage Tips",
    toc_credits="Credits &amp; Version",
    overview_h="Overview",
    overview_p1="NF Tape Machine is a clean-room analog tape emulator built around four classic tape-formula profiles, three transport speeds and two reproduce-head curves. It reproduces the character of a real reel-to-reel deck &mdash; tape saturation, head-bump low end, high-frequency roll-off, wow &amp; flutter, hiss and dropouts &mdash; all under direct control rather than baked into a single \"vibe\" knob.",
    overview_p2="The plugin is designed to be usable at two extremes: completely transparent (drive low, character sources switched out) for mix-bus glue, and deliberately worn and characterful for creative sound design &mdash; with the Default preset sitting deliberately close to the transparent end, so inserting the plugin doesn't surprise you.",
    cap_topbar="Preset bar &amp; power",
    install_h="Installation",
    mac_step1="Open <code>NF Tape Machine - Mac Installer.dmg</code>.",
    mac_step2="Double-click <code>NF Tape Machine Installer.pkg</code> and follow the prompts.",
    mac_step3="The installer places the plugin in:",
    mac_step4="Rescan plugins in your DAW if it doesn't appear automatically.",
    win_step1="Run <code>NF Tape Machine Installer.exe</code>.",
    win_step2="The installer places the VST3 in <code>C:\\Program Files\\Common Files\\VST3\\NF Tape Machine.vst3</code>, and the PDF manuals (English + Portuguese) in <code>C:\\Program Files\\NF Audio Tools\\NF Tape Machine\\Manual\\</code>.",
    win_step3="Rescan plugins in your DAW if it doesn't appear automatically.",
    install_note="An uninstaller is included on both platforms (macOS: re-run the package and choose remove where offered; Windows: \"Uninstall NF Tape Machine\" from Control Panel / Apps). The bilingual PDF manual is installed by both installers &mdash; on macOS into <code>/Users/Shared/NF Audio Tools/NF Tape Machine/Manual</code>.",
    audio_unit="Audio Unit",
    signal_h="Signal Flow",
    signal_p1="Input Trim &rarr; High-Pass Filter &rarr; Tape Saturation (Drive / Bias) &rarr; Head Bump &amp; HF Roll-off (set by Tape Type, Speed &amp; Repro Head) &rarr; Wow &amp; Flutter &rarr; Noise / Dropouts &rarr; Tone (EQ Low/High) &rarr; Low-Pass Filter &rarr; Output Trim &rarr; Dry/Wet Mix.",
    signal_p2="Gain Link, when engaged, couples the Input and Output trims so raising one attenuates the other by the same amount in dB &mdash; useful for driving the saturation stage harder without changing the overall level hitting your meters.",
    controls_h="Controls Reference",
    th_control="Control", th_range="Range", th_description="Description", th_options="Options",
    th_formula="Formula", th_character="Character", th_preset="Preset", th_format="Format", th_version="Version",
    to="to", onoff="On/Off",
    sec_input_h="Input Section",
    cap_input="Input + HPF",
    c_input="Trim applied before the tape stage. Higher settings drive the saturation harder.",
    c_hpf="High-pass filter ahead of the tape stage, useful for tightening low end before it hits saturation.",
    sec_tapetype_h="Tape Type",
    cap_tapetype="Tape Type",
    tt_gp9="Modern, high-headroom formula &mdash; the most neutral, extended top end.",
    tt_456="Classic studio formula &mdash; warm mids, gentle compression.",
    tt_499="Punchy, slightly brighter formula with a firmer head bump.",
    tt_250="Vintage/low-output formula &mdash; softer highs, more visible wow &amp; flutter and noise floor.",
    sec_drive_h="Drive / Saturation",
    cap_drivebias="Drive/Sat + Bias/Cal",
    c_drive="Amount of tape saturation. Also drives the tube lamps' glow in the interface.",
    c_sat="Enables/disables the saturation stage entirely.",
    c_bias="Simulated tape bias &mdash; shifts saturation symmetry and harmonic balance.",
    c_cal="Bias Calibrated &mdash; engages the factory-calibrated bias curve for the selected Tape Type.",
    sec_wow_h="Wow &amp; Flutter",
    cap_wowflutter="Wow &amp; Flutter",
    c_rate="Modulation speed of the pitch instability.",
    c_depth="Modulation depth. Kept low by default for a subtle \"alive\" quality rather than audible detuning.",
    c_wowin="Enables/disables wow &amp; flutter. On by default, subtle.",
    sec_noise_h="Noise &amp; Dropouts",
    cap_noise="Noise", cap_dropouts="Dropouts",
    noise_word="Noise", dropouts_word="Dropouts",
    c_noise="Tape hiss level.",
    c_noisein="Enables/disables hiss. Off by default.",
    c_dropouts="Frequency/severity of momentary level dropouts.",
    c_dropoutsin="Enables/disables dropouts. Off by default.",
    sec_eq_h="EQ &amp; Gain Link",
    cap_eqlink="EQ + Gain Link",
    c_lf="Low-shelf tone control.",
    c_hf="High-shelf tone control.",
    sec_output_h="Output Section",
    cap_output="Output + LPF",
    c_output="Trim applied after the tape stage.",
    c_lpf="Low-pass filter after the tape stage, for extra high-frequency taming.",
    c_link="Gain Link &mdash; see below.",
    gainlink_note="<b>Gain Link</b> couples INPUT and OUTPUT so the two move inversely, dB for dB, keeping overall level roughly constant while you push the saturation harder or gentler. While linked, dragging either knob shows its usual value bubble plus a matching one over the other knob, so both linked values stay visible at once. A single click (no drag) on either the INPUT or OUTPUT knob resets both to 0 dB. Turning Link off frees both knobs to move independently again.",
    sec_speedhead_h="Tape Speed &amp; Repro Head",
    cap_speedhead="Tape Speed &amp; Repro Head",
    c_speed="Higher speeds extend high-frequency response and reduce wow &amp; flutter/noise; lower speeds are darker and more colored.",
    c_reprohead="Playback equalization standard &mdash; changes the tonal balance of the head-bump/roll-off curve.",
    sec_agemix_h="Tape Age &amp; Mix",
    cap_agemix="Tape Age, Dropouts &amp; Mix",
    age_range="0 (New) &ndash; 100 (Worn)",
    mix_range="0 (Dry) &ndash; 100 (Wet)",
    c_age="Global \"wear\" control &mdash; gradually softens highs and increases noticeable imperfections as it's raised.",
    c_mix="Blends the processed signal with the untouched input. 100% by default for traditional insert use.",
    sec_global_h="Global",
    cap_meterbypass="Output Meter &amp; Bypass",
    c_bypass="Bypasses the entire tape engine, passing audio through unprocessed.",
    reels_h="Reels, Tube Lamps &amp; Metering",
    cap_reelsvu="Reels, VU meters &amp; nameplate",
    reels_h3="Reel-to-reel transport",
    reels_p="The two reels are cosmetic but interactive &mdash; clicking either one starts or stops the spinning transport for both (they're threaded on the same tape). Each Tape Type recolors the reel finish (gold for GP9, silver for 456, red for 499, black for 250) so you always know which formula is loaded at a glance. They also spin faster or slower to match the selected TAPE SPEED &mdash; quickest at 30 IPS, slowest at 7.5 IPS.",
    tubes_h3="Tube lamps",
    tubes_p="The two lamps flanking the reels brighten and intensify in color as DRIVE is raised, and hold a fixed, vivid resting glow when SAT is switched off or the plugin is bypassed &mdash; a visual read on how hard the tape stage is working.",
    vu_h3="VU meters &amp; output meter",
    vu_p="The twin VU dials show the processed signal's level on a standard -20&hellip;+3 dB analog scale. The LED-style bar meter below the control rows shows the same left/right output levels in a more modern peak-reading format.",
    window_h="Window &amp; Logo",
    window_p1="The plugin window can be freely resized from its corner, from roughly a third of its native size up to double it, always keeping its original proportions.",
    window_note="<b>Double-click the \"NF TAPE MACHINE\" logo</b> at the top of the panel at any time to instantly snap the window back to its default size &mdash; handy after dragging the corner larger for detail work. Double-clicking while the window is already at its default size does nothing.",
    presets_h="Factory Presets",
    p_default="Subtle, transparent-leaning starting point &mdash; saturation and wow &amp; flutter present but restrained, noise and dropouts off.",
    p_warmbus="Gentle 456-formula warmth and compression for mix-bus duty.",
    p_vintage="499 formula pushed harder, with noise and dropouts engaged for an overtly vintage character.",
    p_mastering="Very light touch on 499 formula with IEC repro head &mdash; for mastering-chain use.",
    p_lofi="250 formula driven hard, high tape age, dropouts and noise all engaged &mdash; heavily degraded, lo-fi character.",
    presets_p="Use the <b>&lt;</b> / <b>&gt;</b> arrows either side of the preset name to step through the factory bank.",
    tips_h="Usage Tips",
    tip1="For mix-bus glue, keep DRIVE low (2&ndash;4), MIX around 100%, and leave NOISE/DROPOUTS off.",
    tip2="Engage GAIN LINK before dialing in DRIVE so you can judge the saturation's tone without a level jump confusing the comparison &mdash; click either knob to snap back to 0/0 and start again.",
    tip3="Raising TAPE AGE is a fast way to dial in \"worn\" character without touching every individual parameter by hand.",
    tip4="Switch TAPE SPEED to 7.5 IPS for a noticeably darker, more colored result; 30 IPS for the cleanest, most extended top end.",
    tip5="WOW &amp; FLUTTER's DEPTH is intentionally subtle by default &mdash; raise it further only when you want an obviously wobbly, lo-fi pitch effect.",
    tip6="Double-click the NF logo at the top of the panel to reset the window to its default size after resizing it larger for detail work.",
    credits_h="Credits &amp; Version",
    credits_p="NF Tape Machine is developed by NF Audio Tools. This manual and the plugin describe version " + VERSION + ".",
),
"pt": dict(
    title_suffix="Manual do Usu\u00e1rio",
    tagline="Emulador de Fita Anal\u00f3gica &mdash; Manual do Usu\u00e1rio",
    version_line="Vers\u00e3o {v} &nbsp;&middot;&nbsp; NF Audio Tools &nbsp;&middot;&nbsp; VST3 / AU / Standalone",
    allrights="Todos os direitos reservados.",
    toc_h="\u00cdndice",
    toc_overview="Vis\u00e3o Geral",
    toc_install="Instala\u00e7\u00e3o",
    toc_signal="Fluxo de Sinal",
    toc_controls="Refer\u00eancia de Controles",
    toc_reels="Rolos, L\u00e2mpadas Valvuladas e Medi\u00e7\u00e3o",
    toc_window="Janela e Logo",
    toc_presets="Presets de F\u00e1brica",
    toc_tips="Dicas de Uso",
    toc_credits="Cr\u00e9ditos e Vers\u00e3o",
    overview_h="Vis\u00e3o Geral",
    overview_p1="O NF Tape Machine \u00e9 um emulador de fita anal\u00f3gica feito do zero, constru\u00eddo em torno de quatro perfis cl\u00e1ssicos de f\u00f3rmula de fita, tr\u00eas velocidades de transporte e duas curvas de cabe\u00e7a de reprodu\u00e7\u00e3o. Ele reproduz o car\u00e1ter de um deck real de rolo a rolo &mdash; satura\u00e7\u00e3o de fita, grave com \u201chead bump\u201d, roll-off de agudos, wow &amp; flutter, chiado (hiss) e dropouts &mdash; tudo sob controle direto, em vez de embutido num \u00fanico knob de \u201cvibe\u201d.",
    overview_p2="O plugin foi projetado pra ser usado em dois extremos: completamente transparente (drive baixo, fontes de car\u00e1ter desligadas) pra colagem de mix-bus, e deliberadamente desgastado e caracter\u00edstico pra design de som criativo &mdash; com o preset Default ficando propositalmente perto do lado transparente, pra inserir o plugin n\u00e3o surpreender.",
    cap_topbar="Barra de presets e power",
    install_h="Instala\u00e7\u00e3o",
    mac_step1="Abra o <code>NF Tape Machine - Mac Installer.dmg</code>.",
    mac_step2="D\u00ea duplo clique no <code>NF Tape Machine Installer.pkg</code> e siga as instru\u00e7\u00f5es.",
    mac_step3="O instalador coloca o plugin em:",
    mac_step4="Refa\u00e7a a busca de plugins na sua DAW caso ele n\u00e3o apare\u00e7a automaticamente.",
    win_step1="Execute o <code>NF Tape Machine Installer.exe</code>.",
    win_step2="O instalador coloca o VST3 em <code>C:\\Program Files\\Common Files\\VST3\\NF Tape Machine.vst3</code>, e os manuais em PDF (Ingl\u00eas + Portugu\u00eas) em <code>C:\\Program Files\\NF Audio Tools\\NF Tape Machine\\Manual\\</code>.",
    win_step3="Refa\u00e7a a busca de plugins na sua DAW caso ele n\u00e3o apare\u00e7a automaticamente.",
    install_note="Um desinstalador \u00e9 inclu\u00eddo nas duas plataformas (macOS: execute o pacote novamente e escolha remover quando oferecido; Windows: \u201cUninstall NF Tape Machine\u201d no Painel de Controle / Apps). O manual bilingue em PDF \u00e9 instalado pelos dois instaladores &mdash; no macOS em <code>/Users/Shared/NF Audio Tools/NF Tape Machine/Manual</code>.",
    audio_unit="Audio Unit",
    signal_h="Fluxo de Sinal",
    signal_p1="Trim de Entrada &rarr; Filtro Passa-Altas &rarr; Satura\u00e7\u00e3o de Fita (Drive / Bias) &rarr; Head Bump e Roll-off de Agudos (definidos por Tape Type, Speed e Repro Head) &rarr; Wow &amp; Flutter &rarr; Noise / Dropouts &rarr; Tom (EQ Low/High) &rarr; Filtro Passa-Baixas &rarr; Trim de Sa\u00edda &rarr; Mix Dry/Wet.",
    signal_p2="O Gain Link, quando ativado, acopla os trims de Input e Output, de forma que aumentar um atenua o outro na mesma quantidade em dB &mdash; \u00fatil pra empurrar o est\u00e1gio de satura\u00e7\u00e3o mais forte sem mudar o n\u00edvel geral que chega nos seus medidores.",
    controls_h="Refer\u00eancia de Controles",
    th_control="Controle", th_range="Faixa", th_description="Descri\u00e7\u00e3o", th_options="Op\u00e7\u00f5es",
    th_formula="F\u00f3rmula", th_character="Car\u00e1ter", th_preset="Preset", th_format="Formato", th_version="Vers\u00e3o",
    to="a", onoff="On/Off",
    sec_input_h="Se\u00e7\u00e3o de Entrada",
    cap_input="Input + HPF",
    c_input="Trim aplicado antes do est\u00e1gio de fita. Ajustes mais altos empurram a satura\u00e7\u00e3o com mais for\u00e7a.",
    c_hpf="Filtro passa-altas antes do est\u00e1gio de fita, \u00fatil pra apertar o grave antes de chegar na satura\u00e7\u00e3o.",
    sec_tapetype_h="Tape Type",
    cap_tapetype="Tape Type",
    tt_gp9="F\u00f3rmula moderna, de headroom alto &mdash; a mais neutra, com agudos mais estendidos.",
    tt_456="F\u00f3rmula cl\u00e1ssica de est\u00fadio &mdash; m\u00e9dios quentes, compress\u00e3o suave.",
    tt_499="F\u00f3rmula impactante, um pouco mais brilhante, com um head bump mais firme.",
    tt_250="F\u00f3rmula vintage/de baixo output &mdash; agudos mais suaves, wow &amp; flutter e ru\u00eddo de fundo mais vis\u00edveis.",
    sec_drive_h="Drive / Satura\u00e7\u00e3o",
    cap_drivebias="Drive/Sat + Bias/Cal",
    c_drive="Quantidade de satura\u00e7\u00e3o de fita. Tamb\u00e9m controla o brilho das l\u00e2mpadas valvuladas na interface.",
    c_sat="Ativa/desativa completamente o est\u00e1gio de satura\u00e7\u00e3o.",
    c_bias="Bias de fita simulado &mdash; desloca a simetria da satura\u00e7\u00e3o e o balan\u00e7o harm\u00f4nico.",
    c_cal="Bias Calibrado &mdash; ativa a curva de bias calibrada de f\u00e1brica pro Tape Type selecionado.",
    sec_wow_h="Wow &amp; Flutter",
    cap_wowflutter="Wow &amp; Flutter",
    c_rate="Velocidade de modula\u00e7\u00e3o da instabilidade de afina\u00e7\u00e3o.",
    c_depth="Profundidade da modula\u00e7\u00e3o. Mantida baixa por padr\u00e3o pra uma qualidade sutil de \u201cvivo\u201d, em vez de um desafinamento audivel.",
    c_wowin="Ativa/desativa o wow &amp; flutter. Ligado por padr\u00e3o, de forma sutil.",
    sec_noise_h="Noise e Dropouts",
    cap_noise="Noise", cap_dropouts="Dropouts",
    noise_word="Noise", dropouts_word="Dropouts",
    c_noise="N\u00edvel de chiado (hiss) da fita.",
    c_noisein="Ativa/desativa o chiado. Desligado por padr\u00e3o.",
    c_dropouts="Frequ\u00eancia/severidade dos dropouts moment\u00e2neos de n\u00edvel.",
    c_dropoutsin="Ativa/desativa os dropouts. Desligado por padr\u00e3o.",
    sec_eq_h="EQ e Gain Link",
    cap_eqlink="EQ + Gain Link",
    c_lf="Controle de tom shelf de graves.",
    c_hf="Controle de tom shelf de agudos.",
    sec_output_h="Se\u00e7\u00e3o de Sa\u00edda",
    cap_output="Output + LPF",
    c_output="Trim aplicado depois do est\u00e1gio de fita.",
    c_lpf="Filtro passa-baixas depois do est\u00e1gio de fita, pra suavizar ainda mais os agudos.",
    c_link="Gain Link &mdash; veja abaixo.",
    gainlink_note="O <b>Gain Link</b> acopla o INPUT e o OUTPUT de forma que os dois se movem de forma inversa, dB por dB, mantendo o n\u00edvel geral praticamente constante enquanto voc\u00ea empurra a satura\u00e7\u00e3o mais forte ou mais suave. Enquanto ligado, arrastar qualquer um dos knobs mostra sua bolha de valor normal, mais uma correspondente sobre o outro knob, pra ambos os valores ligados ficarem vis\u00edveis ao mesmo tempo. Um \u00fanico clique (sem arrastar) no INPUT ou no OUTPUT reseta os dois pra 0 dB. Desligar o Link libera os dois knobs pra se moverem de forma independente de novo.",
    sec_speedhead_h="Tape Speed e Repro Head",
    cap_speedhead="Tape Speed e Repro Head",
    c_speed="Velocidades mais altas estendem a resposta de agudos e reduzem wow &amp; flutter/ru\u00eddo; velocidades mais baixas s\u00e3o mais escuras e coloridas.",
    c_reprohead="Padr\u00e3o de equaliza\u00e7\u00e3o de reprodu\u00e7\u00e3o &mdash; muda o balan\u00e7o tonal da curva de head-bump/roll-off.",
    sec_agemix_h="Tape Age e Mix",
    cap_agemix="Tape Age, Dropouts e Mix",
    age_range="0 (Nova) &ndash; 100 (Desgastada)",
    mix_range="0 (Dry) &ndash; 100 (Wet)",
    c_age="Controle global de \u201cdesgaste\u201d &mdash; suaviza gradualmente os agudos e aumenta imperfei\u00e7\u00f5es percept\u00edveis conforme aumentado.",
    c_mix="Mistura o sinal processado com a entrada intacta. 100% por padr\u00e3o pra uso tradicional em insert.",
    sec_global_h="Global",
    cap_meterbypass="Output Meter e Bypass",
    c_bypass="Faz bypass de todo o motor de fita, deixando o \u00e1udio passar sem processamento.",
    reels_h="Rolos, L\u00e2mpadas Valvuladas e Medi\u00e7\u00e3o",
    cap_reelsvu="Rolos, medidores VU e placa de identifica\u00e7\u00e3o",
    reels_h3="Transporte rolo a rolo",
    reels_p="Os dois rolos s\u00e3o cosm\u00e9ticos, mas interativos &mdash; clicar em qualquer um deles inicia ou para o transporte giratório dos dois (est\u00e3o enfiados na mesma fita). Cada Tape Type recolore o acabamento do rolo (dourado pro GP9, prateado pro 456, vermelho pro 499, preto pro 250), ent\u00e3o voc\u00ea sempre sabe qual f\u00f3rmula est\u00e1 carregada s\u00f3 de olhar. Eles tamb\u00e9m giram mais r\u00e1pido ou mais devagar pra combinar com o TAPE SPEED selecionado &mdash; mais r\u00e1pido em 30 IPS, mais devagar em 7.5 IPS.",
    tubes_h3="L\u00e2mpadas valvuladas",
    tubes_p="As duas l\u00e2mpadas ao lado dos rolos ficam mais brilhantes e com cor mais intensa conforme o DRIVE \u00e9 aumentado, e mant\u00eam um brilho de repouso fixo e vivo quando o SAT est\u00e1 desligado ou o plugin est\u00e1 em bypass &mdash; uma leitura visual de o qu\u00e3o forte o est\u00e1gio de fita est\u00e1 trabalhando.",
    vu_h3="Medidores VU e medidor de sa\u00edda",
    vu_p="Os dois medidores VU mostram o n\u00edvel do sinal processado numa escala anal\u00f3gica padr\u00e3o de -20&hellip;+3 dB. O medidor de barra estilo LED abaixo das fileiras de controle mostra os mesmos n\u00edveis de sa\u00edda esquerdo/direito num formato mais moderno de leitura de pico.",
    window_h="Janela e Logo",
    window_p1="A janela do plugin pode ser redimensionada livremente pelo canto, de aproximadamente um ter\u00e7o do tamanho nativo at\u00e9 o dobro, sempre mantendo as propor\u00e7\u00f5es originais.",
    window_note="<b>D\u00ea um duplo clique no logo \u201cNF TAPE MACHINE\u201d</b> no topo do painel a qualquer momento pra a janela voltar instantaneamente ao tamanho padr\u00e3o &mdash; \u00fatil depois de arrastar o canto pra deixar maior durante um trabalho de detalhe. Dar duplo clique quando a janela j\u00e1 est\u00e1 no tamanho padr\u00e3o n\u00e3o faz nada.",
    presets_h="Presets de F\u00e1brica",
    p_default="Ponto de partida sutil, inclinado pro transparente &mdash; satura\u00e7\u00e3o e wow &amp; flutter presentes mas contidos, noise e dropouts desligados.",
    p_warmbus="Calor e compress\u00e3o suaves da f\u00f3rmula 456 pra uso em mix-bus.",
    p_vintage="F\u00f3rmula 499 empurrada mais forte, com noise e dropouts ativados pra um car\u00e1ter vintage expl\u00edcito.",
    p_mastering="Toque bem leve na f\u00f3rmula 499 com repro head IEC &mdash; pra uso em cadeia de masteriza\u00e7\u00e3o.",
    p_lofi="F\u00f3rmula 250 empurrada forte, tape age alto, dropouts e noise todos ativados &mdash; car\u00e1ter lo-fi, bem degradado.",
    presets_p="Use as setas <b>&lt;</b> / <b>&gt;</b> dos dois lados do nome do preset pra passar pelo banco de f\u00e1brica.",
    tips_h="Dicas de Uso",
    tip1="Pra colagem de mix-bus, mantenha o DRIVE baixo (2&ndash;4), o MIX perto de 100%, e deixe NOISE/DROPOUTS desligados.",
    tip2="Ative o GAIN LINK antes de ajustar o DRIVE, pra voc\u00ea julgar o tom da satura\u00e7\u00e3o sem um salto de n\u00edvel confundir a compara\u00e7\u00e3o &mdash; clique em qualquer um dos knobs pra voltar a 0/0 e come\u00e7ar de novo.",
    tip3="Aumentar o TAPE AGE \u00e9 uma forma r\u00e1pida de ajustar um car\u00e1ter \u201cdesgastado\u201d sem mexer em cada par\u00e2metro individual na m\u00e3o.",
    tip4="Mude o TAPE SPEED pra 7.5 IPS pra um resultado noticeavelmente mais escuro e colorido; 30 IPS pro agudo mais limpo e estendido.",
    tip5="O DEPTH do WOW &amp; FLUTTER \u00e9 intencionalmente sutil por padr\u00e3o &mdash; aumente mais s\u00f3 quando quiser um efeito de afina\u00e7\u00e3o obviamente bamba, tipo lo-fi.",
    tip6="D\u00ea um duplo clique no logo NF no topo do painel pra voltar a janela ao tamanho padr\u00e3o depois de deix\u00e1-la maior pra um trabalho de detalhe.",
    credits_h="Cr\u00e9ditos e Vers\u00e3o",
    credits_p="O NF Tape Machine \u00e9 desenvolvido pela NF Audio Tools. Este manual e o plugin descrevem a vers\u00e3o " + VERSION + ".",
),
}


if __name__ == "__main__":
    for lang, fname in (("en", "NF_Tape_Machine_Manual_EN.html"), ("pt", "NF_Tape_Machine_Manual_PT.html")):
        html = build(lang)
        (here / fname).write_text(html, encoding="utf-8")
        print("wrote", fname, len(html), "bytes")
