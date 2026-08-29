#pragma once
#include <JuceHeader.h>
#include "DSP/DelayEngine.h"
#include "DSP/HostSync.h"
#include "DSP/OutputStage.h"

// ============================================================
// FASE 2 + FASE 3 -- DelayEngine (núcleo + feedback + ping-pong +
// crossfade de tempo) e host sync estão ligados. processBlock() lê
// o AudioPlayHead (com fallback seguro), decide entre tempo livre e
// sincronizado, e aplica feedback/ping-pong. Ainda sem filtros,
// modulação LFO, lo-fi, ducking, character, dry/wet, output ou
// bypass (cada um é uma fase própria; ver DelayEngine.h/HostSync.h
// e o roteiro no briefing). O sinal continua saindo 100% wet.
// ============================================================

class NFWhiteDelayAudioProcessor : public juce::AudioProcessor
{
public:
    NFWhiteDelayAudioProcessor();
    ~NFWhiteDelayAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NF White Delay"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // IDs de parâmetro -- estáveis, não renomear depois sem migration
    // (ver Seção 17 do briefing). São 20 no total (confirmado: 6 de
    // time/sync/feedback, 4 de modulação, 3 de filtros, 4 de
    // ducking/lo-fi/character, 3 de dry-wet/output/bypass). Mantidos
    // como constantes pra não espalhar strings literais pelo resto do
    // código.
    struct ParamIDs
    {
        static constexpr const char* delayTimeMs     = "delayTimeMs";
        static constexpr const char* syncEnabled      = "syncEnabled";
        static constexpr const char* syncDivision      = "syncDivision";
        static constexpr const char* syncModifier      = "syncModifier";
        static constexpr const char* feedback          = "feedback";
        static constexpr const char* pingPong          = "pingPong";

        static constexpr const char* modRate           = "modRate";
        static constexpr const char* modDepth          = "modDepth";
        static constexpr const char* modShape          = "modShape";
        static constexpr const char* modSpread         = "modSpread";

        static constexpr const char* highPass          = "highPass";
        static constexpr const char* lowPass           = "lowPass";
        static constexpr const char* resonance         = "resonance";

        static constexpr const char* duckingAmount     = "duckingAmount";
        static constexpr const char* loFiEnabled       = "loFiEnabled";
        static constexpr const char* characterAmount   = "characterAmount";
        static constexpr const char* delayMode         = "delayMode";

        static constexpr const char* dryWet            = "dryWet";
        static constexpr const char* outputGain        = "outputGain";
        static constexpr const char* bypass            = "bypass";
    };

    // ========================================================
    // VERSÃO DO STATE -- salva junto do XML do APVTS (atributo
    // "stateVersion"). Não dá pra assumir que ranges/defaults dos 20
    // parâmetros nunca vão mudar; isso dá um ponto de entrada real pra
    // migração de presets/sessões antigas quando precisar, em vez de
    // só confiar que o formato ficou congelado pra sempre.
    //
    // v1 = versão inicial (FASE 1) -- os 20 IDs/ranges/defaults desta
    // fase. Sem migração ainda porque só existe esta versão; quando a
    // v2 aparecer, migrateState() é onde a conversão entra.
    // ========================================================

    static constexpr int kCurrentStateVersion = 1;

    juce::AudioProcessorValueTreeState apvts;

    // ========================================================
    // FASE 6.5 -- correção de crash real em host (LUNA/AU, crash log
    // capturado em ~/Library/Logs/DiagnosticReports/LUNA-2026-08-26-
    // 072146.ips): o editor NÃO pode chamar getPlayHead()->getPosition()
    // direto da UI thread -- a stack trace mostrou o crash exatamente
    // em JuceAU::ScopedPlayHead::getPosition(), chamado a partir de
    // NFWhiteDelayAudioProcessorEditor::timerCallback() -- "Scoped" não
    // é decoração: em pelo menos o wrapper AU do JUCE, esse objeto só é
    // válido DENTRO do escopo de processBlock() (audio thread); fora
    // dele, os ponteiros internos podem ser inválidos, e foi
    // exatamente isso que gerou o SIGSEGV.
    //
    // O BPM do host é lido com segurança AQUI, dentro de processChunk()
    // (audio thread, onde getPlayHead() já era chamado antes), e
    // exposto via este std::atomic -- o editor só LÊ este valor, nunca
    // chama getPlayHead() por conta própria (ver PluginEditor.cpp,
    // updateDisplay()).
    // ========================================================
    std::atomic<double> lastKnownHostBpm { -1.0 }; // <= 0 = desconhecido/indisponível

    // ========================================================
    // FASE 7.4 -- "display vivo" orientado por atividade REAL do wet,
    // não mais por uma heurística de parâmetros na UI. O audio thread
    // calcula um envelope simples (pico por bloco + smoothing de
    // attack/release, ver processChunk()) do sinal WET (pós-DelayEngine,
    // pré-mix) e publica aqui; a UI só LÊ este valor (mesmo padrão de
    // lastKnownHostBpm acima) -- nunca toca em delayInputBuffer nem em
    // nenhum outro objeto do audio thread. Não altera em nada o sinal
    // processado -- é só uma leitura/medição em paralelo.
    // ========================================================
    std::atomic<float> wetActivity { 0.0f }; // 0..~1 (pode passar de 1 com feedback alto)

private:
    // Ponto de entrada de migração -- não faz nada hoje (só existe a
    // v1), mas já é chamado de setStateInformation() pra não precisar
    // reestruturar o carregamento de state quando a v2 chegar.
    static void migrateState(juce::XmlElement& stateXml, int loadedVersion);

    NF::DelayEngine delayEngine;

    // ========================================================
    // FASE 6 -- Dry/Wet + Output + Bypass.
    //
    // dryBuffer/delayInputBuffer são pré-alocados em prepareToPlay()
    // com capacidade preparedBlockSize, e NUNCA redimensionados dentro
    // de processBlock() (Seção 2 do briefing). Se o host mandar um
    // bloco maior que preparedBlockSize (raro, mas alguns hosts/
    // configurações permitem), processBlock() processa em CHUNKS de
    // no máximo preparedBlockSize amostras (ver processChunk()) em vez
    // de redimensionar -- zero alocação garantida independente do
    // tamanho de bloco que o host de fato entregar.
    // ========================================================

    void processChunk(juce::AudioBuffer<float>& chunk);

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> delayInputBuffer;
    int preparedBlockSize = 512;

    // Dry/Wet + Output + Bypass -- ver OutputStage.h (extraído do
    // processor pra ficar testável isolado, mesmo padrão do resto do
    // motor).
    NF::OutputStage outputStage;

    double currentSampleRate = 44100.0;
    int maxBlockSize = 512;

    // Estado do smoothing attack/release do envelope de wetActivity --
    // só o audio thread toca nisto (não precisa ser atômico; só o
    // resultado final, publicado em wetActivity, é compartilhado).
    float wetEnvelopeState = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFWhiteDelayAudioProcessor)
};
