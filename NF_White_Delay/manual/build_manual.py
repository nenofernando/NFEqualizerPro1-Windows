#!/usr/bin/env python3
"""Builds the NF White Delay user manual (English + Portugues) as standalone
HTML files with the screenshots embedded as base64 data URIs, then renders
each to PDF with headless Chrome. Produces NF_White_Delay_Manual_EN.pdf and
NF_White_Delay_Manual_PT.pdf in this folder.
"""

import base64
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).parent


def b64(name):
    return base64.b64encode((HERE / name).read_bytes()).decode("ascii")


IMG_TOPBAR = b64("manual_topbar.png")
IMG_MAIN = b64("manual_main.png")
IMG_CONTROLS = b64("manual_controls.png")
IMG_BOTTOM = b64("manual_bottom.png")

CSS = """
@page { size: A4; margin: 20mm 18mm; }
* { box-sizing: border-box; }
body {
    font-family: -apple-system, 'Helvetica Neue', Arial, sans-serif;
    color: #1a1b20;
    line-height: 1.55;
    margin: 0;
    font-size: 14px;
}
.cover {
    text-align: center;
    padding: 70px 0 40px 0;
    page-break-after: always;
}
.cover .badge {
    width: 96px; height: 96px; margin: 0 auto 24px auto;
    border-radius: 22px; background: linear-gradient(180deg,#f4f5f8,#c8cad1);
    border: 2px solid #26282d; display: flex; align-items: center; justify-content: center;
    font-size: 40px; font-weight: 800; color: #1a1b20; font-family: -apple-system, Arial, sans-serif;
}
.cover h1 { font-size: 34px; margin: 24px 0 0 0; color: #1a1b20; letter-spacing: 1px; }
.cover h2 { font-size: 14px; margin: 8px 0 0 0; color: #5fa8c9; font-weight: 600; letter-spacing: 2px; text-transform: uppercase; }
.cover .meta { margin-top: 50px; font-size: 12px; color: #8a8a8a; }
h1.section { font-size: 22px; color: #1a1b20; border-bottom: 3px solid #2f8fe0; padding-bottom: 6px; margin: 40px 0 16px 0; page-break-before: always; }
h1.section:first-of-type { page-break-before: avoid; }
h2 { font-size: 16px; color: #1a1b20; margin: 26px 0 8px 0; }
p { margin: 0 0 12px 0; }
figure { margin: 18px 0; text-align: center; }
figure img { max-width: 100%; border-radius: 10px; border: 1px solid #ddd; }
figcaption { font-size: 11px; color: #888; margin-top: 6px; }
table { border-collapse: collapse; width: 100%; margin: 12px 0 20px 0; font-size: 12.5px; }
th, td { border: 1px solid #ddd; padding: 6px 10px; text-align: left; vertical-align: top; }
th { background: #2f8fe0; color: white; font-weight: 600; }
tr:nth-child(even) { background: #f2f7fb; }
ul { margin: 0 0 14px 0; padding-left: 20px; }
li { margin-bottom: 6px; }
.tip { background: #eaf5fc; border-left: 4px solid #2f8fe0; padding: 10px 14px; margin: 16px 0; border-radius: 0 6px 6px 0; }
.footer { margin-top: 50px; font-size: 11px; color: #999; text-align: center; }
code { background: #eef2f6; padding: 1px 5px; border-radius: 4px; font-size: 12px; }
"""

TEXT = {
    "en": {
        "html_lang": "en",
        "doc_title": "NF White Delay -- User Manual",
        "tagline": "Professional Delay",
        "meta": "Version 1.0.0 &middot; NF Audio Tools &mdash; By Nenno Fernando &middot; VST3 / AU / AAX",

        "s1_title": "1. Overview",
        "s1_body": """
<p>NF White Delay is a tempo-synced stereo delay built around a clean digital core with two
optional colour engines &mdash; <strong>Analog</strong> and <strong>Tape</strong> &mdash; plus
modulation, filtering and a musical ducking/character stage. It is designed to sit comfortably
on a vocal, a guitar bus or a full mix send: transparent when you need it clean, characterful
when you push the Character and Mode controls.</p>
<p>Its signal path is: <strong>Input &rarr; Delay line (Digital / Analog / Tape mode, Ping-Pong
optional) &rarr; Modulation (chorus-style pitch wobble on the repeats) &rarr; Filters (High
Pass / Low Pass / Resonance shaping the repeats) &rarr; Character &amp; Ducking &rarr; Dry/Wet
&rarr; Output</strong>.</p>
""",
        "fig1_caption": "Fig. 1 &mdash; Header: brand, Save, Menu (opens this manual), Bypass.",

        "s2_title": "2. Main Section",
        "fig2_caption": "Fig. 2 &mdash; TIME, live display, FEEDBACK, DRY/WET and OUTPUT.",
        "s2_table": """
<tr><th>Control</th><th>Description</th></tr>
<tr><td><strong>TIME</strong></td><td>Delay time in milliseconds (1&ndash;2000 ms). Dimmed and read-only when SYNC is on, since the division/modifier below take over.</td></tr>
<tr><td><strong>Display</strong></td><td>Shows the active division (or free-running ms), SYNC modifier (Straight/Dotted/Triplet), host BPM, and a live stereo activity meter of the wet signal (L/R).</td></tr>
<tr><td><strong>FEEDBACK</strong></td><td>How much of each repeat feeds back into the delay line -- higher values create longer, denser repeat trails.</td></tr>
<tr><td><strong>DRY / WET</strong></td><td>Balance between the untouched input and the delayed signal.</td></tr>
<tr><td><strong>OUTPUT</strong></td><td>Final output trim in dB, applied after the Dry/Wet mix.</td></tr>
""",

        "s3_title": "3. Sync &amp; Quick Controls",
        "fig3_caption": "Fig. 3 &mdash; SYNC, DIVISION, MODIFIER, PING PONG / LO-FI, DIGITAL / ANALOG / TAPE.",
        "s3_table": """
<tr><th>Control</th><th>Description</th></tr>
<tr><td><strong>SYNC</strong></td><td>Locks the delay time to the host tempo using DIVISION and MODIFIER instead of the free TIME knob.</td></tr>
<tr><td><strong>DIVISION</strong></td><td>Note value used when SYNC is on: 1/64 up to 2 Bars.</td></tr>
<tr><td><strong>MODIFIER</strong></td><td>Straight, Dotted or Triplet feel applied to the selected division.</td></tr>
<tr><td><strong>PING PONG</strong></td><td>Alternates repeats between the left and right channels instead of a centred delay.</td></tr>
<tr><td><strong>LO-FI</strong></td><td>Adds bit-crush/sample-rate style degradation to the repeats for a lower-fidelity, vintage character.</td></tr>
<tr><td><strong>DIGITAL / ANALOG / TAPE</strong></td><td>Delay engine character: Digital is the clean reference; Analog adds warmth and gentle non-linearity typical of BBD-style delays; Tape adds wow/flutter and saturation typical of tape echo units.</td></tr>
""",

        "s4_title": "4. Modulation, Filters &amp; Character",
        "fig4_caption": "Fig. 4 &mdash; The three lower modules: Modulation, Filters, Character.",
        "s4_mod_title": "Modulation",
        "s4_mod_table": """
<tr><th>Control</th><th>Description</th></tr>
<tr><td><strong>RATE</strong></td><td>Speed of the pitch-modulation LFO applied to the repeats, in Hz.</td></tr>
<tr><td><strong>DEPTH</strong></td><td>Amount of pitch modulation -- higher settings create a stronger chorus/vibrato effect on the repeats.</td></tr>
<tr><td><strong>SHAPE</strong></td><td>LFO waveform: Sine, Triangle, or Soft Random for a less predictable, tape-like wobble.</td></tr>
<tr><td><strong>SPREAD</strong></td><td>Stereo offset between the left and right modulation phase, widening the repeats.</td></tr>
""",
        "s4_filt_title": "Filters",
        "s4_filt_table": """
<tr><th>Control</th><th>Description</th></tr>
<tr><td><strong>HIGH PASS</strong></td><td>Removes low frequencies from the repeats, keeping the low end of the dry signal clean.</td></tr>
<tr><td><strong>LOW PASS</strong></td><td>Removes high frequencies from the repeats, for a darker, more distant trail.</td></tr>
<tr><td><strong>RESO</strong></td><td>Resonance/emphasis at the filter cutoffs, for a more pronounced, characterful roll-off.</td></tr>
""",
        "s4_char_title": "Character",
        "s4_char_table": """
<tr><th>Control</th><th>Description</th></tr>
<tr><td><strong>AMOUNT</strong></td><td>Overall intensity of the Analog/Tape colour stage (saturation, wobble, age) on the repeats.</td></tr>
<tr><td><strong>DUCKING</strong></td><td>Sidechains the repeats against the dry input, so the delay ducks under the source and comes forward in the gaps -- keeps busy passages readable.</td></tr>
""",
        "tip": "Tip: for a classic slap-back, turn SYNC off, set TIME between 80&ndash;140&nbsp;ms, keep FEEDBACK low, and use DIGITAL mode with Character at zero.",

        "s5_title": "5. Header &amp; Bypass",
        "s5_list": """
<li><strong>Save icon</strong> -- preset management (coming in a future update).</li>
<li><strong>Menu icon (three lines)</strong> -- opens this manual and the About box.</li>
<li><strong>BYPASS</strong> / power button -- both toggle the same true bypass; the delay line is frozen and the dry signal passes through untouched.</li>
""",
        "footer": "NF White Delay -- User Manual -- Version 1.0.0<br>NF Audio Tools &mdash; By Nenno Fernando. All rights reserved.",
    },
    "pt": {
        "html_lang": "pt-BR",
        "doc_title": "NF White Delay -- Manual do Usuario",
        "tagline": "Delay Profissional",
        "meta": "Versao 1.0.0 &middot; NF Audio Tools &mdash; By Nenno Fernando &middot; VST3 / AU / AAX",

        "s1_title": "1. Visao Geral",
        "s1_body": """
<p>NF White Delay e um delay estereo sincronizavel ao tempo do host, construido sobre um nucleo
digital limpo com dois motores de cor opcionais &mdash; <strong>Analog</strong> e
<strong>Tape</strong> &mdash; alem de modulacao, filtragem e um estagio musical de
character/ducking. Foi projetado para funcionar bem em um vocal, num bus de guitarra ou num
envio de mix inteiro: transparente quando voce precisa dele limpo, cheio de carater quando voce
usa os controles Character e Mode.</p>
<p>O caminho do sinal e: <strong>Entrada &rarr; Linha de delay (modo Digital / Analog / Tape,
Ping-Pong opcional) &rarr; Modulacao (oscilacao de pitch tipo chorus nas repeticoes) &rarr;
Filtros (High Pass / Low Pass / Resonance moldando as repeticoes) &rarr; Character &amp; Ducking
&rarr; Dry/Wet &rarr; Output</strong>.</p>
""",
        "fig1_caption": "Fig. 1 &mdash; Cabecalho: marca, Save, Menu (abre este manual), Bypass.",

        "s2_title": "2. Secao Principal",
        "fig2_caption": "Fig. 2 &mdash; TIME, display ao vivo, FEEDBACK, DRY/WET e OUTPUT.",
        "s2_table": """
<tr><th>Controle</th><th>Descricao</th></tr>
<tr><td><strong>TIME</strong></td><td>Tempo de delay em milissegundos (1&ndash;2000 ms). Fica esmaecido e somente leitura quando SYNC esta ligado, ja que a divisao/modificador abaixo assumem o controle.</td></tr>
<tr><td><strong>Display</strong></td><td>Mostra a divisao ativa (ou o tempo livre em ms), o modificador do SYNC (Straight/Dotted/Triplet), o BPM do host, e um medidor estereo ao vivo do sinal molhado (L/R).</td></tr>
<tr><td><strong>FEEDBACK</strong></td><td>Quanto de cada repeticao volta pra linha de delay -- valores mais altos criam trilhas de repeticao mais longas e densas.</td></tr>
<tr><td><strong>DRY / WET</strong></td><td>Balanco entre o sinal de entrada intacto e o sinal com delay.</td></tr>
<tr><td><strong>OUTPUT</strong></td><td>Ajuste final de saida em dB, aplicado depois da mistura Dry/Wet.</td></tr>
""",

        "s3_title": "3. Sync &amp; Controles Rapidos",
        "fig3_caption": "Fig. 3 &mdash; SYNC, DIVISION, MODIFIER, PING PONG / LO-FI, DIGITAL / ANALOG / TAPE.",
        "s3_table": """
<tr><th>Controle</th><th>Descricao</th></tr>
<tr><td><strong>SYNC</strong></td><td>Trava o tempo de delay no tempo do host usando DIVISION e MODIFIER em vez do knob livre TIME.</td></tr>
<tr><td><strong>DIVISION</strong></td><td>Figura ritmica usada quando SYNC esta ligado: de 1/64 ate 2 Compassos.</td></tr>
<tr><td><strong>MODIFIER</strong></td><td>Sensacao Straight, Dotted (pontuada) ou Triplet (tercina) aplicada a divisao selecionada.</td></tr>
<tr><td><strong>PING PONG</strong></td><td>Alterna as repeticoes entre os canais esquerdo e direito em vez de um delay centralizado.</td></tr>
<tr><td><strong>LO-FI</strong></td><td>Adiciona degradacao tipo bit-crush/sample-rate as repeticoes, para um carater vintage de menor fidelidade.</td></tr>
<tr><td><strong>DIGITAL / ANALOG / TAPE</strong></td><td>Carater do motor de delay: Digital e a referencia limpa; Analog adiciona calor e nao-linearidade suave, tipica de delays BBD; Tape adiciona wow/flutter e saturacao, tipicos de echos de fita.</td></tr>
""",

        "s4_title": "4. Modulacao, Filtros &amp; Character",
        "fig4_caption": "Fig. 4 &mdash; Os tres modulos inferiores: Modulation, Filters, Character.",
        "s4_mod_title": "Modulation",
        "s4_mod_table": """
<tr><th>Controle</th><th>Descricao</th></tr>
<tr><td><strong>RATE</strong></td><td>Velocidade do LFO de modulacao de pitch aplicado as repeticoes, em Hz.</td></tr>
<tr><td><strong>DEPTH</strong></td><td>Quantidade de modulacao de pitch -- valores mais altos criam um efeito de chorus/vibrato mais forte nas repeticoes.</td></tr>
<tr><td><strong>SHAPE</strong></td><td>Forma de onda do LFO: Sine, Triangle, ou Soft Random para uma oscilacao menos previsivel, tipo fita.</td></tr>
<tr><td><strong>SPREAD</strong></td><td>Deslocamento estereo entre a fase de modulacao esquerda e direita, alargando as repeticoes.</td></tr>
""",
        "s4_filt_title": "Filters",
        "s4_filt_table": """
<tr><th>Controle</th><th>Descricao</th></tr>
<tr><td><strong>HIGH PASS</strong></td><td>Remove frequencias baixas das repeticoes, mantendo o grave do sinal seco limpo.</td></tr>
<tr><td><strong>LOW PASS</strong></td><td>Remove frequencias altas das repeticoes, para uma trilha mais escura e distante.</td></tr>
<tr><td><strong>RESO</strong></td><td>Ressonancia/enfase nos cortes do filtro, para um roll-off mais pronunciado e cheio de carater.</td></tr>
""",
        "s4_char_title": "Character",
        "s4_char_table": """
<tr><th>Controle</th><th>Descricao</th></tr>
<tr><td><strong>AMOUNT</strong></td><td>Intensidade geral do estagio de cor Analog/Tape (saturacao, oscilacao, envelhecimento) nas repeticoes.</td></tr>
<tr><td><strong>DUCKING</strong></td><td>Aplica sidechain das repeticoes contra a entrada seca, entao o delay abaixa sob a fonte e volta nas pausas -- mantem passagens densas legiveis.</td></tr>
""",
        "tip": "Dica: para um slap-back classico, desligue o SYNC, ajuste o TIME entre 80&ndash;140&nbsp;ms, mantenha o FEEDBACK baixo, e use o modo DIGITAL com Character em zero.",

        "s5_title": "5. Cabecalho &amp; Bypass",
        "s5_list": """
<li><strong>Icone Save</strong> -- gerenciamento de presets (chegando em uma atualizacao futura).</li>
<li><strong>Icone Menu (tres tracos)</strong> -- abre este manual e a caixa Sobre.</li>
<li><strong>BYPASS</strong> / botao power -- ambos alternam o mesmo bypass real; a linha de delay e congelada e o sinal seco passa intacto.</li>
""",
        "footer": "NF White Delay -- Manual do Usuario -- Versao 1.0.0<br>NF Audio Tools &mdash; By Nenno Fernando. Todos os direitos reservados.",
    },
}


def render(lang):
    t = TEXT[lang]
    return f"""<!doctype html>
<html lang="{t['html_lang']}">
<head>
<meta charset="utf-8">
<title>{t['doc_title']}</title>
<style>{CSS}</style>
</head>
<body>

<div class="cover">
  <div class="badge">NF</div>
  <h1>NF White Delay</h1>
  <h2>{t['tagline']}</h2>
  <div class="meta">{t['meta']}</div>
</div>

<h1 class="section">{t['s1_title']}</h1>
{t['s1_body']}
<figure>
  <img src="data:image/png;base64,{IMG_TOPBAR}">
  <figcaption>{t['fig1_caption']}</figcaption>
</figure>

<h1 class="section">{t['s2_title']}</h1>
<figure>
  <img src="data:image/png;base64,{IMG_MAIN}">
  <figcaption>{t['fig2_caption']}</figcaption>
</figure>
<table>{t['s2_table']}</table>

<h1 class="section">{t['s3_title']}</h1>
<figure>
  <img src="data:image/png;base64,{IMG_CONTROLS}">
  <figcaption>{t['fig3_caption']}</figcaption>
</figure>
<table>{t['s3_table']}</table>

<h1 class="section">{t['s4_title']}</h1>
<figure>
  <img src="data:image/png;base64,{IMG_BOTTOM}">
  <figcaption>{t['fig4_caption']}</figcaption>
</figure>

<h2>{t['s4_mod_title']}</h2>
<table>{t['s4_mod_table']}</table>

<h2>{t['s4_filt_title']}</h2>
<table>{t['s4_filt_table']}</table>

<h2>{t['s4_char_title']}</h2>
<table>{t['s4_char_table']}</table>

<div class="tip">{t['tip']}</div>

<h1 class="section">{t['s5_title']}</h1>
<ul>{t['s5_list']}</ul>

<div class="footer">{t['footer']}</div>

</body>
</html>
"""


def render_pdf(html_path, pdf_path):
    chrome = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
    cmd = [
        chrome,
        "--headless",
        "--disable-gpu",
        f"--print-to-pdf={pdf_path}",
        "--no-pdf-header-footer",
        "--print-to-pdf-no-header",
        str(html_path),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        sys.exit(1)
    print(f"Wrote {pdf_path}")


def main():
    for lang, suffix in (("en", "EN"), ("pt", "PT")):
        html = render(lang)
        out_html = HERE / f"NF_White_Delay_Manual_{suffix}.html"
        out_html.write_text(html, encoding="utf-8")
        print(f"Wrote {out_html}")

        out_pdf = HERE / f"NF_White_Delay_Manual_{suffix}.pdf"
        render_pdf(out_html, out_pdf)


if __name__ == "__main__":
    main()
