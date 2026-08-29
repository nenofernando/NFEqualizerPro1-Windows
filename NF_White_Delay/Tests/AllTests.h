#pragma once

// Uma função por arquivo de teste -- devolve a contagem de falhas.
// Declaradas aqui (e incluídas tanto no arquivo que define quanto em
// main.cpp) só pra silenciar -Wmissing-prototypes; nada de lógica.
int runDelayEngineTests();
int runFeedbackTests();
int runPingPongTests();
int runHostSyncTests();
int runLargeJumpTests();
int runSyncIntegrationTests();
int runFilterTests();
int runModulationTests();
int runModulationCrossfadeTests();
int runStressTests();
int runDuckingTests();
int runLoFiTests();
int runCharacterTests();
int runPhase5StressTests();
int runOutputStageTests();
int runPhase6IntegrationTests();
int runMixLawAndResponseTests();
