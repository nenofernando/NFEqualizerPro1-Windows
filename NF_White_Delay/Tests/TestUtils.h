#pragma once
#include <juce_core/juce_core.h>
#include <iostream>

// Helper compartilhado entre todos os arquivos de teste -- cada
// arquivo mantém seu próprio contador de falhas local (passado por
// referência) em vez de um global escondido, pra dar pra compor
// vários arquivos de teste num único executável sem um main() por
// arquivo (ver Tests/main.cpp).
namespace NFTests
{
inline void expect(int& failures, bool condition, const juce::String& description)
{
    if (condition)
    {
        std::cout << "  [PASS] " << description << std::endl;
    }
    else
    {
        std::cout << "  [FAIL] " << description << std::endl;
        ++failures;
    }
}
}
