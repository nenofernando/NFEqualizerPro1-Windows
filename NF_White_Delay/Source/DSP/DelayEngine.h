#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "SVFFilter.h"
#include "ModulationEngine.h"
#include "DuckingProcessor.h"
#include "LoFiProcessor.h"
#include "CharacterProcessor.h"

namespace NF
{
// ============================================================
// FASE 2 + FASE 3 -- delay estéreo com feedback, ping-pong e
// transição profissional de tempo (dual-read-head + crossfade).
//
// Escopo desta fase, de propósito: ainda SEM filtros, modulação LFO,
// lo-fi, ducking ou character no caminho de feedback (isso é fase
// 4/5). process() continua devolvendo o sinal 100% wet -- dry/wet é
// fase 6.
//
// ARQUITETURA DE TEMPO (por que duas DelayLine inteiras, não uma só
// com leitura multi-tap):
//
// juce::dsp::DelayLine suporta leitura multi-tap via
// popSample(channel, delayInSamples, updateReadPointer=false), mas o
// índice de leitura interno (readPos) só avança quando
// updateReadPointer=true -- ou seja, EXATAMENTE UMA das leituras por
// amostra pode avançar o ponteiro, e a outra ficaria presa num offset
// fixo do buffer físico (não "N amostras atrás da escrita atual").
// Sincronizar isso à mão é fácil de errar sutilmente. Em vez disso,
// mantemos DUAS instâncias de DelayLine (A e B) que recebem
// exatamente a mesma amostra de entrada (dry + feedback) a cada
// pushSample -- as duas têm o histórico idêntico, cada uma com seu
// próprio readPos avançando corretamente via popSample(channel)
// (default updateReadPointer=true). A única diferença entre elas é o
// tempo de delay configurado em cada uma. Isso dá duas cabeças de
// leitura de verdade, de forma robusta, ao custo de memória
// desprezível (a mais é só o buffer circular duplicado, alocado uma
// vez em prepare()).
//
// Mudança PEQUENA de tempo (automação contínua, ajuste de knob):
// só re-alveja a rampa suave (SmoothedValue) da cabeça atualmente
// ativa -- comportamento idêntico à FASE 2.
//
// Mudança GRANDE de tempo (ex.: divisão sync 1/16 -> 1/2, ou um salto
// grande no modo livre): a cabeça INATIVA é ajustada instantaneamente
// pro novo tempo (ela está muda, então o salto não é ouvido), e um
// crossfade curto (~25ms) troca o peso da cabeça ativa antiga pra
// essa nova cabeça. Ao fim do crossfade, os papéis trocam. Isso evita
// o "pitch glide" que uma rampa sozinha produziria num salto grande
// -- física de qualquer delay de tempo variável com leitura contínua.
//
// CAPACIDADE: o parâmetro MANUAL (FREE) vai de 1 a maxFreeDelayMs
// (4000ms) -- mas o SYNC pode pedir bem mais que isso (1 Bar/2 Bars
// com Dotted, em BPM baixo -- ex.: 60 BPM/4-4/2 Bars Dotted = 12s).
// Por isso o motor internamente comporta até maxInternalDelayMs
// (16000ms): setDelayTimeMs() clampa contra a capacidade real, não
// contra o limite do knob manual, então um tempo de sync válido nunca
// é cortado silenciosamente pra 4s.
//
// PENDÊNCIA CONHECIDA (registrada a pedido, não corrigida agora --
// não reabre a FASE 3): maxInternalDelayMs=16000ms cobre 60 BPM/4-4/
// 2 Bars Dotted (12000ms) com folga, mas NÃO cobre qualquer BPM
// arbitrariamente baixo. Pior caso pra uma dada assinatura de tempo é
// sempre "2 Bars Dotted": quartersPerBar * 2 * 1.5 * quarterNoteMs.
// Isolando o BPM mínimo que ainda cabe em maxInternalDelayMs:
//
//     bpmMinimo = quartersPerBar * 3 * 60000 / maxInternalDelayMs
//               = quartersPerBar * 11.25
//
// Em 4/4 (quartersPerBar=4): bpmMinimo = 45 BPM -- abaixo disso (ex.:
// o 40 BPM/4-4/2 Bars Dotted = 18000ms do exemplo) o valor calculado
// excede a capacidade e setDelayTimeMs() clampa em 16000ms (clamp
// SEGURO -- sem crash, sem NaN, só não é o tempo audível "correto"
// pedido). Assinaturas com mais quartersPerBar (5/4, 7/4, 9/8...)
// precisam de BPM mínimo proporcionalmente maior.
//
// Solução proposta pra depois (não implementada agora, por decisão
// explícita de não reabrir a FASE 3 neste momento): em vez de uma
// capacidade fixa dimensionada pro pior caso absoluto, redimensionar
// as DelayLine sob demanda -- PluginProcessor recalcularia o maior
// tempo de sync já visto (a cada troca de BPM/assinatura/divisão do
// host) e chamaria um novo DelayEngine::ensureCapacityMs(ms) (que só
// re-chama setMaximumDelayInSamples(), operação de não-audio-thread,
// só quando o pedido realmente excede a capacidade atual) em vez de
// sempre alocar os 16s completos de antemão. Mais eficiente em
// memória pro caso comum (BPMs normais raramente pedem mais que
// poucos segundos) e ainda cobre qualquer BPM baixo sem limite fixo.
// ============================================================

class DelayEngine
{
public:
    // Explícito porque JUCE_DECLARE_NON_COPYABLE (abaixo) deleta o
    // construtor de cópia, e isso por si só suprime a geração
    // implícita do construtor padrão.
    DelayEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    // FASE 6.6 -- só para a comparação empírica de tempos de resposta
    // (Tests/MixLawAndResponseTests.cpp), que testou 5/8/10/15/25ms de
    // crossfade e escolheu o menor valor que continuou sem clique
    // audível. NÃO é chamado pelo PluginProcessor -- os valores de
    // produção já vêm fixos em prepare() (ver .cpp), escolhidos a
    // partir desse mesmo teste. Precisa ser chamado logo após
    // prepare(), antes de qualquer setDelayTimeMs() (os SmoothedValue
    // ainda não têm valor real pra preservar nesse ponto).
    void setDelayTimingForTesting(double smoothingSeconds, double crossfadeSeconds) noexcept;

    // Alvo em milissegundos. Decide sozinho (comparando com o alvo
    // atual da cabeça ativa) se é uma mudança pequena (rampa simples)
    // ou grande (dispara o crossfade de duas cabeças) -- ver nota no
    // topo do arquivo. Chamar uma vez por bloco, antes de process().
    void setDelayTimeMs(float targetMs) noexcept;

    // 0-0.95 (95% -- ver Seção 1 do briefing). Sempre suavizado.
    void setFeedback(float feedback0to1) noexcept;

    // Quando true: saída atrasada L alimenta o feedback de R e
    // vice-versa (cross-feedback DENTRO do loop, não um swap da
    // saída final -- ver process()).
    void setPingPong(bool enabled) noexcept;

    // ========================================================
    // FILTROS (FASE 4) -- Delay output -> HPF -> LPF -> Feedback.
    // Cada repetição fica progressivamente mais filtrada (o sinal
    // filtrado é o que recircula). Chamar qualquer um destes ativa o
    // estágio de filtros (antes disso, process() se comporta
    // byte-a-byte como na FASE 2/3 -- ver "ATIVAÇÃO PREGUIÇOSA" no
    // .cpp). Cutoff/resonance sempre suavizados (uma vez por bloco).
    // ========================================================

    void setHighPassHz(float hz) noexcept;
    void setLowPassHz(float hz) noexcept;

    // 0-1 -- mapeado internamente pra Q (0.5 a maxResonanceQ) e
    // compensado em ganho (ver SVFFilter.h) -- 100% nunca gera
    // runaway mesmo com feedback em 95%.
    void setResonance(float resonance0to1) noexcept;

    static constexpr float minFilterHz = 20.0f;
    static constexpr float maxFilterHz = 20000.0f;
    static constexpr float minResonanceQ = 0.5f;
    static constexpr float maxResonanceQ = 20.0f;

    // ========================================================
    // MODULAÇÃO (FASE 4) -- ver ModulationEngine.h pro LFO em si.
    // Chamar qualquer um destes ativa a modulação (mesma ativação
    // preguiçosa dos filtros). O offset do LFO se soma ao delay BASE
    // já suavizado (baseDelayMs + modulationOffsetMs = finalReadDelayMs,
    // sempre clampado a [minDelayMs, maxInternalDelayMs]) -- nunca
    // passa por setDelayTimeMs()/pela lógica de crossfade de duas
    // cabeças, então o LFO jamais dispara uma troca de cabeça A/B.
    // ========================================================

    void setModRate(float hz) noexcept;
    void setModDepth(float depth0to1) noexcept;
    void setModShape(ModShape shape) noexcept;
    void setModSpread(float spread0to1) noexcept;

    // ========================================================
    // DUCKING (FASE 5) -- Seção 1/2 do briefing: fica FORA do loop de
    // feedback por construção. O detector lê o pico DRY (capturado no
    // topo de cada amostra em process(), antes de qualquer
    // processamento) -- nunca a saída do delay. O ganho resultante só
    // multiplica a apresentação final do wet; o que recircula pro
    // feedback é sempre o sinal SEM duck. Ver DuckingProcessor.h pro
    // envelope follower e o mapeamento de amount.
    // ========================================================

    void setDuckingAmount(float amount0to1) noexcept;

    // ========================================================
    // LO-FI (FASE 5, reescrito na FASE 6.6) -- Delay output -> Filters
    // -> LO-FI -> Character -> Feedback (dentro do loop, cada repeat
    // degrada progressivamente). Botão simples ON/OFF -- ver
    // LoFiProcessor.h pra cadeia interna (bandwidth reduction +
    // sample-and-hold + saturação suave, SEM dither/quantização --
    // removidos por gerarem ruído audível; sem eles não há mais risco
    // de limit cycle porque não sobrou nenhum estágio não-linear
    // discreto no loop).
    // ========================================================

    void setLoFiEnabled(bool enabled) noexcept;

    // ========================================================
    // CHARACTER (FASE 5) -- Digital/Analog/Tape + Amount. Fica depois
    // do Lo-Fi, ainda dentro do loop de feedback -- ver
    // CharacterProcessor.h pra prova de small-signal gain == 1 (é
    // isso que garante que o loop continua estável com Character no
    // máximo, mesmo em 95% de feedback).
    //
    // MAKEUP GAIN (FASE 6): a contração de tanh(k*x)/k reduz o nível
    // (mais em Analog/Tape que Digital -- ver CharacterProcessor::
    // wetMakeupGainForMode). A compensação conservadora acontece SÓ
    // depois que o sinal já foi bifurcado pro feedback, em process() --
    // aplicada exclusivamente na amostra de saída, nunca no que é
    // empurrado de volta pra dentro da delay line. O loop de feedback
    // nunca vê esse ganho -- a prova de estabilidade da FASE 5
    // continua idêntica.
    // ========================================================

    void setCharacterMode(CharacterMode mode) noexcept;
    void setCharacterAmount(float amount0to1) noexcept;

    // Substitui o conteúdo do buffer pelo sinal atrasado com feedback,
    // filtros, modulação, lo-fi, character e ducking (100% wet -- ver
    // nota de escopo no topo do arquivo). Realtime-safe: nenhuma
    // alocação, sem I/O, sem locks.
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    static constexpr float minDelayMs = 1.0f;

    // Limite do parâmetro MANUAL (FREE) -- já é o range da própria
    // APVTS ("delayTimeMs", 1-4000, ver PluginProcessor.cpp), então o
    // host/UI nunca consegue nem pedir mais que isso no modo livre.
    // DelayEngine não usa essa constante pra clampar nada sozinho --
    // ela existe aqui só como referência/documentação pro resto do
    // código (e pra FASE 8, UI).
    static constexpr float maxFreeDelayMs = 4000.0f;

    // Capacidade REAL do motor -- maior que o range manual de propósito,
    // porque o SYNC (1 Bar/2 Bars + Dotted, em BPMs baixos) pode pedir
    // tempos bem acima de 4s (ex.: 60 BPM/4-4/2 Bars Dotted = 12000ms).
    // setDelayTimeMs() clampa contra ISTO, não contra maxFreeDelayMs --
    // um tempo de sync válido nunca é silenciosamente cortado pra 4s.
    static constexpr float maxInternalDelayMs = 16000.0f;

    static constexpr float maxFeedback = 0.95f;

    // Acima disso (25ms de diferença no alvo), setDelayTimeMs() trata
    // como salto grande e dispara o crossfade de duas cabeças em vez
    // de só mover a rampa simples.
    static constexpr float largeJumpThresholdMs = 25.0f;

private:
    using DelayLineType = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>;

    DelayLineType delayLineA, delayLineB;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delayMsSmoothA, delayMsSmoothB;

    // 0 = A totalmente audível, 1 = B totalmente audível. Só avança
    // enquanto crossfading == true; parado (0 ou 1) em repouso.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> crossfadeToB;
    bool restingHeadIsB = false;
    bool crossfading = false;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedbackSmooth;
    bool feedbackInitialised = false;
    bool pingPongEnabled = false;

    // Filtros -- HPF/LPF por canal, coeficientes recalculados uma vez
    // por bloco a partir dos valores já suavizados (recomputar TPT
    // coefs amostra a amostra seria caro à toa, mesmo padrão já usado
    // no resto do código pra EQ/tone). filtersActive só liga na
    // primeira chamada de qualquer setter de filtro -- até lá,
    // process() pula o estágio inteiro (comportamento idêntico à
    // FASE 2/3, byte a byte, o que é o que garante que os testes
    // antigos continuam passando sem tocar neles).
    SVFFilter hpfL, hpfR, lpfL, lpfR;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> hpCutoffSmooth, lpCutoffSmooth, resonanceSmooth;
    bool filterParamsInitialised = false;
    bool filtersActive = false;

    // Modulação -- mesma ativação preguiçosa que os filtros.
    ModulationEngine modulation;
    bool modulationActive = false;

    // Ducking -- ativação preguiçosa igual às outras seções.
    DuckingProcessor ducking;
    bool duckingActive = false;

    // Lo-Fi -- botão simples, sem parâmetro contínuo (ver LoFiProcessor.h).
    LoFiProcessor loFi;
    bool loFiEnabled = false;

    // Character -- k/hfCutoff suavizados por bloco, mesmo padrão dos
    // filtros (recomputar por amostra seria caro à toa e o resultado
    // já é suavizado bloco a bloco de qualquer forma).
    CharacterProcessor character;
    CharacterMode characterMode = CharacterMode::Digital;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> characterAmountSmooth;
    bool characterParamsInitialised = false;
    bool characterActive = false;

    // Micro-wow do Tape -- fase própria, bem mais lenta que a
    // modulação principal e independente dela (ver Seção 11 do
    // briefing: "muito pequeno, muito lento, decorrelated"). Só
    // contribui quando characterMode==Tape && characterActive.
    float tapeWowPhase = 0.0f;
    static constexpr float tapeWowRateHz = 0.15f;
    static constexpr float tapeWowMaxMs = 0.4f;

    double sampleRate = 44100.0;
    bool parametersInitialised = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayEngine)
};
}
