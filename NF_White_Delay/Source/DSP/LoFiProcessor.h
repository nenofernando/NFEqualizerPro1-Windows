#pragma once
#include <cmath>
#include <algorithm>

namespace NF
{
// ============================================================
// FASE 6.6 -- reescrita do "lo-fi" a partir de teste real: a versão
// da FASE 5 (sample-and-hold -> quantização de 9 bits COM dither TPDF
// -> amaciamento -> saturação) tinha um piso de ruído perceptível --
// o dither, mesmo pequeno (~1 LSB), é audível em silêncio/passagens
// baixas, e não é o caráter que o usuário quer aqui: "sem hiss, sem
// noise, sem dither audível, sem vinyl noise, sem tape noise".
//
// A quantização de bit-depth foi REMOVIDA por completo (não só o
// dither) -- decisão explícita do usuário ("prefiro essa segunda
// opção se soar melhor": tirar a quantização do feedback e construir
// o caráter só pelos outros estágios). Sem quantização não-linear no
// loop, não existe mais risco de limit cycle -- os estágios que
// sobraram são todos filtros LINEARES (HPF/LPF one-pole) + a mesma
// saturação tanh(k*x)/k de ganho de pequeno sinal EXATO 1 usada em
// CharacterProcessor.h, então a prova de estabilidade do loop de
// feedback (FASE 4/5) se aplica aqui sem nenhuma mudança -- e, mais
// importante pro pedido do usuário: com entrada exatamente 0 e estado
// inicial 0, todo estágio devolve exatamente 0 pra sempre (filtro
// linear * 0 = 0, tanh(0)/k = 0) -- SEM NENHUMA fonte de ruído
// (nenhum juce::Random, nenhum dither), o Lo-Fi não pode gerar piso
// de ruído sozinho, por construção, não só na prática.
//
// Cadeia (Seção 2 do pedido, "bandwidth reduction + sample-rate style
// reduction + soft saturation + leve instabilidade", NÃO um
// bitcrusher):
//
//   HPF leve fixo (~110Hz) -> pre-filter (~9kHz, ANTES do hold --
//   tone-shaping PARCIAL, não anti-alias completo, ver nota logo
//   abaixo em prepare()) -> sample-and-hold (textura vintage/digital
//   antiga) -> LPF de amaciamento pós-hold (~7kHz, com uma variação
//   MUITO lenta e sutil de cutoff -- a "micro instabilidade" sugerida,
//   nunca afeta o TEMPO do delay, só a resposta em frequência deste
//   estágio) -> saturação suave.
//
// Cada estágio remove só uma fatia do espectro/arredonda transientes
// -- o objetivo é soar como "delay antigo/limitado em banda", não
// bitcrusher com ruído.
//
// FATOR DE HOLD ADAPTATIVO AO SAMPLE RATE (verificação pedida antes de
// considerar o Lo-Fi definitivo): um downsampleFactor FIXO (ex.: 4x)
// faz a "taxa efetiva" do hold escalar linearmente com o sample rate
// da sessão -- 44.1kHz/4≈11kHz, 48kHz/4=12kHz, mas 96kHz/4=24kHz e
// 192kHz/4=48kHz, ambos bem acima do que qualquer ouvido percebe como
// "lo-fi", esvaziando o caráter nas taxas altas. Medido diretamente
// (comparando o sinal COM e SEM o estágio de hold, mesmo tom de
// teste): a contribuição real do hold caía de 0.225 (44.1kHz) pra
// 0.049 (192kHz) -- só 22% do original, o efeito ficava quase
// inaudível. Corrigido: downsampleFactor agora é calculado em
// prepare() a partir do sample rate, mirando uma taxa efetiva
// aproximadamente CONSTANTE (~12kHz, dentro da faixa 11-14kHz
// sugerida) -- em 44.1kHz dá 4x (idêntico a antes, sem mudança na
// faixa mais comum), 48kHz dá 4x, 96kHz dá 8x, 192kHz dá 16x. Com
// isso, a mesma comparação (com/sem hold) ficou consistente em todas
// as taxas (0.225/0.206/0.218/0.227 -- variação de ~10%, não mais os
// 78% de antes). O pre-filter (~9kHz, fixo, ver nota em prepare() --
// é tone-shaping parcial, não anti-alias completo) não precisou de
// nenhum ajuste adicional: como a taxa efetiva do hold agora é
// estável, a relação entre ele e a Nyquist efetiva também fica
// estável -- ele já deixa passar uma quantidade pequena e consistente
// de aliasing como parte do caráter (pedido explícito: não eliminar
// toda a aliasing, só evitar "harsh"), sem precisar escalar com o
// sample rate.
// ============================================================

class LoFiProcessor
{
public:
    void prepare(double sampleRateIn) noexcept
    {
        sampleRate = sampleRateIn;

        // Taxa efetiva alvo ~constante (ver nota no topo do arquivo) --
        // downsampleFactor calculado a partir do sample rate real, não
        // mais um valor fixo. jmax garante pelo menos 1x (sem hold
        // "negativo" ou zero, que quebraria o módulo abaixo) em
        // qualquer sample rate plausível.
        constexpr float targetEffectiveRateHz = 12000.0f;
        downsampleFactor = (int) std::round(sampleRate / (double) targetEffectiveRateHz);
        if (downsampleFactor < 1)
            downsampleFactor = 1;

        constexpr float twoPi = 6.28318530717958647692f;

        // HPF leve -- "um pouco menos de graves" (Seção final do
        // pedido), não um corte agressivo.
        constexpr float hpCutoffHz = 110.0f;
        hpCoeff = 1.0f - std::exp(-twoPi * hpCutoffHz / (float) sampleRate);

        // Pre-filter ANTES do sample-and-hold -- tone-shaping PARCIAL,
        // NÃO um anti-alias filter completo (esclarecimento pedido
        // antes de fechar o Lo-Fi). Com a taxa efetiva do hold em
        // ~12kHz (ver nota no topo do arquivo), a Nyquist efetiva é
        // ~6kHz -- um anti-alias de verdade precisaria cortar ali. Este
        // filtro corta em ~9kHz, ACIMA dessa Nyquist efetiva de
        // propósito: deixa passar uma faixa de conteúdo (6-9kHz) que
        // vai gerar aliasing controlado no hold, em vez de eliminá-lo
        // por completo -- é esse aliasing parcial que contribui pro
        // caráter "granulado"/vintage pedido (Seção 3 do briefing
        // original: "não quero eliminar toda a aliasing... mas também
        // não quero harsh aliasing acidental"). Preservado assim de
        // propósito: testado por comparação offline contra 6kHz (o
        // corte "anti-alias completo" de verdade) e 5kHz, e mantido em
        // 9kHz por decisão do usuário. Não reduzir sem um novo teste
        // auditivo -- ver Tests/LoFiTests.cpp e o relatório da FASE 6.6
        // pra a comparação.
        constexpr float preHoldFilterCutoffHz = 9000.0f;
        preHoldFilterCoeff = 1.0f - std::exp(-twoPi * preHoldFilterCutoffHz / (float) sampleRate);

        // LPF de amaciamento pós-hold -- "menos agudos" (Seção final),
        // dentro da faixa 6-10kHz sugerida.
        baseSoftenCutoffHz = 7000.0f;

        // Micro-instabilidade -- variação MUITO lenta (~0.15Hz) e
        // sutil (±12%) do cutoff de amaciamento, dando uma leve
        // "respiração" de textura sem nunca tocar no tempo do delay.
        instabilityPhaseIncrement = (float) (0.15 * downsampleFactor / sampleRate);

        reset();
    }

    // Só para o teste de consistência entre sample rates (Tests/
    // LoFiTests.cpp) -- força um downsampleFactor específico (1 =
    // efetivamente sem hold, usado como referência pra medir a
    // CONTRIBUIÇÃO real do hold por diferença). NÃO é chamado pelo
    // DelayEngine -- produção sempre usa o valor calculado em
    // prepare(). Chamar depois de prepare().
    void setDownsampleFactorForTesting(int factor) noexcept
    {
        downsampleFactor = factor < 1 ? 1 : factor;
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            hpState[ch] = 0.0f;
            preHoldFilterState[ch] = 0.0f;
            holdValue[ch] = 0.0f;
            holdCounter[ch] = 0;
            softenState[ch] = 0.0f;
        }
        instabilityPhase = 0.0f;
        currentSoftenCoeff = 1.0f - std::exp(-6.28318530717958647692f * baseSoftenCutoffHz / (float) sampleRate);
    }

    float process(int channel, float input) noexcept
    {
        // 1) HPF leve.
        float& hp = hpState[channel];
        hp += (input - hp) * hpCoeff;
        const float highpassed = input - hp;

        // 2) Anti-alias LPF, antes do hold.
        float& aa = preHoldFilterState[channel];
        aa += (highpassed - aa) * preHoldFilterCoeff;

        // 3) Sample-and-hold -- fator moderado (4x), textura "degrau"
        //    característica sem ser extrema. O coeficiente do LPF de
        //    amaciamento (com a micro-instabilidade) só é recalculado
        //    na MESMA cadência do hold (não amostra a amostra) --
        //    custo de exp() reduzido, sem perder a textura; canal 0
        //    sempre processa antes do canal 1 na mesma amostra (ver
        //    DelayEngine::process()), então currentSoftenCoeff já está
        //    atualizado quando o canal 1 o usa logo abaixo.
        float& held = holdValue[channel];
        int& counter = holdCounter[channel];

        if (counter == 0)
        {
            held = aa;

            if (channel == 0)
            {
                instabilityPhase += instabilityPhaseIncrement;
                if (instabilityPhase >= 1.0f)
                    instabilityPhase -= 1.0f;

                constexpr float twoPi = 6.28318530717958647692f;
                const float wobble = 1.0f + instabilityDepth * std::sin(twoPi * instabilityPhase);
                const float softenCutoffHz = baseSoftenCutoffHz * wobble;
                currentSoftenCoeff = 1.0f - std::exp(-twoPi * softenCutoffHz / (float) sampleRate);
            }
        }
        counter = (counter + 1) % downsampleFactor;

        // 4) LPF de amaciamento pós-hold (com a micro-instabilidade
        //    já embutida no coeficiente).
        float& soften = softenState[channel];
        soften += (held - soften) * currentSoftenCoeff;

        // 5) Saturação suave -- mesma forma segura (ganho de pequeno
        //    sinal exato 1) do CharacterProcessor.h.
        return std::tanh(satK * soften) / satK;
    }

private:
    int downsampleFactor = 4; // recalculado em prepare() a partir do sample rate -- ver nota no topo
    static constexpr float satK = 0.5f;
    static constexpr float instabilityDepth = 0.12f;

    double sampleRate = 44100.0;
    float hpCoeff = 0.0f;
    float preHoldFilterCoeff = 1.0f;
    float baseSoftenCutoffHz = 7000.0f;
    float instabilityPhaseIncrement = 0.0f;
    float instabilityPhase = 0.0f;
    float currentSoftenCoeff = 1.0f;

    float hpState[2] { 0.0f, 0.0f };
    float preHoldFilterState[2] { 0.0f, 0.0f };
    float holdValue[2] { 0.0f, 0.0f };
    int holdCounter[2] { 0, 0 };
    float softenState[2] { 0.0f, 0.0f };
};
}
