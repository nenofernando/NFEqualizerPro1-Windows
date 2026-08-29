#pragma once

// ============================================================
// FASE 3 -- matemática pura de host sync. De propósito, SEM
// dependência de juce::AudioPlayHead (nem de JUCE nenhum) -- só
// double/enum/cmath. Isso deixa a conversão BPM+divisão+modificador
// -> ms totalmente testável sem precisar simular um host de verdade,
// e deixa claríssimo que não tem nenhuma alocação/I/O/string aqui.
//
// Quem lê o AudioPlayHead de verdade é PluginProcessor::processBlock()
// -- ele extrai bpm/assinatura de tempo (com fallback seguro se o
// host não informar nada) e chama essas funções com valores simples.
// ============================================================

namespace NF
{
enum class SyncDivision
{
    Div1_64 = 0,
    Div1_32,
    Div1_16,
    Div1_8,
    Div1_4,
    Div1_2,
    Div1Bar,
    Div2Bar
};

enum class SyncModifier
{
    Straight = 0,
    Dotted,
    Triplet
};

constexpr double fallbackBpm = 120.0;

// BPM seguro pra usar em cálculo -- qualquer valor <=0, NaN ou Inf
// (host sem BPM, host sem transporte, ou um valor absurdo) vira
// fallbackBpm. Nunca escreve nada de volta no host -- isso é só uma
// sanitização local do valor lido.
double sanitiseBpm(double hostBpmOrInvalid) noexcept;

// ms de uma nota de 1/4 nesse BPM (já sanitizado).
double quarterNoteMs(double bpm) noexcept;

// ms da divisão+modificador pedidos, no BPM (já sanitizado) e
// assinatura de tempo dados. timeSigNumerator/Denominator só entram
// no cálculo de 1 Bar/2 Bars -- as outras divisões não dependem de
// compasso. Denominator <= 0 é tratado como 4 (fallback defensivo,
// nunca deve dividir por zero).
double syncDivisionMs(double bpm, SyncDivision division, SyncModifier modifier,
                      int timeSigNumerator = 4, int timeSigDenominator = 4) noexcept;
}
