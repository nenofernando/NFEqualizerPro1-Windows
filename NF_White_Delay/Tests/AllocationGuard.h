#pragma once

// Guarda de alocação -- operator new/delete globais (definidos em
// AllocationGuard.cpp, exatamente uma vez em todo o executável de
// testes) só repassam pra malloc/free (comportamento idêntico ao
// padrão o tempo todo), mas registram se alguma alocação aconteceu
// enquanto gGuardActive estiver ligado. Uso: ligar só em volta da
// chamada que deve ser realtime-safe, checar gAllocationSeenInGuard
// logo depois, desligar de novo.
namespace NFTests
{
extern thread_local bool gGuardActive;
extern thread_local bool gAllocationSeenInGuard;
}
