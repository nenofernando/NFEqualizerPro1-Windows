#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include "NFLicensePublicKeyRaw.h"

/** Verificação de assinatura do certificado de ativação do NF License
    System - RSA "cru" (sem padding PKCS1), o mesmo esquema implementado
    do lado do servidor em nf-license-system/supabase/functions/_shared/
    nfSign.ts. Validado ponta a ponta ANTES de virar código de produção
    (nf-license-system/scripts/sign_test.js + rsa_verify_test.cpp - uma
    assinatura feita em JS com esse algoritmo foi verificada de verdade
    com o juce::RSAKey real, positivo e negativo, antes desse header
    existir).

    value < n (hash SHA-256 de 32 bytes é sempre muito menor que o módulo
    RSA-2048 de 256 bytes) => RSAKey::applyToValue faz uma única
    modpow (sig^e mod n), que só bate com o hash esperado se a assinatura
    foi feita com a chave privada correspondente.

    juce::BigInteger::loadFromMemoryBlock é LITTLE-ENDIAN (primeiro byte =
    menos significativo) - por isso os bytes do hash (que vem em
    big-endian, ordem padrão) são invertidos antes de carregar, e a
    assinatura recebida do servidor (que já chega em bytes little-endian,
    ver nfSign.ts) é carregada direto, sem inverter de novo.
*/
namespace NFLicenseVerify
{
    inline juce::RSAKey getPublicKey()
    {
        return juce::RSAKey (juce::String (NFLicense::publicKeyExponentHex) + ","
                              + juce::String (NFLicense::publicKeyModulusHex));
    }

    inline bool verify (const juce::String& json, const juce::String& signatureBase64)
    {
        auto publicKey = getPublicKey();
        if (! publicKey.isValid())
            return false;

        juce::SHA256 sha (json.toRawUTF8(), (size_t) json.getNumBytesAsUTF8());
        auto hashBlock = sha.getRawData();
        const auto* hashPtr = static_cast<const juce::uint8*> (hashBlock.getData());

        if (hashBlock.getSize() != 32)
            return false;

        juce::MemoryBlock hashLE (32);
        auto* hashLEData = static_cast<juce::uint8*> (hashLE.getData());
        for (int i = 0; i < 32; ++i)
            hashLEData[i] = hashPtr[31 - i];

        juce::BigInteger expected;
        expected.loadFromMemoryBlock (hashLE);

        juce::MemoryOutputStream sigStream;
        if (! juce::Base64::convertFromBase64 (sigStream, signatureBase64))
            return false;

        juce::BigInteger signatureValue;
        signatureValue.loadFromMemoryBlock ({ sigStream.getData(), sigStream.getDataSize() });

        if (! publicKey.applyToValue (signatureValue))
            return false;

        return expected == signatureValue;
    }
}
