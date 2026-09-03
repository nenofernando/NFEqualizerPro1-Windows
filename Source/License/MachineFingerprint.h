#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

/** Identificador de máquina pro NF License System.

    Usa juce::SystemStats::getUniqueDeviceID() como primeira escolha -
    confirmado no código-fonte do JUCE (juce_SystemStats_windows.cpp lê o
    UUID do SMBIOS/BIOS; juce_SystemStats_mac.mm lê o IOPlatformUUID via
    IOKit) que esse valor mora na firmware/placa-mãe, não no disco/SO -
    sobrevive a reinstalação de SO, do plugin, da DAW, e a troca de
    periféricos/RAM/disco. Só muda em troca de placa-mãe (Windows) ou
    logic board (Mac).

    Fallback (machine_id_fallback = true): se getUniqueDeviceID() vier
    vazio ou for um dos valores degenerados conhecidos (placa-mãe
    genérica com SMBIOS mal configurado, ver comentário no código-fonte
    do JUCE), usa um hash SHA-256 dos MAC addresses combinados - NUNCA
    envia/guarda o MAC cru, só o hash. */
namespace MachineFingerprint
{
    inline bool isDegenerateId (const juce::String& id)
    {
        if (id.isEmpty())
            return true;

        auto stripped = id.toUpperCase();
        return stripped.containsOnly ("0") || stripped.containsOnly ("F");
    }

    inline juce::String hashedMacFallback()
    {
        juce::StringArray macs;
        for (const auto& mac : juce::MACAddress::getAllAddresses())
            macs.add (mac.toString());

        macs.sort (false); // ordena pra dar sempre o mesmo hash, não importa a ordem que o SO devolveu as interfaces

        juce::SHA256 hash (macs.joinIntoString ("|").toUTF8());
        return hash.toHexString();
    }

    struct Result
    {
        juce::String machineId;
        bool isFallback = false;
    };

    inline Result get()
    {
        auto primary = juce::SystemStats::getUniqueDeviceID();

        if (! isDegenerateId (primary))
            return { primary, false };

        return { hashedMacFallback(), true };
    }
}
