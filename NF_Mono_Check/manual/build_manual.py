import base64
import pathlib

here = pathlib.Path(__file__).parent

def b64(name):
    return base64.b64encode((here / name).read_bytes()).decode("ascii")

IMG_MONO = b64("full_mono.png")
IMG_LEFT = b64("full_left.png")
IMG_RIGHT = b64("full_right.png")
IMG_STEREO = b64("full_stereo.png")
IMG_BUTTONS = b64("detail_buttons.png")
IMG_LOGO = b64("detail_logo.png")
IMG_METER = b64("detail_meter_left.png")
IMG_VECTORSCOPE = b64("detail_vectorscope.png")
IMG_RESIZE = b64("detail_resize.png")
IMG_FOOTER = b64("detail_footer.png")
IMG_ABOUT = b64("about_dialog.png")

VERSION = "1.0.0"

def fig(b64data, caption, width="260px"):
    return f'''<div class="fig"><img src="data:image/png;base64,{b64data}" style="max-width:{width}"><div class="fig-cap">{caption}</div></div>'''

CSS = """
  @page { size: A4; margin: 22mm 18mm 22mm 18mm; }
  * { box-sizing: border-box; }
  body {
    font-family: 'Helvetica Neue', Arial, sans-serif;
    color: #222833;
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
    box-shadow: 0 6px 24px rgba(0,0,0,0.35);
  }
  .cover h1 {
    font-size: 30pt;
    letter-spacing: 2px;
    color: #1c8fe0;
    margin: 26px 0 4px 0;
  }
  .cover .sub {
    font-size: 12pt;
    color: #556070;
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
    color: #9aa4b0;
  }
  h1.section {
    font-size: 17pt;
    color: #1c6fb5;
    border-bottom: 2px solid #33aaff;
    padding-bottom: 6px;
    margin-top: 0;
    page-break-before: always;
  }
  h2 {
    font-size: 12.5pt;
    color: #10537f;
    margin-top: 22px;
    margin-bottom: 6px;
  }
  h3 {
    font-size: 11pt;
    color: #2a2f38;
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
    border: 1px solid #cfd8e3;
    padding: 5px 8px;
    text-align: left;
    vertical-align: top;
  }
  th {
    background: #e4f0fb;
    color: #10537f;
  }
  tr:nth-child(even) td { background: #f5f9fd; }
  .toc { page-break-after: always; }
  .toc ol { font-size: 11pt; line-height: 2.0; }
  .toc a { color: #10537f; text-decoration: none; }
  .note {
    background: #eaf4fd;
    border-left: 3px solid #33aaff;
    padding: 8px 12px;
    margin: 10px 0;
    font-size: 9.7pt;
  }
  code {
    background: #eef1f5;
    padding: 1px 5px;
    border-radius: 3px;
    font-size: 9.5pt;
  }
  .fig {
    display: inline-block;
    text-align: center;
    background: #10131a;
    border-radius: 6px;
    padding: 8px;
    margin: 6px 10px 10px 0;
    vertical-align: top;
  }
  .fig img { display: block; border-radius: 3px; }
  .fig-cap {
    color: #9db3c9;
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

    fig_mono = fig(IMG_MONO, t["cap_mono"], "100%")
    fig_left = fig(IMG_LEFT, t["cap_left"], "100%")
    fig_right = fig(IMG_RIGHT, t["cap_right"], "100%")
    fig_stereo = fig(IMG_STEREO, t["cap_stereo"], "100%")
    fig_buttons = fig(IMG_BUTTONS, t["cap_buttons"], "100%")
    fig_logo = fig(IMG_LOGO, t["cap_logo"], "100%")
    fig_meter = fig(IMG_METER, t["cap_meter"], "220px")
    fig_vectorscope = fig(IMG_VECTORSCOPE, t["cap_vectorscope"], "100%")
    fig_resize = fig(IMG_RESIZE, t["cap_resize"], "160px")
    fig_footer = fig(IMG_FOOTER, t["cap_footer"], "100%")
    fig_about = fig(IMG_ABOUT, t["cap_about"], "260px")

    return f"""<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>NF Mono Check &mdash; {t['title_suffix']}</title>
<style>{CSS}</style>
</head>
<body>

<div class="cover">
  <img src="data:image/png;base64,{IMG_MONO}" alt="NF Mono Check">
  <h1>NF MONO CHECK</h1>
  <div class="sub">{t['tagline']}</div>
  <div class="ver">{t['version_line'].format(v=VERSION)}</div>
  <div class="rights">NENNO FERNANDO AUDIO TOOLS&reg; &mdash; {t['allrights']}</div>
</div>

<div class="toc">
  <h1 class="section" style="page-break-before: avoid;">{t['toc_h']}</h1>
  <ol>
    <li><a href="#overview">{t['toc_overview']}</a></li>
    <li><a href="#install">{t['toc_install']}</a></li>
    <li><a href="#interface">{t['toc_interface']}</a></li>
    <li><a href="#modes">{t['toc_modes']}</a></li>
    <li><a href="#correlation">{t['toc_correlation']}</a></li>
    <li><a href="#window">{t['toc_window']}</a></li>
    <li><a href="#automation">{t['toc_automation']}</a></li>
    <li><a href="#tips">{t['toc_tips']}</a></li>
    <li><a href="#credits">{t['toc_credits']}</a></li>
  </ol>
</div>

<h1 class="section" id="overview" style="page-break-before: avoid;">1. {t['overview_h']}</h1>
<p>{t['overview_p1']}</p>
<p>{t['overview_p2']}</p>
<div class="figrow">{fig_mono}</div>

<h1 class="section" id="install">2. {t['install_h']}</h1>
<h2>macOS</h2>
<ol>
  <li>{t['mac_step1']}</li>
  <li>{t['mac_step2']}</li>
  <li>{t['mac_step3']}
    <ul>
      <li><code>/Library/Audio/Plug-Ins/VST3/NF Mono Check.vst3</code></li>
      <li><code>/Library/Audio/Plug-Ins/Components/NF Mono Check.component</code> ({t['audio_unit']})</li>
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

<h1 class="section" id="interface">3. {t['interface_h']}</h1>
<p>{t['interface_p1']}</p>

<h2>{t['sec_logo_h']}</h2>
<div class="figrow">{fig_logo}</div>
<p>{t['logo_p']}</p>

<h2>{t['sec_buttons_h']}</h2>
<div class="figrow">{fig_buttons}</div>
<p>{t['buttons_p']}</p>

<h2>{t['sec_meter_h']}</h2>
<div class="figrow">{fig_meter}</div>
<p>{t['meter_p']}</p>

<h2>{t['sec_vectorscope_h']}</h2>
<div class="figrow">{fig_vectorscope}</div>
<p>{t['vectorscope_p']}</p>

<h2>{t['sec_about_h']}</h2>
<div class="figrow">{fig_about}</div>
<p>{t['about_p']}</p>

<h2>{t['sec_footer_h']}</h2>
<div class="figrow">{fig_footer}</div>
<p>{t['footer_p']}</p>

<h1 class="section" id="modes">4. {t['modes_h']}</h1>
<p>{t['modes_p1']}</p>
<div class="figrow">{fig_left}{fig_right}{fig_stereo}</div>
<table>
<tr><th>{t['th_mode']}</th><th>{t['th_output_left']}</th><th>{t['th_output_right']}</th><th>{t['th_description']}</th></tr>
<tr><td><b>L &mdash; LEFT</b></td><td>{t['m_left_l']}</td><td>{t['m_left_r']}</td><td>{t['m_left_desc']}</td></tr>
<tr><td><b>M &mdash; MONO</b></td><td>{t['m_mono']}</td><td>{t['m_mono']}</td><td>{t['m_mono_desc']}</td></tr>
<tr><td><b>R &mdash; RIGHT</b></td><td>{t['m_right_l']}</td><td>{t['m_right_r']}</td><td>{t['m_right_desc']}</td></tr>
<tr><td><b>{t['m_stereo_title']}</b></td><td>{t['m_stereo_l']}</td><td>{t['m_stereo_r']}</td><td>{t['m_stereo_desc']}</td></tr>
</table>
<div class="note">{t['modes_note']}</div>

<h1 class="section" id="correlation">5. {t['correlation_h']}</h1>
<p>{t['correlation_p1']}</p>
<table>
<tr><th>{t['th_value']}</th><th>{t['th_meaning']}</th></tr>
<tr><td>+1</td><td>{t['corr_plus1']}</td></tr>
<tr><td>0</td><td>{t['corr_zero']}</td></tr>
<tr><td>-1</td><td>{t['corr_minus1']}</td></tr>
</table>
<p>{t['correlation_p2']}</p>
<div class="note">{t['correlation_note']}</div>

<h1 class="section" id="window">6. {t['window_h']}</h1>
<p>{t['window_p1']}</p>
<div class="figrow">{fig_resize}</div>
<div class="note">{t['window_note']}</div>

<h1 class="section" id="automation">7. {t['automation_h']}</h1>
<p>{t['automation_p1']}</p>
<table>
<tr><th>{t['th_param']}</th><th>{t['th_values']}</th><th>{t['th_default']}</th></tr>
<tr><td>Monitor Mode</td><td>Left / Mono / Right / Stereo</td><td>Mono</td></tr>
</table>
<p>{t['automation_p2']}</p>

<h1 class="section" id="tips">8. {t['tips_h']}</h1>
<ul>
<li>{t['tip1']}</li>
<li>{t['tip2']}</li>
<li>{t['tip3']}</li>
<li>{t['tip4']}</li>
</ul>

<h1 class="section" id="credits">9. {t['credits_h']}</h1>
<p>{t['credits_p']}</p>
<table>
<tr><th>{t['th_format']}</th><th>{t['th_version']}</th><th>{t['th_platform']}</th></tr>
<tr><td>VST3</td><td>{VERSION}</td><td>macOS / Windows</td></tr>
<tr><td>{t['audio_unit']} (AU)</td><td>{VERSION}</td><td>macOS</td></tr>
<tr><td>Standalone</td><td>{VERSION}</td><td>macOS</td></tr>
</table>
<p style="margin-top:18px;color:#8a8a8a;font-size:9pt">NENNO FERNANDO AUDIO TOOLS&reg; &mdash; {t['allrights']}<br>{t['copyright_note']}</p>
<div class="footer-tag">NF Audio Tools &nbsp;&middot;&nbsp; NF Mono Check &nbsp;&middot;&nbsp; v{VERSION}</div>

</body>
</html>
"""


TEXT = {
"en": dict(
    title_suffix="User Manual",
    tagline="Mono / Stereo Compatibility Monitor &mdash; User Manual",
    version_line="Version {v} &nbsp;&middot;&nbsp; NF Audio Tools &nbsp;&middot;&nbsp; VST3 / AU / Standalone",
    allrights="All rights reserved.",
    toc_h="Contents",
    toc_overview="Overview",
    toc_install="Installation",
    toc_interface="Interface Guide",
    toc_modes="L / M / R / Stereo Modes",
    toc_correlation="Correlation Meter &amp; Goniometer",
    toc_window="Window &amp; Resizing",
    toc_automation="Automation &amp; Parameters",
    toc_tips="Usage Tips",
    toc_credits="Credits &amp; Version",
    overview_h="Overview",
    overview_p1="NF Mono Check is a monitoring utility for the master bus. It switches the way you listen to a stereo mix &mdash; Left only, Right only, true Mono, or unprocessed Stereo &mdash; without altering the signal being sent onward, so you can catch mono-compatibility problems, phase cancellation, polarity issues and stereo-width problems before they leave the studio.",
    overview_p2="A live correlation meter and a real-time goniometer (vectorscope), both driven from the original, unprocessed stereo signal, give you a constant visual read on how correlated your left and right channels are &mdash; regardless of which monitoring mode is currently selected.",
    cap_mono="Default view &mdash; Mono mode active",
    cap_left="Left mode active",
    cap_right="Right mode active",
    cap_stereo="Stereo mode active (M deselected, L and R lit together)",
    install_h="Installation",
    mac_step1="Open <code>NF Mono Check - Mac Installer.dmg</code>.",
    mac_step2="Double-click <code>NF Mono Check Installer.pkg</code> and follow the prompts.",
    mac_step3="The installer places the plugin in:",
    mac_step4="Rescan plugins in your DAW if it doesn't appear automatically.",
    win_step1="Run <code>NF Mono Check Installer.exe</code>.",
    win_step2="The installer places the VST3 in <code>C:\\Program Files\\Common Files\\VST3\\NF Mono Check.vst3</code>, and the PDF manuals (English + Portuguese) in <code>C:\\Program Files\\NF Audio Tools\\NF Mono Check\\Manual\\</code>.",
    win_step3="Rescan plugins in your DAW if it doesn't appear automatically.",
    install_note="An uninstaller is included on both platforms (macOS: re-run the package and choose remove where offered; Windows: \"Uninstall NF Mono Check\" from Control Panel / Apps). The bilingual PDF manual is installed by both installers &mdash; on macOS into <code>/Users/Shared/NF Audio Tools/NF Mono Check/Manual</code>.",
    audio_unit="Audio Unit",
    interface_h="Interface Guide",
    interface_p1="Every element on the panel is described below, in the order it appears from top to bottom.",
    sec_logo_h="Logo &amp; Title Bar",
    cap_logo="NF logo, title and status indicator",
    logo_p="The NF roundel at the top-left identifies the manufacturer. <b>Double-clicking it resets the plugin window to its default size</b> &mdash; handy after dragging the resize corner to make the window larger. The blue dot at the top-right is a status indicator; the ABOUT button opens a small dialog with the plugin name, version and manufacturer.",
    sec_buttons_h="L / M / R Mode Buttons",
    cap_buttons="The three mode buttons, with M active and its light bar lit",
    buttons_p="These three buttons select what you hear. A thin light bar beneath each button lights up blue when that button's mode is engaged, mirroring the button's own glow. See the full Modes chapter below for exactly what each one does to the signal.",
    sec_meter_h="Level Meters (L / R)",
    cap_meter="Left level meter, 0 to -60 dB",
    meter_p="The two vertical meters at the left and right edges of the panel always show the level of the <b>original, unprocessed</b> input signal &mdash; Input L on the left, Input R on the right &mdash; regardless of which monitoring mode is selected. This lets you keep an eye on the real mix levels even while auditioning Mono or an isolated channel.",
    sec_vectorscope_h="Goniometer (Vectorscope) &amp; Correlation Meter",
    cap_vectorscope="Goniometer circle and correlation bar, centred and idle",
    vectorscope_p="The circular display is a real-time goniometer, plotting the original stereo signal's Left/Right relationship as a scatter of points: a signal that collapses to a vertical line is highly mono-compatible, while a signal that spreads out horizontally has significant stereo width or phase difference between channels. The horizontal bar beneath it is the correlation meter &mdash; see the dedicated chapter below for how to read it.",
    sec_about_h="About Dialog",
    cap_about="ABOUT dialog",
    about_p="Click the ABOUT button in the top-right of the panel to see the plugin's name, version number and manufacturer at a glance.",
    sec_footer_h="Footer",
    cap_footer="Footer: manufacturer branding and version number",
    footer_p="The bottom-left corner shows the NF Audio Tools brand; the bottom-right corner shows the installed plugin version.",
    modes_h="L / M / R / Stereo Modes",
    modes_p1="NF Mono Check listens to the same original stereo input at all times for its meters and correlation display, but reroutes what's actually sent to your monitors depending on which mode is selected:",
    th_mode="Mode", th_output_left="Output L", th_output_right="Output R", th_description="Description",
    m_left_l="Input L", m_left_r="silent", m_left_desc="Isolates the left channel, hard left &mdash; the right side of your monitors goes silent so you hear only what's panned/recorded to the left.",
    m_mono="(L+R) &times; 0.5", m_mono_desc="Sums Left and Right into a single, true-mono signal, played identically on both outputs. This is the primary tool for checking mono compatibility: anything that disappears or thins out here has a phase or polarity problem.",
    m_right_l="silent", m_right_r="Input R", m_right_desc="Isolates the right channel, hard right &mdash; the left side of your monitors goes silent so you hear only what's panned/recorded to the right.",
    m_stereo_title="STEREO (M deselected)", m_stereo_l="Input L", m_stereo_r="Input R",
    m_stereo_desc="Unprocessed passthrough &mdash; the original stereo signal, untouched. Reached by clicking M a second time (toggling it off), or by double-clicking L or R while either is active.",
    modes_note="<b>Quick toggles:</b> click M again while it's already active to jump straight to Stereo (L and R light up together). Double-clicking an isolated L or R button also returns you to Stereo. Clicking L or R always selects that channel with a single click.",
    correlation_h="Correlation Meter &amp; Goniometer",
    correlation_p1="Both the correlation bar and the goniometer circle are driven by the <b>original, unprocessed stereo signal</b> &mdash; they keep reporting on your true stereo image even while you're listening in Mono, Left or Right monitoring mode.",
    th_value="Value", th_meaning="Meaning",
    corr_plus1="Fully correlated. Left and Right are identical or very similar &mdash; excellent mono compatibility.",
    corr_zero="Low correlation. Left and Right share little in common &mdash; typical of wide stereo content; not necessarily a problem, but worth checking in Mono.",
    corr_minus1="Out of phase. Left and Right substantially cancel each other &mdash; a real mono-compatibility problem that will lose energy (or disappear) when summed to mono.",
    correlation_p2="As a rule of thumb, a mix that stays mostly in positive territory (the marker sitting to the right of centre) is in good shape for mono playback systems, club systems, TV broadcast, and other environments that sum to mono. Frequent excursions into negative territory are worth investigating with the M button.",
    correlation_note="The goniometer is a live, real-time display &mdash; each dot is an actual audio sample, not a synthetic animation, so what you see always matches what's currently passing through the plugin's input.",
    window_h="Window &amp; Resizing",
    window_p1="The plugin window can be freely resized by dragging the small handle in the bottom-right corner, from a compact minimum up to a much larger size &mdash; the entire interface scales together, so nothing gets cropped or repositioned.",
    cap_resize="Resize handle, bottom-right corner",
    window_note="<b>Double-click the NF logo</b> in the top-left corner at any time to instantly snap the window back to its default size.",
    automation_h="Automation &amp; Parameters",
    automation_p1="NF Mono Check exposes a single automatable parameter to your DAW:",
    th_param="Parameter", th_values="Values", th_default="Default",
    automation_p2="This parameter is saved and recalled with your session, and can be automated like any other plugin parameter in your DAW &mdash; useful for, e.g., automatically dropping into Mono for a few bars during a mix review pass.",
    tips_h="Usage Tips",
    tip1="Check Mono compatibility early and often during a mix, not just at the end &mdash; it's much easier to fix a phase problem on one element than to hunt for it once the whole mix collapses in Mono.",
    tip2="Watch the correlation meter while adjusting stereo-widening tools, double-tracked parts, or heavily panned elements &mdash; sudden dips toward -1 usually point to the change you just made.",
    tip3="The goniometer's shape is a fast visual gut-check: a tall, narrow vertical shape is safe for mono; a wide, horizontal shape means real stereo-only content that will lose energy when summed.",
    tip4="Use the L and R isolate modes to confirm channel identity and catch accidental channel swaps, especially after routing or bouncing stems.",
    th_format="Format", th_version="Version", th_platform="Platform",
    credits_h="Credits &amp; Version",
    credits_p="NF Mono Check is developed by NF Audio Tools. This manual and the plugin describe version " + VERSION + ".",
    copyright_note="NF Mono Check, its interface design, and this manual are original works by NF Audio Tools. Unauthorized reproduction or redistribution is prohibited.",
),
"pt": dict(
    title_suffix="Manual do Usu\u00e1rio",
    tagline="Monitor de Compatibilidade Mono/Stereo &mdash; Manual do Usu\u00e1rio",
    version_line="Vers\u00e3o {v} &nbsp;&middot;&nbsp; NF Audio Tools &nbsp;&middot;&nbsp; VST3 / AU / Standalone",
    allrights="Todos os direitos reservados.",
    toc_h="\u00cdndice",
    toc_overview="Vis\u00e3o Geral",
    toc_install="Instala\u00e7\u00e3o",
    toc_interface="Guia da Interface",
    toc_modes="Modos L / M / R / Stereo",
    toc_correlation="Correlation Meter e Gonimetro",
    toc_window="Janela e Redimensionamento",
    toc_automation="Automa\u00e7\u00e3o e Par\u00e2metros",
    toc_tips="Dicas de Uso",
    toc_credits="Cr\u00e9ditos e Vers\u00e3o",
    overview_h="Vis\u00e3o Geral",
    overview_p1="O NF Mono Check \u00e9 uma ferramenta de monitoramento pro master bus. Ele muda a forma como voc\u00ea ouve uma mixagem stereo &mdash; s\u00f3 o Esquerdo, s\u00f3 o Direito, Mono de verdade, ou Stereo sem processamento &mdash; sem alterar o sinal que segue adiante, pra voc\u00ea pegar problemas de compatibilidade mono, cancelamento de fase, polaridade e largura stereo antes que saiam do est\u00fadio.",
    overview_p2="Um correlation meter ao vivo e um gonimetro (vectorscope) em tempo real, os dois alimentados pelo sinal stereo original sem processamento, te dão uma leitura visual constante de o quanto os canais esquerdo e direito est\u00e3o correlacionados &mdash; independente de qual modo de monitoramento est\u00e1 selecionado no momento.",
    cap_mono="Vis\u00e3o padr\u00e3o &mdash; modo Mono ativo",
    cap_left="Modo Left ativo",
    cap_right="Modo Right ativo",
    cap_stereo="Modo Stereo ativo (M desativado, L e R acesos juntos)",
    install_h="Instala\u00e7\u00e3o",
    mac_step1="Abra o <code>NF Mono Check - Mac Installer.dmg</code>.",
    mac_step2="D\u00ea duplo clique no <code>NF Mono Check Installer.pkg</code> e siga as instru\u00e7\u00f5es.",
    mac_step3="O instalador coloca o plugin em:",
    mac_step4="Refa\u00e7a a busca de plugins na sua DAW caso ele n\u00e3o apare\u00e7a automaticamente.",
    win_step1="Execute o <code>NF Mono Check Installer.exe</code>.",
    win_step2="O instalador coloca o VST3 em <code>C:\\Program Files\\Common Files\\VST3\\NF Mono Check.vst3</code>, e os manuais em PDF (Ingl\u00eas + Portugu\u00eas) em <code>C:\\Program Files\\NF Audio Tools\\NF Mono Check\\Manual\\</code>.",
    win_step3="Refa\u00e7a a busca de plugins na sua DAW caso ele n\u00e3o apare\u00e7a automaticamente.",
    install_note="Um desinstalador \u00e9 inclu\u00eddo nas duas plataformas (macOS: execute o pacote novamente e escolha remover quando oferecido; Windows: \u201cUninstall NF Mono Check\u201d no Painel de Controle / Apps). O manual bilingue em PDF \u00e9 instalado pelos dois instaladores &mdash; no macOS em <code>/Users/Shared/NF Audio Tools/NF Mono Check/Manual</code>.",
    audio_unit="Audio Unit",
    interface_h="Guia da Interface",
    interface_p1="Todo elemento do painel \u00e9 descrito abaixo, na ordem em que aparece de cima pra baixo.",
    sec_logo_h="Logo e Barra de T\u00edtulo",
    cap_logo="Logo NF, t\u00edtulo e indicador de status",
    logo_p="O emblema NF no canto superior esquerdo identifica o fabricante. <b>Dar um duplo clique nele reseta a janela do plugin pro tamanho padr\u00e3o</b> &mdash; \u00fatil depois de arrastar o canto de redimensionar pra deixar a janela maior. O ponto azul no canto superior direito \u00e9 um indicador de status; o bot\u00e3o ABOUT abre uma pequena caixa de di\u00e1logo com o nome do plugin, vers\u00e3o e fabricante.",
    sec_buttons_h="Botões L / M / R",
    cap_buttons="Os tr\u00eas bot\u00f5es de modo, com M ativo e sua barrinha de luz acesa",
    buttons_p="Esses tr\u00eas bot\u00f5es selecionam o que voc\u00ea ouve. Uma barrinha de luz fina embaixo de cada bot\u00e3o acende em azul quando o modo daquele bot\u00e3o est\u00e1 ativo, espelhando o brilho do pr\u00f3prio bot\u00e3o. Veja o cap\u00edtulo completo de Modos abaixo pra saber exatamente o que cada um faz com o sinal.",
    sec_meter_h="Meters de N\u00edvel (L / R)",
    cap_meter="Meter de n\u00edvel esquerdo, 0 a -60 dB",
    meter_p="Os dois meters verticais nas bordas esquerda e direita do painel sempre mostram o n\u00edvel do sinal de entrada <b>original, sem processamento</b> &mdash; Input L \u00e0 esquerda, Input R \u00e0 direita &mdash; independente de qual modo de monitoramento est\u00e1 selecionado. Isso permite acompanhar os n\u00edveis reais da mixagem mesmo enquanto voc\u00ea audiciona o Mono ou um canal isolado.",
    sec_vectorscope_h="Gonimetro (Vectorscope) e Correlation Meter",
    cap_vectorscope="C\u00edrculo do gonimetro e barra de correla\u00e7\u00e3o, centralizados e parados",
    vectorscope_p="O display circular \u00e9 um gonimetro em tempo real, plotando a rela\u00e7\u00e3o Esquerdo/Direito do sinal stereo original como uma nuvem de pontos: um sinal que colapsa numa linha vertical tem alta compatibilidade mono, enquanto um sinal que se espalha horizontalmente tem largura stereo significativa ou diferen\u00e7a de fase entre os canais. A barra horizontal abaixo dele \u00e9 o correlation meter &mdash; veja o cap\u00edtulo dedicado abaixo pra saber como ler.",
    sec_about_h="Caixa de Di\u00e1logo About",
    cap_about="Caixa de di\u00e1logo ABOUT",
    about_p="Clique no bot\u00e3o ABOUT no canto superior direito do painel pra ver o nome do plugin, n\u00famero da vers\u00e3o e fabricante rapidamente.",
    sec_footer_h="Rodap\u00e9",
    cap_footer="Rodap\u00e9: marca do fabricante e n\u00famero da vers\u00e3o",
    footer_p="O canto inferior esquerdo mostra a marca NF Audio Tools; o canto inferior direito mostra a vers\u00e3o instalada do plugin.",
    modes_h="Modos L / M / R / Stereo",
    modes_p1="O NF Mono Check sempre escuta o mesmo sinal stereo original de entrada pros seus meters e display de correla\u00e7\u00e3o, mas redireciona o que \u00e9 de fato enviado pros seus monitores dependendo de qual modo est\u00e1 selecionado:",
    th_mode="Modo", th_output_left="Sa\u00edda L", th_output_right="Sa\u00edda R", th_description="Descri\u00e7\u00e3o",
    m_left_l="Input L", m_left_r="silencioso", m_left_desc="Isola o canal esquerdo, hard left &mdash; o lado direito dos seus monitores fica em sil\u00eancio, ent\u00e3o voc\u00ea s\u00f3 ouve o que foi panejado/gravado \u00e0 esquerda.",
    m_mono="(L+R) &times; 0.5", m_mono_desc="Soma Esquerdo e Direito num \u00fanico sinal mono de verdade, tocado de forma id\u00eantica nas duas sa\u00eddas. Essa \u00e9 a ferramenta principal pra checar compatibilidade mono: qualquer coisa que desapare\u00e7a ou fique mais fina aqui tem um problema de fase ou polaridade.",
    m_right_l="silencioso", m_right_r="Input R", m_right_desc="Isola o canal direito, hard right &mdash; o lado esquerdo dos seus monitores fica em sil\u00eancio, ent\u00e3o voc\u00ea s\u00f3 ouve o que foi panejado/gravado \u00e0 direita.",
    m_stereo_title="STEREO (M desativado)", m_stereo_l="Input L", m_stereo_r="Input R",
    m_stereo_desc="Passagem direta sem processamento &mdash; o sinal stereo original, intocado. Alcan\u00e7ado clicando no M uma segunda vez (desativando), ou dando duplo clique no L ou no R enquanto um deles est\u00e1 ativo.",
    modes_note="<b>Atalhos r\u00e1pidos:</b> clique no M de novo enquanto ele j\u00e1 est\u00e1 ativo pra ir direto pro Stereo (L e R acendem juntos). Dar duplo clique num bot\u00e3o L ou R isolado tamb\u00e9m te leva de volta pro Stereo. Clicar em L ou R sempre seleciona aquele canal com um \u00fanico clique.",
    correlation_h="Correlation Meter e Gonimetro",
    correlation_p1="Tanto a barra de correla\u00e7\u00e3o quanto o c\u00edrculo do gonimetro s\u00e3o alimentados pelo <b>sinal stereo original, sem processamento</b> &mdash; eles continuam reportando sua imagem stereo real mesmo enquanto voc\u00ea est\u00e1 ouvindo no modo Mono, Left ou Right.",
    th_value="Valor", th_meaning="Significado",
    corr_plus1="Totalmente correlacionado. Esquerdo e Direito s\u00e3o id\u00eanticos ou muito parecidos &mdash; excelente compatibilidade mono.",
    corr_zero="Baixa correla\u00e7\u00e3o. Esquerdo e Direito t\u00eam pouco em comum &mdash; t\u00edpico de conte\u00fado stereo largo; n\u00e3o \u00e9 necessariamente um problema, mas vale checar no Mono.",
    corr_minus1="Fora de fase. Esquerdo e Direito se cancelam substancialmente &mdash; um problema real de compatibilidade mono que vai perder energia (ou desaparecer) quando somado pro mono.",
    correlation_p2="Como regra geral, uma mixagem que fica majoritariamente em territ\u00f3rio positivo (o marcador ficando \u00e0 direita do centro) est\u00e1 em boa forma pra sistemas de reprodu\u00e7\u00e3o mono, sistemas de clube, transmiss\u00e3o de TV, e outros ambientes que somam pro mono. Idas frequentes pro territ\u00f3rio negativo valem a pena investigar com o bot\u00e3o M.",
    correlation_note="O gonimetro \u00e9 um display ao vivo, em tempo real &mdash; cada ponto \u00e9 uma amostra de \u00e1udio real, n\u00e3o uma anima\u00e7\u00e3o sint\u00e9tica, ent\u00e3o o que voc\u00ea v\u00ea sempre corresponde ao que est\u00e1 passando pela entrada do plugin naquele momento.",
    window_h="Janela e Redimensionamento",
    window_p1="A janela do plugin pode ser redimensionada livremente arrastando a pequena al\u00e7a no canto inferior direito, de um tamanho m\u00ednimo compacto at\u00e9 um tamanho bem maior &mdash; toda a interface escala junto, ent\u00e3o nada fica cortado ou reposicionado.",
    cap_resize="Al\u00e7a de redimensionar, canto inferior direito",
    window_note="<b>D\u00ea um duplo clique no logo NF</b> no canto superior esquerdo a qualquer momento pra a janela voltar instantaneamente ao tamanho padr\u00e3o.",
    automation_h="Automa\u00e7\u00e3o e Par\u00e2metros",
    automation_p1="O NF Mono Check exp\u00f5e um \u00fanico par\u00e2metro automatiz\u00e1vel pra sua DAW:",
    th_param="Par\u00e2metro", th_values="Valores", th_default="Padr\u00e3o",
    automation_p2="Esse par\u00e2metro \u00e9 salvo e recuperado junto com sua sess\u00e3o, e pode ser automatizado como qualquer outro par\u00e2metro de plugin na sua DAW &mdash; \u00fatil, por exemplo, pra cair automaticamente no Mono por alguns compassos durante uma passada de revis\u00e3o de mixagem.",
    tips_h="Dicas de Uso",
    tip1="Cheque a compatibilidade mono cedo e sempre durante a mixagem, n\u00e3o s\u00f3 no final &mdash; \u00e9 bem mais f\u00e1cil corrigir um problema de fase num elemento s\u00f3 do que ca\u00e7ar ele depois que a mixagem inteira colapsa no Mono.",
    tip2="Observe o correlation meter enquanto ajusta ferramentas de alargamento stereo, partes dubladas (double-tracked), ou elementos muito panejados &mdash; quedas repentinas em dire\u00e7\u00e3o ao -1 geralmente apontam pra mudan\u00e7a que voc\u00ea acabou de fazer.",
    tip3="O formato do gonimetro \u00e9 uma checagem visual r\u00e1pida: um formato vertical, alto e estreito \u00e9 seguro pro mono; um formato largo e horizontal significa conte\u00fado real s\u00f3-stereo que vai perder energia quando somado.",
    tip4="Use os modos de isolamento L e R pra confirmar a identidade dos canais e pegar trocas acidentais de canal, especialmente depois de roteamento ou bounce de stems.",
    th_format="Formato", th_version="Vers\u00e3o", th_platform="Plataforma",
    credits_h="Cr\u00e9ditos e Vers\u00e3o",
    credits_p="O NF Mono Check \u00e9 desenvolvido pela NF Audio Tools. Este manual e o plugin descrevem a vers\u00e3o " + VERSION + ".",
    copyright_note="O NF Mono Check, o design da sua interface, e este manual s\u00e3o obras originais da NF Audio Tools. Reprodu\u00e7\u00e3o ou redistribui\u00e7\u00e3o n\u00e3o autorizada \u00e9 proibida.",
),
}


if __name__ == "__main__":
    for lang, fname in (("en", "NF_Mono_Check_Manual_EN.html"), ("pt", "NF_Mono_Check_Manual_PT.html")):
        html = build(lang)
        (here / fname).write_text(html, encoding="utf-8")
        print("wrote", fname, len(html), "bytes")
