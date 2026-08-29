// Ponto de entrada único do executável de testes -- cada arquivo
// (DelayEngineTests.cpp, FeedbackTests.cpp, PingPongTests.cpp,
// HostSyncTests.cpp) expõe uma função runXTests() que devolve sua
// contagem de falhas, em vez de ter um main() por arquivo.
#include "AllTests.h"
#include <iostream>

int main()
{
    int totalFailures = 0;

    totalFailures += runDelayEngineTests();
    totalFailures += runFeedbackTests();
    totalFailures += runPingPongTests();
    totalFailures += runLargeJumpTests();
    totalFailures += runHostSyncTests();
    totalFailures += runSyncIntegrationTests();
    totalFailures += runFilterTests();
    totalFailures += runModulationTests();
    totalFailures += runModulationCrossfadeTests();
    totalFailures += runStressTests();
    totalFailures += runDuckingTests();
    totalFailures += runLoFiTests();
    totalFailures += runCharacterTests();
    totalFailures += runPhase5StressTests();
    totalFailures += runOutputStageTests();
    totalFailures += runPhase6IntegrationTests();
    totalFailures += runMixLawAndResponseTests();

    std::cout << std::endl;

    if (totalFailures == 0)
    {
        std::cout << "ALL TESTS PASSED" << std::endl;
        return 0;
    }

    std::cout << totalFailures << " TEST(S) FAILED" << std::endl;
    return 1;
}
