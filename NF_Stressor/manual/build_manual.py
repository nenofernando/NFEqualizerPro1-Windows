#!/usr/bin/env python3
"""Generates the bilingual NF - Stressor manual (EN + PT) as standalone HTML
files with embedded (base64) screenshots, matching the style used by the
NF Tape Machine manual. Run this, then print each HTML to PDF (see
build_pdf.sh in this same folder)."""

import base64
from pathlib import Path

HERE = Path(__file__).parent


def b64(name):
    return base64.b64encode((HERE / name).read_bytes()).decode("ascii")


IMAGES = {
    "panel": b64("full_panel.png"),
    "top": b64("top_plate.png"),
    "knobs": b64("knobs_section.png"),
    "ratio": b64("ratio_row.png"),
    "character": b64("character_grid.png"),
    "mix": b64("mix_nuke.png"),
    "nuke_on": b64("nuke_on_zoom.png"),
    "dist_on": b64("dist_on_zoom.png"),
    "opto_on": b64("opto_zoom.png"),
    "bypass_on": b64("bypass_nuke_zoom.png"),
    "preset_menu": b64("preset_menu_zoom.png"),
    "meter_demo": b64("gr_meter_zoom.png"),
}

VERSION = "V1.0"

CSS = """
@page { size: A4; margin: 0; }
* { box-sizing: border-box; }
body {
    margin: 0; padding: 0;
    font-family: -apple-system, "Helvetica Neue", Helvetica, Arial, sans-serif;
    color: #d9d9dc;
    background: #141416;
}
.page {
    width: 210mm; min-height: 297mm;
    padding: 18mm 20mm;
    page-break-after: always;
    position: relative;
}
.page:last-child { page-break-after: auto; }

.cover { display: flex; flex-direction: column; justify-content: center; align-items: center; text-align: center; background: radial-gradient(circle at 50% 30%, #2c2c30, #0e0e10 75%); }
.cover .brand { color: #86868c; letter-spacing: 4px; font-size: 13px; text-transform: uppercase; margin-bottom: 28px; }
.cover h1 { font-size: 52px; margin: 0; color: #f2efe6; letter-spacing: 1px; }
.cover .tagline { color: #ffab3d; letter-spacing: 3px; font-size: 14px; margin-top: 10px; text-transform: uppercase; }
.cover img { max-width: 260px; margin: 46px 0 46px; filter: drop-shadow(0 10px 40px rgba(0,0,0,0.6)); border-radius: 6px; }
.cover .sub { color: #86868c; font-size: 13px; margin-top: 4px; }
.cover .version { position: absolute; bottom: 16mm; color: #55555a; font-size: 11px; letter-spacing: 1px; }
.cover .rights { position: absolute; bottom: 11mm; color: #45454a; font-size: 9.5px; letter-spacing: 0.5px; }

h2 { color: #ffab3d; font-size: 22px; border-bottom: 1px solid #3a3a3d; padding-bottom: 8px; margin: 0 0 16px; text-transform: uppercase; letter-spacing: 1px; }
h3 { color: #f2efe6; font-size: 15px; margin: 22px 0 6px; }
p, li { font-size: 12.5px; line-height: 1.65; color: #c7c7cc; }
ul { margin: 6px 0; padding-left: 20px; }
strong { color: #f2efe6; }
.section-img { text-align: center; margin: 14px 0 20px; }
.section-img img { max-width: 100%; border-radius: 4px; box-shadow: 0 6px 24px rgba(0,0,0,0.5); }
.section-img.knobs img { max-width: 55%; }
.section-img.top img, .section-img.ratio img, .section-img.mix img { max-width: 90%; }
.section-img.character img { max-width: 62%; }
.section-img.inline { margin: 10px 0 16px; }
.section-img.inline img { max-width: 70%; }
.section-img.inline.wide img { max-width: 90%; }
.img-caption { text-align: center; font-size: 11px; color: #86868c; margin-top: -12px; margin-bottom: 18px; font-style: italic; }
.control { margin-bottom: 18px; }
.control .name { color: #ffab3d; font-weight: 600; font-size: 13px; letter-spacing: 0.5px; }
.two-col { display: flex; gap: 24px; }
.two-col > div { flex: 1; }
table.steps { width: 100%; border-collapse: collapse; margin: 10px 0 18px; font-size: 12px; }
table.steps th, table.steps td { border: 1px solid #3a3a3d; padding: 6px 10px; text-align: left; }
table.steps th { background: #1e1e20; color: #ffab3d; }
.callout { background: #1c1c1e; border-left: 3px solid #ffab3d; padding: 10px 14px; margin: 14px 0; font-size: 12px; border-radius: 0 4px 4px 0; }
.callout.red { border-left-color: #ff4a36; }
.footer-note { position: absolute; bottom: 14mm; left: 20mm; right: 20mm; font-size: 10px; color: #55555a; text-align: center; border-top: 1px solid #2a2a2c; padding-top: 8px; }
.pagenum { position: absolute; bottom: 14mm; right: 20mm; font-size: 10px; color: #55555a; }
"""


def page(title, body, footer=None, pagenum=None):
    foot = f'<div class="footer-note">{footer}</div>' if footer else ""
    num = f'<div class="pagenum">{pagenum}</div>' if pagenum else ""
    return f'<div class="page">{body}{foot}{num}</div>'


def build(lang: str) -> str:
    t = TEXT[lang]

    cover = f"""
    <div class="page cover">
        <div class="brand">NF Audio Tools</div>
        <h1>NF &ndash; STRESSOR</h1>
        <div class="tagline">REDLINE / 1% THD</div>
        <img src="data:image/png;base64,{IMAGES['panel']}" />
        <div class="sub">{t['ownersManual']}</div>
        <div class="version">{VERSION} &middot; {t['footer']}</div>
        <div class="rights">NENNO FERNANDO AUDIO TOOLS&reg; &mdash; {t['footer']}</div>
    </div>
    """

    overview = page(t['overview_h'], f"""
        <h2>{t['overview_h']}</h2>
        <p>{t['overview_p1']}</p>
        <p>{t['overview_p2']}</p>
        <div class="callout">{t['overview_callout']}</div>
        <h3>{t['formats_h']}</h3>
        <ul>
            <li>{t['formats_mac']}</li>
            <li>{t['formats_win']}</li>
        </ul>
    """, footer=t['footer'], pagenum="2")

    install = page(t['install_h'], f"""
        <h2>{t['install_h']}</h2>
        <div class="two-col">
            <div>
                <h3>macOS</h3>
                <p>{t['install_mac_p']}</p>
                <ul>
                    <li><strong>VST3</strong>: <code>/Library/Audio/Plug-Ins/VST3/</code></li>
                    <li><strong>AU</strong>: <code>/Library/Audio/Plug-Ins/Components/</code></li>
                </ul>
            </div>
            <div>
                <h3>Windows</h3>
                <p>{t['install_win_p']}</p>
                <ul>
                    <li><strong>VST3</strong>: <code>C:\\Program Files\\Common Files\\VST3\\</code></li>
                </ul>
            </div>
        </div>
        <div class="callout">{t['install_rescan']}</div>
    """, footer=t['footer'], pagenum="3")

    tour = page(t['tour_h'], f"""
        <h2>{t['tour_h']}</h2>
        <p>{t['tour_p']}</p>
        <div class="section-img top"><img src="data:image/png;base64,{IMAGES['top']}" /></div>
        <p>{t['tour_top']}</p>
        <div class="control"><div class="name">BYPASS</div><p>{t['c_bypass']}</p></div>
        <div class="section-img inline"><img src="data:image/png;base64,{IMAGES['bypass_on']}" /></div>
        <div class="img-caption">{t['cap_bypass']}</div>
        <div class="section-img knobs"><img src="data:image/png;base64,{IMAGES['knobs']}" /></div>
        <p>{t['tour_knobs']}</p>
    """, footer=t['footer'], pagenum="4")

    ctrl1 = page(t['controls_h'], f"""
        <h2>{t['controls_h']}</h2>
        <div class="control"><div class="name">INPUT</div><p>{t['c_input']}</p></div>
        <div class="control"><div class="name">ATTACK</div><p>{t['c_attack']}</p></div>
        <div class="control"><div class="name">RELEASE</div><p>{t['c_release']}</p></div>
        <div class="callout">{t['c_opto']}</div>
        <div class="section-img inline wide"><img src="data:image/png;base64,{IMAGES['opto_on']}" /></div>
        <div class="img-caption">{t['cap_opto']}</div>
        <div class="control"><div class="name">OUTPUT</div><p>{t['c_output']}</p></div>
    """, footer=t['footer'], pagenum="5")

    ctrl2 = page(t['ratio_h'], f"""
        <h2>{t['ratio_h']}</h2>
        <p>{t['c_ratio']}</p>
        <div class="section-img ratio"><img src="data:image/png;base64,{IMAGES['ratio']}" /></div>
        <table class="steps">
            <tr><th>1:1</th><th>2:1</th><th>3:1</th><th>4:1</th><th>6:1</th><th>10:1</th><th>20:1</th></tr>
            <tr><td colspan="5">{t['ratio_soft']}</td><td colspan="2">{t['ratio_redline']}</td></tr>
        </table>
        <div class="callout red">{t['c_nuke']}</div>
        <div class="section-img inline"><img src="data:image/png;base64,{IMAGES['nuke_on']}" /></div>
        <div class="img-caption">{t['cap_nuke']}</div>
    """, footer=t['footer'], pagenum="6")

    ctrl3 = page(t['character_h'], f"""
        <h2>{t['character_h']}</h2>
        <div class="section-img character"><img src="data:image/png;base64,{IMAGES['character']}" /></div>
        <h3>DETECTOR</h3>
        <div class="control"><div class="name">HP</div><p>{t['c_hp']}</p></div>
        <div class="control"><div class="name">LINK</div><p>{t['c_link']}</p></div>
        <h3>AUDIO</h3>
        <div class="control"><div class="name">DIST 2 / DIST 3</div><p>{t['c_dist']}</p></div>
        <div class="control"><div class="name">{t['c_outhp_name']}</div><p>{t['c_outhp']}</p></div>
        <div class="control"><div class="name">NUKE</div><p>{t['c_nuke_short']}</p></div>
        <div class="section-img inline wide"><img src="data:image/png;base64,{IMAGES['dist_on']}" /></div>
        <div class="img-caption">{t['cap_dist']}</div>
    """, footer=t['footer'], pagenum="7")

    ctrl4 = page(t['mix_h'], f"""
        <h2>{t['mix_h']}</h2>
        <div class="section-img mix"><img src="data:image/png;base64,{IMAGES['mix']}" /></div>
        <div class="control"><div class="name">MIX</div><p>{t['c_mix']}</p></div>
    """, footer=t['footer'], pagenum="8")

    ctrl5 = page(t['c_meter_name'], f"""
        <h2>{t['c_meter_name']}</h2>
        <p>{t['c_meter']}</p>
        <div class="section-img inline wide"><img src="data:image/png;base64,{IMAGES['meter_demo']}" /></div>
        <div class="img-caption">{t['cap_meter']}</div>
    """, footer=t['footer'], pagenum="9")

    shortcuts = page(t['shortcuts_h'], f"""
        <h2>{t['shortcuts_h']}</h2>
        <div class="control"><div class="name">{t['sc_knob_name']}</div><p>{t['sc_knob']}</p></div>
        <div class="control"><div class="name">{t['sc_logo_name']}</div><p>{t['sc_logo']}</p></div>
    """, footer=t['footer'], pagenum="10")

    presets = page(t['presets_h'], f"""
        <h2>{t['presets_h']}</h2>
        <p>{t['presets_p1']}</p>
        <div class="section-img inline"><img src="data:image/png;base64,{IMAGES['preset_menu']}" /></div>
        <div class="img-caption">{t['cap_presets']}</div>
        <ul>
            <li><strong>{t['presets_default']}</strong> &mdash; {t['presets_default_d']}</li>
            <li><strong>{t['presets_save']}</strong> &mdash; {t['presets_save_d']}</li>
            <li><strong>{t['presets_load']}</strong> &mdash; {t['presets_load_d']}</li>
        </ul>
        <p>{t['presets_p2']}</p>
    """, footer=t['footer'], pagenum="11")

    tips = page(t['tips_h'], f"""
        <h2>{t['tips_h']}</h2>
        <ul>
            <li>{t['tip1']}</li>
            <li>{t['tip2']}</li>
            <li>{t['tip3']}</li>
            <li>{t['tip4']}</li>
            <li>{t['tip5']}</li>
        </ul>
    """, footer=t['footer'], pagenum="12")

    credits = page(t['credits_h'], f"""
        <h2>{t['credits_h']}</h2>
        <p>{t['credits_p']}</p>
        <p style="margin-top:30px;color:#86868c">NF &ndash; STRESSOR &middot; {VERSION}<br/>NF Audio Tools &middot; {t['footer']}</p>
        <p style="margin-top:18px;color:#55555a;font-size:11px">NENNO FERNANDO AUDIO TOOLS&reg; &mdash; {t['footer']}</p>
    """, footer=t['footer'], pagenum="13")

    return f"""<!doctype html><html><head><meta charset="utf-8"><title>NF - Stressor Manual</title><style>{CSS}</style></head><body>
    {cover}{overview}{install}{tour}{ctrl1}{ctrl2}{ctrl3}{ctrl4}{ctrl5}{shortcuts}{presets}{tips}{credits}
    </body></html>"""


TEXT = {
"en": dict(
    ownersManual="Owner's Manual",
    footer="All rights reserved.",
    overview_h="Overview",
    overview_p1="NF &ndash; Stressor is a faithful, from-scratch emulation of a classic opto/FET rack compressor, built as a tall, narrow vertical strip inspired by outboard hardware. It covers the full signature behaviour of the original design: a fixed internal threshold driven by the INPUT knob, a true discrete OPTO mode tied to the RATIO 10:1 position, a hard-knee \u201cNUKE\u201d ratio stage, independent character (distortion) switches, and a two-stage output high-pass filter for cleaning up low-end buildup.",
    overview_p2="Everything on the panel mirrors real hardware conventions &mdash; there is no dedicated THRESHOLD knob; instead, INPUT drives your signal harder or softer against a fixed internal reference, exactly like the classic units this plugin is inspired by.",
    overview_callout="Tip: because there's no threshold control, gain reduction is driven entirely by how hard you push INPUT. Start low and raise it until the GR meter starts moving.",
    formats_h="Formats",
    formats_mac="macOS: VST3 and Audio Unit (AU), Intel and Apple Silicon.",
    formats_win="Windows: VST3, 64-bit.",
    install_h="Installation",
    install_mac_p="Run the NF &ndash; Stressor installer package and follow the prompts. It installs VST3, AU and the bilingual PDF manual into the standard system-wide folders:",
    install_win_p="Run the NF - Stressor installer and follow the wizard. It installs the VST3 into the standard system-wide folder:",
    install_rescan="After installing, rescan plug-ins in your DAW if it doesn't pick up NF &ndash; Stressor automatically.",
    tour_h="Interface Tour",
    tour_p="The panel is a single vertical strip, read top to bottom: brand plate at the top with the BYPASS switch in its top-right corner, the four main knobs with the OPTO light and gain-reduction ladder meter beside them, the RATIO row, the DETECTOR/AUDIO character switches (AUDIO now also holds the output HP filter and NUKE), and finally MIX on its own.",
    tour_top="The hamburger icon (top-left) opens the preset menu. Double-clicking the brand plate itself, or the bare top-left corner of the chassis, snaps the window back to its default size &mdash; handy after resizing it larger for detail work.",
    tour_knobs="INPUT, ATTACK, RELEASE and OUTPUT, each with a rotating numbered dial (0&ndash;10) and a fixed red index mark at the top. The ladder of LEDs on the right shows live gain reduction in dB.",
    controls_h="Main Controls",
    c_input="Drives the signal into the compressor's fixed internal threshold. There is no separate threshold control &mdash; turning INPUT up pushes the signal harder against that reference, increasing gain reduction.",
    c_attack="Sets how quickly the compressor reacts to a transient, from a fast FET-like response at the SLOW-labelled end of its travel to a much slower response toward the FAST-labelled end &mdash; small tick marks beside the knob mark exactly which end is which.",
    c_release="Sets how quickly gain reduction lets go after the signal drops, from a fast release at the FAST-labelled end of its travel to a slow one toward the SLOW-labelled end.",
    c_opto="OPTO mode: selecting RATIO 10:1 (or clicking the OPTO light itself) engages it, snapping ATTACK to 10 and RELEASE to 0 &mdash; the panel's own \u201cOPTO\u201d detent, exactly like the hardware's end-of-travel position. Both knobs stay fully interactive the whole time: dragging either one away from that position turns the OPTO light back off (RATIO can stay at 10:1 &mdash; the light always reflects what the engine is actually doing, never just where RATIO is set). While genuinely engaged, RELEASE becomes program-dependent: a short, shallow hit recovers quickly, while a deep or sustained one lets go progressively more slowly, smoothly, up to several seconds &mdash; never in audible steps. Double-click the OPTO light for a hard exit back to whichever RATIO you were on before.",
    c_output="Trims the final output level after compression, character shaping and mix.",
    ratio_h="RATIO",
    c_ratio="Selects the compression ratio. 1:1 through 6:1 behave as classic soft-knee compression. 10:1 and 20:1 engage an extra \u201credline\u201d stage: a harder knee and a touch more drive into the character stage, matching the tagline lighting up amber. 10:1 is also the panel's dedicated OPTO position &mdash; see OPTO mode above.",
    ratio_soft="Soft-knee compression",
    ratio_redline="Redline / harder knee",
    c_nuke="NUKE: a dedicated button, independent of RATIO, living in the AUDIO column below the output HP filter (it used to sit beside MIX). Engaging it layers a true brick-wall limiter on top of whatever ratio is dialled in &mdash; an effectively infinite ratio, a razor-thin knee, a near-instant catch regardless of the ATTACK knob, and extra harmonic bite. It lights blue instead of red, so it still reads as a more extreme switch even though it now shares DIST 2/DIST 3's exact pill shape. Use it when you need a hard ceiling, not just more compression.",
    character_h="DETECTOR & AUDIO",
    c_hp="Engages a fixed high-pass filter in the sidechain, so low end doesn't dominate the detector's decisions &mdash; useful on bass-heavy sources.",
    c_link="Links the detector across both channels of a stereo signal so gain reduction tracks together, avoiding image shift on stereo material.",
    c_dist="Two independent character switches shaping the tone of the compressed signal: DIST 2 alone gives an FET-ish, asymmetric edge; DIST 3 alone gives a smoother, more opto-ish drive; engaging both together gives a coloured, more aggressive \u201cBritish\u201d character.",
    c_outhp_name="OUTPUT HP (HPF)",
    c_outhp="A three-position switch, cycling Off &rarr; ~70 Hz &rarr; ~120 Hz on each click. Engages a clean high-pass filter after compression, character shaping and mix &mdash; aimed at the low-end buildup the character switches (or the source itself) can leave behind, e.g. on vocals. The button lights red like the other AUDIO switches, and its label shows the active cutoff.",
    c_nuke_short="Now lives here in the AUDIO column, below the output HP filter, lit blue instead of red &mdash; see NUKE under RATIO for its full brick-wall-limiter behaviour.",
    mix_h="MIX",
    c_mix="Blends dry and compressed (wet) signal, for parallel compression &mdash; keep the punch of the uncompressed signal while still catching peaks.",
    c_bypass="Disengages all processing. Sits in the top-right corner of the brand plate, matching the hamburger menu button's slot on the opposite side. The whole button blinks while engaged, so it's never ambiguous that the plugin is doing nothing.",
    c_meter_name="Gain Reduction Ladder",
    c_meter="A vertical LED ladder marked in dB (1 through 26) shows live gain reduction: green for light reduction, amber from 6 dB, red from 12 dB up &mdash; matching the NUKE/redline territory. The \u201cGR\u201d label sits directly above the first LED, and the whole ladder is positioned so its 4 dB step lines up with the OPTO light beside ATTACK.",
    shortcuts_h="Window & Shortcuts",
    sc_knob_name="Double-click any main knob",
    sc_knob="INPUT, ATTACK, RELEASE, OUTPUT and MIX all snap straight back to that parameter's own factory default the moment you double-click them &mdash; the same convention used by other Distressor-style plug-ins, so you never have to hunt for where a control started.",
    sc_logo_name="Double-click the logo / chassis corner",
    sc_logo="Double-clicking the \u201cNF &ndash; STRESSOR\u201d brand plate, or the bare top-left corner of the chassis, instantly resets the plug-in window to its default size &mdash; a quick way back after dragging the corner resizer larger for detail work. It only affects window size, never any parameter.",
    presets_h="Presets",
    presets_p1="Click the hamburger icon in the top-left corner to open the preset menu:",
    presets_default="Default",
    presets_default_d="resets every control to its factory value.",
    presets_save="Save Preset&hellip;",
    presets_save_d="writes the current settings to a preset file you name.",
    presets_load="Load Preset&hellip;",
    presets_load_d="opens a preset file from anywhere on disk.",
    presets_p2="Any preset saved into the default presets folder also appears directly in this menu for one-click recall.",
    tips_h="Usage Tips",
    tip1="For vocals: moderate RATIO (3:1&ndash;4:1), ATTACK around the middle, or RATIO 10:1 for OPTO's natural, program-dependent release.",
    tip2="For drum bus glue: LINK on, HP on to keep kick/bass from pumping the detector, RATIO 4:1&ndash;6:1.",
    tip3="For aggressive drum room/parallel duty: push INPUT hard, RATIO 20:1, MIX under 50% to blend in the squashed signal under the dry track.",
    tip4="Reach for NUKE only when you need a hard ceiling on top of your ratio &mdash; it's a limiter stage, not a subtler compression option.",
    tip5="Double-click a knob any time you want to compare against its factory default without reaching for the preset menu.",
    cap_opto="OPTO engaged: RATIO at 10:1, ATTACK snapped to 10, RELEASE snapped to 0, the light lit beside ATTACK.",
    cap_nuke="NUKE engaged (blue), in the AUDIO column below the output HP filter.",
    cap_dist="DIST 2, DIST 3, the output HP filter (at 70 Hz) and NUKE, all engaged together in the AUDIO column.",
    cap_bypass="BYPASS engaged (blinking red), top-right corner of the brand plate.",
    cap_meter="\u201cGR\u201d directly over the first LED; the ladder shifted so its 4 dB step lines up with the OPTO light.",
    cap_presets="The preset menu, with a saved preset ready to recall.",
    credits_h="Credits & Version",
    credits_p="NF &ndash; Stressor is developed by NF Audio Tools. This manual and the plugin describe version " + VERSION + "."
),
"pt": dict(
    ownersManual="Manual do Usu\u00e1rio",
    footer="Todos os direitos reservados.",
    overview_h="Vis\u00e3o Geral",
    overview_p1="O NF &ndash; Stressor \u00e9 uma emula\u00e7\u00e3o fiel, feita do zero, de um cl\u00e1ssico compressor de rack opto/FET, constru\u00eddo como uma tira vertical estreita inspirada em equipamentos de outboard. Ele reproduz todo o comportamento caracter\u00edstico do design original: um limiar interno fixo controlado pelo knob INPUT, um verdadeiro modo OPTO discreto ligado \u00e0 posi\u00e7\u00e3o RATIO 10:1, um est\u00e1gio de raz\u00e3o \u201cNUKE\u201d de joelho duro, chaves de car\u00e1ter (distor\u00e7\u00e3o) independentes, e um filtro passa-altas de sa\u00edda de dois est\u00e1gios para limpar sobra de grave.",
    overview_p2="Tudo no painel segue as conven\u00e7\u00f5es do hardware real &mdash; n\u00e3o existe um knob de THRESHOLD dedicado; em vez disso, o INPUT empurra seu sinal com mais ou menos for\u00e7a contra uma refer\u00eancia interna fixa, exatamente como nos equipamentos cl\u00e1ssicos que inspiraram este plugin.",
    overview_callout="Dica: como n\u00e3o h\u00e1 controle de threshold, a redu\u00e7\u00e3o de ganho \u00e9 controlada inteiramente por o qu\u00e3o forte voc\u00ea empurra o INPUT. Comece baixo e v\u00e1 aumentando at\u00e9 o medidor de GR come\u00e7ar a se mover.",
    formats_h="Formatos",
    formats_mac="macOS: VST3 e Audio Unit (AU), Intel e Apple Silicon.",
    formats_win="Windows: VST3, 64 bits.",
    install_h="Instala\u00e7\u00e3o",
    install_mac_p="Execute o pacote instalador do NF &ndash; Stressor e siga as instru\u00e7\u00f5es. Ele instala o VST3, o AU e o manual bilingue em PDF nas pastas padr\u00e3o do sistema:",
    install_win_p="Execute o instalador do NF - Stressor e siga o assistente. Ele instala o VST3 na pasta padr\u00e3o do sistema:",
    install_rescan="Depois de instalar, refa\u00e7a a busca de plugins na sua DAW caso ela n\u00e3o reconhe\u00e7a o NF &ndash; Stressor automaticamente.",
    tour_h="Tour pela Interface",
    tour_p="O painel \u00e9 uma \u00fanica tira vertical, lida de cima para baixo: placa de marca no topo com o bot\u00e3o BYPASS no canto superior direito, os quatro knobs principais com a luz OPTO e o medidor de redu\u00e7\u00e3o de ganho ao lado, a fileira de RATIO, as chaves de car\u00e1ter DETECTOR/AUDIO (a coluna AUDIO agora tamb\u00e9m tem o filtro passa-altas de sa\u00edda e o NUKE), e por fim o MIX sozinho.",
    tour_top="O \u00edcone de menu (canto superior esquerdo) abre o menu de presets. Dar duplo clique na pr\u00f3pria placa de marca, ou no canto superior esquerdo vazio do chassi, faz a janela voltar ao tamanho padr\u00e3o &mdash; \u00fatil depois de deix\u00e1-la maior pra um trabalho de detalhe.",
    tour_knobs="INPUT, ATTACK, RELEASE e OUTPUT, cada um com um disco numerado giratório (0&ndash;10) e uma marca vermelha fixa no topo. A escada de LEDs \u00e0 direita mostra a redu\u00e7\u00e3o de ganho em tempo real, em dB.",
    controls_h="Controles Principais",
    c_input="Empurra o sinal contra o limiar interno fixo do compressor. N\u00e3o existe um controle de threshold separado &mdash; girar o INPUT para cima empurra o sinal com mais for\u00e7a contra essa refer\u00eancia, aumentando a redu\u00e7\u00e3o de ganho.",
    c_attack="Define a rapidez com que o compressor reage a um transiente, de uma resposta r\u00e1pida tipo FET no lado marcado SLOW at\u00e9 uma resposta bem mais lenta perto do lado marcado FAST &mdash; pequenas marcas ao lado do knob indicam exatamente qual lado \u00e9 qual.",
    c_release="Define a rapidez com que a redu\u00e7\u00e3o de ganho \u00e9 solta ap\u00f3s o sinal cair, de uma libera\u00e7\u00e3o r\u00e1pida no lado marcado FAST a uma lenta perto do lado marcado SLOW.",
    c_opto="Modo OPTO: selecionar o RATIO 10:1 (ou clicar na pr\u00f3pria luz OPTO) ativa o modo, fazendo o ATTACK saltar pra 10 e o RELEASE pra 0 &mdash; o detent \u201cOPTO\u201d do painel, igual \u00e0 posi\u00e7\u00e3o de fim de curso do hardware. Os dois knobs continuam totalmente interativos o tempo todo: arrastar qualquer um deles pra longe dessa posi\u00e7\u00e3o apaga a luz OPTO de novo (o RATIO pode continuar em 10:1 &mdash; a luz sempre reflete o que o motor est\u00e1 realmente fazendo, nunca s\u00f3 onde o RATIO est\u00e1 selecionado). Enquanto genuinamente ativo, o RELEASE fica program-dependent: um golpe curto e raso recupera r\u00e1pido, enquanto um golpe profundo ou sustentado solta progressivamente mais devagar, de forma suave, podendo chegar a v\u00e1rios segundos &mdash; nunca em saltos audiveis. D\u00ea um duplo clique na luz OPTO pra sair completamente, voltando pro RATIO que voc\u00ea estava usando antes.",
    c_output="Ajusta o n\u00edvel final de sa\u00edda ap\u00f3s a compress\u00e3o, a modelagem de car\u00e1ter e o mix.",
    ratio_h="RATIO",
    c_ratio="Seleciona a raz\u00e3o de compress\u00e3o. De 1:1 a 6:1 o comportamento \u00e9 de compress\u00e3o soft-knee cl\u00e1ssica. Em 10:1 e 20:1 um est\u00e1gio extra \u201credline\u201d \u00e9 ativado: um joelho mais duro e um toque a mais de satura\u00e7\u00e3o no est\u00e1gio de car\u00e1ter, com a etiqueta do painel acendendo em \u00e2mbar. O 10:1 tamb\u00e9m \u00e9 a posi\u00e7\u00e3o dedicada de OPTO do painel &mdash; veja o modo OPTO acima.",
    ratio_soft="Compress\u00e3o soft-knee",
    ratio_redline="Redline / joelho mais duro",
    c_nuke="NUKE: um bot\u00e3o dedicado, independente do RATIO, que agora mora na coluna AUDIO, abaixo do filtro passa-altas de sa\u00edda (antes ficava ao lado do MIX). Ao ativ\u00e1-lo, um limitador brick-wall de verdade \u00e9 somado por cima do que estiver selecionado no RATIO &mdash; uma raz\u00e3o efetivamente infinita, um joelho fin\u00edssimo, captura quase instant\u00e2nea (ignorando o knob ATTACK) e uma mordida harm\u00f4nica extra. Ele acende azul em vez de vermelho, ent\u00e3o continua lendo como uma chave mais extrema mesmo compartilhando o mesmo formato de pill do DIST 2/DIST 3. Use quando precisar de um teto r\u00edgido, n\u00e3o s\u00f3 de mais compress\u00e3o.",
    character_h="DETECTOR & AUDIO",
    c_hp="Ativa um filtro passa-altas fixo no sidechain, para que os graves n\u00e3o dominem as decis\u00f5es do detector &mdash; \u00fatil em fontes com muito grave.",
    c_link="Liga a detec\u00e7\u00e3o entre os dois canais de um sinal est\u00e9reo, para que a redu\u00e7\u00e3o de ganho acompanhe junto, evitando deslocamento de imagem em material est\u00e9reo.",
    c_dist="Duas chaves de car\u00e1ter independentes que moldam o timbre do sinal comprimido: DIST 2 sozinho d\u00e1 um ar tipo FET, assim\u00e9trico; DIST 3 sozinho d\u00e1 uma satura\u00e7\u00e3o mais suave, tipo opto; ativando os dois juntos d\u00e1 um car\u00e1ter colorido, mais agressivo, \u201cBritish\u201d.",
    c_outhp_name="HP DE SA\u00cdDA (HPF)",
    c_outhp="Um bot\u00e3o de tr\u00eas posi\u00e7\u00f5es, que alterna Desligado &rarr; ~70 Hz &rarr; ~120 Hz a cada clique. Ativa um filtro passa-altas limpo depois da compress\u00e3o, da modelagem de car\u00e1ter e do mix &mdash; mirando na sobra de grave que as chaves de car\u00e1ter (ou a pr\u00f3pria fonte) podem deixar, por exemplo em vocais. O bot\u00e3o acende vermelho como as outras chaves da coluna AUDIO, e o texto mostra o corte ativo.",
    c_nuke_short="Agora mora aqui na coluna AUDIO, abaixo do filtro passa-altas de sa\u00edda, acendendo azul em vez de vermelho &mdash; veja o NUKE em RATIO para o comportamento completo de limitador brick-wall.",
    mix_h="MIX",
    c_mix="Mistura o sinal seco (dry) com o comprimido (wet), para compress\u00e3o paralela &mdash; mant\u00e9m o impacto do sinal n\u00e3o comprimido enquanto ainda captura os picos.",
    c_bypass="Desliga todo o processamento. Fica no canto superior direito da placa de marca, espelhando o bot\u00e3o de menu (hamb\u00farguer) do lado oposto. O bot\u00e3o inteiro pisca enquanto ativado, deixando bem claro que o plugin n\u00e3o est\u00e1 fazendo nada.",
    c_meter_name="Escada de Redu\u00e7\u00e3o de Ganho",
    c_meter="Uma escada vertical de LEDs marcada em dB (de 1 a 26) mostra a redu\u00e7\u00e3o de ganho em tempo real: verde para redu\u00e7\u00e3o leve, \u00e2mbar a partir de 6 dB, vermelho a partir de 12 dB &mdash; coincidindo com o território do NUKE/redline. A etiqueta \u201cGR\u201d fica bem em cima do primeiro LED, e a escada inteira \u00e9 posicionada de forma que o degrau de 4 dB fique alinhado com a luz OPTO ao lado do ATTACK.",
    shortcuts_h="Janela e Atalhos",
    sc_knob_name="Duplo clique em qualquer knob principal",
    sc_knob="INPUT, ATTACK, RELEASE, OUTPUT e MIX voltam direto pro valor de f\u00e1brica daquele par\u00e2metro assim que voc\u00ea d\u00e1 um duplo clique &mdash; a mesma conven\u00e7\u00e3o usada por outros plugins estilo Distressor, pra voc\u00ea nunca precisar ca\u00e7ar onde um controle come\u00e7ou.",
    sc_logo_name="Duplo clique no logo / canto do chassi",
    sc_logo="Dar duplo clique na placa de marca \u201cNF &ndash; STRESSOR\u201d, ou no canto superior esquerdo vazio do chassi, reseta instantaneamente a janela do plugin pro tamanho padr\u00e3o &mdash; uma forma r\u00e1pida de voltar depois de arrastar o canto pra deixar maior durante um trabalho de detalhe. S\u00f3 afeta o tamanho da janela, nunca nenhum par\u00e2metro.",
    presets_h="Presets",
    presets_p1="Clique no \u00edcone de menu no canto superior esquerdo para abrir o menu de presets:",
    presets_default="Default",
    presets_default_d="restaura todos os controles para o valor de f\u00e1brica.",
    presets_save="Save Preset&hellip;",
    presets_save_d="grava os ajustes atuais em um arquivo de preset com o nome que voc\u00ea escolher.",
    presets_load="Load Preset&hellip;",
    presets_load_d="abre um arquivo de preset de qualquer lugar do disco.",
    presets_p2="Qualquer preset salvo na pasta padr\u00e3o de presets tamb\u00e9m aparece direto nesse menu, pronto pra carregar com um clique.",
    tips_h="Dicas de Uso",
    tip1="Para vocais: RATIO moderado (3:1&ndash;4:1), ATTACK pela metade, ou RATIO 10:1 pra liberação natural e program-dependent do OPTO.",
    tip2="Para colar o bus de bateria: LINK ligado, HP ligado para o kick/grave n\u00e3o bombear o detector, RATIO 4:1&ndash;6:1.",
    tip3="Para sala de bateria agressiva/paralela: empurre o INPUT forte, RATIO 20:1, MIX abaixo de 50% para misturar o sinal esmagado por baixo da faixa seca.",
    tip4="Use o NUKE apenas quando precisar de um teto r\u00edgido por cima da sua raz\u00e3o &mdash; \u00e9 um est\u00e1gio de limita\u00e7\u00e3o, n\u00e3o uma op\u00e7\u00e3o de compress\u00e3o mais sutil.",
    tip5="D\u00ea um duplo clique num knob sempre que quiser comparar contra o valor de f\u00e1brica sem precisar abrir o menu de presets.",
    cap_opto="OPTO ativado: RATIO em 10:1, ATTACK saltando pra 10, RELEASE saltando pra 0, luz acesa ao lado do ATTACK.",
    cap_nuke="NUKE ativado (azul), na coluna AUDIO, abaixo do filtro passa-altas de sa\u00edda.",
    cap_dist="DIST 2, DIST 3, o filtro passa-altas de sa\u00edda (em 70 Hz) e o NUKE, todos ativados juntos na coluna AUDIO.",
    cap_bypass="BYPASS ativado (piscando em vermelho), no canto superior direito da placa de marca.",
    cap_meter="\u201cGR\u201d bem em cima do primeiro LED; a escada deslocada pra o degrau de 4 dB alinhar com a luz OPTO.",
    cap_presets="O menu de presets, com um preset salvo pronto para carregar.",
    credits_h="Cr\u00e9ditos e Vers\u00e3o",
    credits_p="O NF &ndash; Stressor \u00e9 desenvolvido pela NF Audio Tools. Este manual e o plugin descrevem a vers\u00e3o " + VERSION + "."
),
}


if __name__ == "__main__":
    for lang, fname in (("en", "NF_Stressor_Manual_EN.html"), ("pt", "NF_Stressor_Manual_PT.html")):
        html = build(lang)
        (HERE / fname).write_text(html, encoding="utf-8")
        print("wrote", fname)
