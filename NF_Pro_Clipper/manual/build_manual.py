#!/usr/bin/env python3
"""Builds the bilingual (EN/PT) NF Pro Clipper manual as two standalone HTML
files with the screenshots embedded as base64 data URIs. Print each HTML file
to PDF (Cmd+P > Save as PDF) to produce the .pdf files the installers expect:
NF_Pro_Clipper_Manual_EN.pdf / NF_Pro_Clipper_Manual_PT.pdf, in this folder.
"""

import base64
import pathlib

HERE = pathlib.Path(__file__).parent


def b64(name):
    return base64.b64encode((HERE / name).read_bytes()).decode("ascii")


IMG_COVER = b64("cover_purple.png")
IMG_TOPBAR = b64("sec_topbar.png")
IMG_MAIN = b64("sec_main.png")
IMG_CONTROLS = b64("sec_controls.png")
IMG_BOTTOM = b64("sec_bottom.png")

CSS = """
@page { size: A4; margin: 20mm 18mm; }
* { box-sizing: border-box; }
body {
    font-family: -apple-system, 'Helvetica Neue', Arial, sans-serif;
    color: #221a2e;
    line-height: 1.55;
    margin: 0;
    font-size: 14px;
}
.cover {
    text-align: center;
    padding: 60px 0 40px 0;
    page-break-after: always;
}
.cover img { max-width: 260px; margin: 0 auto 24px auto; display: block; border-radius: 18px; box-shadow: 0 10px 40px rgba(74,46,115,0.35); }
.cover h1 { font-size: 34px; margin: 0; color: #4a2e73; letter-spacing: 1px; }
.cover h2 { font-size: 15px; margin: 6px 0 0 0; color: #7a5ea3; font-weight: 500; letter-spacing: 2px; text-transform: uppercase; }
.cover .meta { margin-top: 40px; font-size: 12px; color: #8a8a8a; }
h1.section { font-size: 22px; color: #4a2e73; border-bottom: 3px solid #4a2e73; padding-bottom: 6px; margin: 40px 0 16px 0; page-break-before: always; }
h1.section:first-of-type { page-break-before: avoid; }
h2 { font-size: 16px; color: #4a2e73; margin: 26px 0 8px 0; }
p { margin: 0 0 12px 0; }
figure { margin: 18px 0; text-align: center; }
figure img { max-width: 100%; border-radius: 10px; border: 1px solid #ddd; }
figcaption { font-size: 11px; color: #888; margin-top: 6px; }
table { border-collapse: collapse; width: 100%; margin: 12px 0 20px 0; font-size: 12.5px; }
th, td { border: 1px solid #ddd; padding: 6px 10px; text-align: left; vertical-align: top; }
th { background: #4a2e73; color: white; font-weight: 600; }
tr:nth-child(even) { background: #f6f3fa; }
ul { margin: 0 0 14px 0; padding-left: 20px; }
li { margin-bottom: 6px; }
.tip { background: #f3ecfb; border-left: 4px solid #4a2e73; padding: 10px 14px; margin: 16px 0; border-radius: 0 6px 6px 0; }
.footer { margin-top: 50px; font-size: 11px; color: #999; text-align: center; }
code { background: #f0eaf7; padding: 1px 5px; border-radius: 4px; font-size: 12px; }
"""

TEXT = {
    "en": {
        "html_lang": "en",
        "doc_title": "NF Pro Clipper — User Manual",
        "tagline": "TRANSPARENT PEAK CLIPPER",
        "meta": "Version 1.0.0 &middot; Neno Fernando Audio Tools &middot; VST3 / AU",

        "s1_title": "1. Overview",
        "s1_body": """
<p>NF Pro Clipper is a transparent peak clipper for mastering, mix bus and individual-track
loudness work. Instead of a hard brick-wall limiter, it removes overshoots with a
<strong>soft-knee clipping curve</strong> that you can shape directly, so the transition from
clean signal to clipped signal is gradual and musical rather than abrupt.</p>
<p>Its signal path is: <strong>Input trim &rarr; Drive &rarr; Clip stage (Ceiling + Knee + Clip
Mode, with optional oversampling) &rarr; Tone &rarr; Mix &rarr; Output trim</strong>. Because the
clip stage sits mid-chain, you can push the signal into it hard with Drive and then bring the
overall level back down with Output, all while watching exactly how much is being shaved off in
real time on the transfer graph and the Reduction meter.</p>
""",

        "s2_title": "2. The Top Bar",
        "s2_body": """
<p>The top of the plug-in carries the branding, the preset navigator and the skin switch.</p>
<ul>
<li><strong>NF logo</strong> &mdash; if you have resized the plug-in window larger than its
default size (by dragging the small resize handle in the bottom-right corner), clicking the
"NF" logo instantly snaps the window back to its default size.</li>
<li><strong>PRO CLIPPER title</strong> &mdash; click this title at any time to open this manual,
in English or Portuguese, as a PDF.</li>
<li><strong>&lt; / preset name / &gt;</strong> &mdash; step backward/forward through the saved
preset list. The current preset name is shown in the black display between the arrows.</li>
<li><strong>SKIN</strong> &mdash; toggles between the two colour skins (purple and black/yellow).
Only the colour palette changes; every control stays in the same place.</li>
</ul>
<figure><img src="data:image/png;base64,__IMG_TOPBAR__"><figcaption>Top bar: logo, title, preset
navigator and skin switch.</figcaption></figure>
""",

        "s3_title": "3. Meters, Graph and Main Gain Stages",
        "s3_body": """
<p>The centre of the plug-in shows you what is happening to the signal, and the three main
gain knobs that shape it.</p>
<ul>
<li><strong>IN / OUT meters</strong> &mdash; stereo peak meters for the signal arriving at the
plug-in and the signal leaving it.</li>
<li><strong>Transfer graph</strong> &mdash; plots output level against input level for the
current Ceiling/Knee/Clip Mode settings, so you can see the exact shape of the clipping curve
you have dialled in before it ever touches audio.</li>
<li><strong>INPUT</strong> &mdash; trims the signal before it reaches Drive and the clip stage.
Use this to compensate for a track that is already hot or quiet.</li>
<li><strong>DRIVE</strong> &mdash; pushes the signal harder into the clipper. More Drive means
more of the waveform crosses the Ceiling and gets clipped.</li>
<li><strong>OUTPUT</strong> &mdash; trims the final level after the Mix stage, to match the
loudness you had going in (gain-staging) or to hit a specific target level.</li>
</ul>
<figure><img src="data:image/png;base64,__IMG_MAIN__"><figcaption>IN/OUT meters, transfer
graph, and the Input / Drive / Output knobs.</figcaption></figure>
""",

        "s4_title": "4. Shaping the Clip",
        "s4_body": """
<p>These four knobs and three menus define exactly how the clipping curve behaves.</p>
<table>
<tr><th>Control</th><th>Range</th><th>Default</th><th>What it does</th></tr>
<tr><td>Ceiling</td><td>-12.0 to 0.0 dB</td><td>-0.3 dB</td><td>The level above which the
signal is clipped. Lower it to clip more of the signal.</td></tr>
<tr><td>Knee</td><td>0.0 to 12.0 dB</td><td>6.0 dB</td><td>How gradually the curve bends into
the ceiling. 0 dB is a hard, immediate clip; higher values soften the transition over a wider
range for a more transparent, analogue-like result.</td></tr>
<tr><td>Mix</td><td>0 to 100 %</td><td>100 %</td><td>Blends the clipped signal with the dry
(unclipped) signal in parallel &mdash; a form of parallel saturation. Lower values keep more of
the original transient detail.</td></tr>
<tr><td>Tone</td><td>-100 to 100 %</td><td>0 %</td><td>Tilts the tonal balance of the clipped
signal: negative values warm it up, positive values brighten it.</td></tr>
</table>
<p><strong>Clip Mode</strong> (Soft / Medium / Hard, default Soft) selects the character of the
clipping curve itself &mdash; Soft is the most transparent and rounded, Hard approaches a
brick-wall clip with the least headroom recovery.</p>
<p><strong>Oversampling</strong> (1x / 2x / 4x / 8x / 16x, default 4x) runs the clip stage at a
higher internal sample rate to reduce aliasing produced by the hard non-linearity of clipping.
Higher settings sound cleaner on high-frequency material at the cost of CPU.</p>
<p><strong>Monitor</strong> (IN / OUT / CLIP, default OUT) lets you listen to the input signal,
the output signal, or, in CLIP mode, only the difference removed by the clipper &mdash; the
fastest way to hear exactly what is being shaved off.</p>
<figure><img src="data:image/png;base64,__IMG_CONTROLS__"><figcaption>Ceiling, Knee, Mix, Tone
and the Clip Mode / Oversampling / Monitor menus.</figcaption></figure>
""",

        "s5_title": "5. Presets, Metering and Bypass",
        "s5_body": """
<ul>
<li><strong>SAVE</strong> &mdash; stores the current settings as a new preset.</li>
<li><strong>LOAD</strong> &mdash; opens a file picker to load a preset from disk.</li>
<li><strong>REDUCTION</strong> dots &mdash; light up to show how much gain is currently being
removed by the clipper.</li>
<li><strong>CLIPPING</strong> dots &mdash; light up when the signal is actively hitting the
Ceiling.</li>
<li><strong>BYPASS</strong> &mdash; disables clipping entirely so you can A/B against the dry
signal.</li>
</ul>
<figure><img src="data:image/png;base64,__IMG_BOTTOM__"><figcaption>Preset save/load, the
Reduction and Clipping meters, and Bypass.</figcaption></figure>
""",

        "s6_title": "6. Tips",
        "s6_body": """
<div class="tip">Start with Ceiling near 0 dB and Drive low. Raise Drive gradually while
watching the Reduction meter and the transfer graph &mdash; you want to see clipping happen,
not just guess at it.</div>
<div class="tip">If the top end feels harsh after heavy clipping, raise Oversampling before
reaching for Tone &mdash; a lot of perceived harshness is aliasing, not the clip curve itself.</div>
<div class="tip">Use Monitor &rarr; CLIP to solo exactly what the clipper is removing. If it
sounds like useful transient detail, back off Drive or open up the Knee.</div>
<div class="tip">The plug-in window can be resized freely from the bottom-right corner for a
larger view on high-resolution screens &mdash; click the "NF" logo any time to snap it back to
its default size.</div>
""",

        "footer": "NENO FERNANDO AUDIO TOOLS&reg; &mdash; All rights reserved.",
    },

    "pt": {
        "html_lang": "pt-BR",
        "doc_title": "NF Pro Clipper — Manual do Usuário",
        "tagline": "GRAMPEADOR DE PICO TRANSPARENTE",
        "meta": "Versão 1.0.0 &middot; Neno Fernando Audio Tools &middot; VST3 / AU",

        "s1_title": "1. Visão Geral",
        "s1_body": """
<p>O NF Pro Clipper é um grampeador de picos (clipper) transparente para masterização, bus de
mixagem e trabalho de loudness em faixas individuais. Em vez de um limitador brick-wall
agressivo, ele remove os picos que ultrapassam o teto usando uma <strong>curva de clipping com
"joelho" suave</strong> que você mesmo molda, tornando a transição do sinal limpo para o sinal
grampeado gradual e musical, em vez de abrupta.</p>
<p>O caminho do sinal é: <strong>trim de Entrada &rarr; Drive &rarr; estágio de clip (Ceiling +
Knee + Clip Mode, com oversampling opcional) &rarr; Tone &rarr; Mix &rarr; trim de Saída</strong>.
Como o estágio de clip fica no meio da cadeia, você pode empurrar o sinal com força usando o
Drive e depois trazer o nível geral de volta com o Output, enquanto observa exatamente quanto
está sendo cortado em tempo real no gráfico de transferência e no medidor de Reduction.</p>
""",

        "s2_title": "2. A Barra Superior",
        "s2_body": """
<p>A parte superior do plugin traz a marca, o navegador de presets e o botão de skin.</p>
<ul>
<li><strong>Logo NF</strong> &mdash; se você aumentou a janela do plugin além do seu tamanho
padrão (arrastando a alcinha de redimensionar no canto inferior direito), clicar no logo "NF"
devolve a janela ao tamanho padrão instantaneamente.</li>
<li><strong>Título PRO CLIPPER</strong> &mdash; clique neste título a qualquer momento para
abrir este manual, em inglês ou português, em PDF.</li>
<li><strong>&lt; / nome do preset / &gt;</strong> &mdash; navega para o preset anterior/seguinte
na lista salva. O nome do preset atual aparece no visor preto entre as setas.</li>
<li><strong>SKIN</strong> &mdash; alterna entre as duas skins de cor (roxa e preto/amarelo). Só
a paleta de cores muda; todos os controles permanecem no mesmo lugar.</li>
</ul>
<figure><img src="data:image/png;base64,__IMG_TOPBAR__"><figcaption>Barra superior: logo,
título, navegador de presets e botão de skin.</figcaption></figure>
""",

        "s3_title": "3. Medidores, Gráfico e Estágios de Ganho Principais",
        "s3_body": """
<p>O centro do plugin mostra o que está acontecendo com o sinal, e os três knobs de ganho
principais que o moldam.</p>
<ul>
<li><strong>Medidores IN / OUT</strong> &mdash; medidores de pico estéreo para o sinal que
chega ao plugin e o sinal que sai dele.</li>
<li><strong>Gráfico de transferência</strong> &mdash; plota o nível de saída contra o nível de
entrada para os ajustes atuais de Ceiling/Knee/Clip Mode, para você ver exatamente o formato da
curva de clipping antes mesmo de tocar o áudio.</li>
<li><strong>INPUT</strong> &mdash; ajusta o sinal antes que ele chegue ao Drive e ao estágio de
clip. Use para compensar uma faixa que já chega quente ou baixa demais.</li>
<li><strong>DRIVE</strong> &mdash; empurra o sinal com mais força para dentro do clipper. Mais
Drive significa mais forma de onda ultrapassando o Ceiling e sendo grampeada.</li>
<li><strong>OUTPUT</strong> &mdash; ajusta o nível final depois do estágio de Mix, para igualar
o loudness de entrada (gain-staging) ou atingir um nível-alvo específico.</li>
</ul>
<figure><img src="data:image/png;base64,__IMG_MAIN__"><figcaption>Medidores IN/OUT, gráfico de
transferência e os knobs de Input / Drive / Output.</figcaption></figure>
""",

        "s4_title": "4. Moldando o Clip",
        "s4_body": """
<p>Estes quatro knobs e três menus definem exatamente como a curva de clipping se comporta.</p>
<table>
<tr><th>Controle</th><th>Faixa</th><th>Padrão</th><th>O que faz</th></tr>
<tr><td>Ceiling</td><td>-12,0 a 0,0 dB</td><td>-0,3 dB</td><td>O nível acima do qual o sinal é
grampeado. Abaixe para grampear mais o sinal.</td></tr>
<tr><td>Knee</td><td>0,0 a 12,0 dB</td><td>6,0 dB</td><td>Quão gradual é a curva ao se aproximar
do teto. 0 dB é um clip duro e imediato; valores mais altos suavizam a transição por uma faixa
mais ampla, para um resultado mais transparente e analógico.</td></tr>
<tr><td>Mix</td><td>0 a 100 %</td><td>100 %</td><td>Mistura o sinal grampeado com o sinal seco
(sem clip) em paralelo &mdash; uma forma de saturação paralela. Valores mais baixos preservam
mais o detalhe transiente original.</td></tr>
<tr><td>Tone</td><td>-100 a 100 %</td><td>0 %</td><td>Inclina o equilíbrio tonal do sinal
grampeado: valores negativos esquentam, valores positivos clareiam.</td></tr>
</table>
<p><strong>Clip Mode</strong> (Soft / Medium / Hard, padrão Soft) seleciona o caráter da própria
curva de clipping &mdash; Soft é o mais transparente e arredondado, Hard se aproxima de um clip
brick-wall com a menor recuperação de headroom.</p>
<p><strong>Oversampling</strong> (1x / 2x / 4x / 8x / 16x, padrão 4x) roda o estágio de clip em
uma taxa de amostragem interna mais alta para reduzir o aliasing produzido pela não-linearidade
dura do clipping. Ajustes mais altos soam mais limpos em material com muito agudo, ao custo de
CPU.</p>
<p><strong>Monitor</strong> (IN / OUT / CLIP, padrão OUT) permite ouvir o sinal de entrada, o
sinal de saída ou, no modo CLIP, apenas a diferença removida pelo clipper &mdash; a forma mais
rápida de ouvir exatamente o que está sendo cortado.</p>
<figure><img src="data:image/png;base64,__IMG_CONTROLS__"><figcaption>Ceiling, Knee, Mix, Tone
e os menus de Clip Mode / Oversampling / Monitor.</figcaption></figure>
""",

        "s5_title": "5. Presets, Medição e Bypass",
        "s5_body": """
<ul>
<li><strong>SAVE</strong> &mdash; salva os ajustes atuais como um novo preset.</li>
<li><strong>LOAD</strong> &mdash; abre um seletor de arquivos para carregar um preset do
disco.</li>
<li><strong>Pontos de REDUCTION</strong> &mdash; acendem para mostrar quanto ganho está sendo
removido pelo clipper no momento.</li>
<li><strong>Pontos de CLIPPING</strong> &mdash; acendem quando o sinal está de fato atingindo o
Ceiling.</li>
<li><strong>BYPASS</strong> &mdash; desativa o clipping por completo, para comparar A/B com o
sinal seco.</li>
</ul>
<figure><img src="data:image/png;base64,__IMG_BOTTOM__"><figcaption>Salvar/carregar preset, os
medidores de Reduction e Clipping, e o Bypass.</figcaption></figure>
""",

        "s6_title": "6. Dicas",
        "s6_body": """
<div class="tip">Comece com o Ceiling perto de 0 dB e o Drive baixo. Suba o Drive aos poucos
observando o medidor de Reduction e o gráfico de transferência &mdash; você quer ver o clipping
acontecendo, não apenas adivinhar.</div>
<div class="tip">Se o agudo ficar áspero depois de um clipping pesado, suba o Oversampling antes
de mexer no Tone &mdash; boa parte da aspereza percebida é aliasing, não a curva de clip em
si.</div>
<div class="tip">Use Monitor &rarr; CLIP para isolar exatamente o que o clipper está removendo.
Se soar como detalhe transiente útil, reduza o Drive ou abra mais o Knee.</div>
<div class="tip">A janela do plugin pode ser redimensionada livremente pelo canto inferior
direito para uma visualização maior em telas de alta resolução &mdash; clique no logo "NF" a
qualquer momento para devolvê-la ao tamanho padrão.</div>
""",

        "footer": "NENO FERNANDO AUDIO TOOLS&reg; &mdash; Todos os direitos reservados.",
    },
}


def build(lang):
    t = TEXT[lang]

    body = f"""
<div class="cover">
  <img src="data:image/png;base64,{IMG_COVER}">
  <h1>NF PRO CLIPPER</h1>
  <h2>{t['tagline']}</h2>
  <div class="meta">{t['meta']}</div>
</div>

<h1 class="section">{t['s1_title']}</h1>
{t['s1_body']}

<h1 class="section">{t['s2_title']}</h1>
{t['s2_body']}

<h1 class="section">{t['s3_title']}</h1>
{t['s3_body']}

<h1 class="section">{t['s4_title']}</h1>
{t['s4_body']}

<h1 class="section">{t['s5_title']}</h1>
{t['s5_body']}

<h1 class="section">{t['s6_title']}</h1>
{t['s6_body']}

<div class="footer">{t['footer']}</div>
"""

    body = (
        body.replace("__IMG_TOPBAR__", IMG_TOPBAR)
            .replace("__IMG_MAIN__", IMG_MAIN)
            .replace("__IMG_CONTROLS__", IMG_CONTROLS)
            .replace("__IMG_BOTTOM__", IMG_BOTTOM)
    )

    return f"""<!doctype html>
<html lang="{t['html_lang']}">
<head>
<meta charset="utf-8">
<title>{t['doc_title']}</title>
<style>{CSS}</style>
</head>
<body>
{body}
</body>
</html>
"""


if __name__ == "__main__":
    (HERE / "NF_Pro_Clipper_Manual_EN.html").write_text(build("en"), encoding="utf-8")
    (HERE / "NF_Pro_Clipper_Manual_PT.html").write_text(build("pt"), encoding="utf-8")
    print("Wrote NF_Pro_Clipper_Manual_EN.html and NF_Pro_Clipper_Manual_PT.html")
    print("Open each in a browser and print to PDF (matching filename, .pdf) to finish.")
