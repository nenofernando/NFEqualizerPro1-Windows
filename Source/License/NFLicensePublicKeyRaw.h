#pragma once

/** Componentes crus (expoente e módulo, em hex) da MESMA chave pública de
    NFLicensePublicKey.h, extraídos uma vez (scripts auxiliares do
    nf-license-system) pra montar direto um juce::RSAKey sem precisar de
    um parser ASN.1/DER completo em C++ pra ler o PEM em tempo real.

    Formato esperado por juce::RSAKey: "<expoente_hex>,<modulo_hex>".
*/
namespace NFLicense
{
    static constexpr const char* publicKeyExponentHex = "010001";
    static constexpr const char* publicKeyModulusHex =
        "bf5c665a955224b61943c62518558bd22c143e1866fcbc9315eda3b7bf124704613715286a5e2d27036f72d65265b86c03a356a932e785d4904433375b55b026efe366e7864b64471bc7f36fbcd9b21fb43c66fc0f981894f31f4e216bfe8b98b61b0e9e3100b26581e257a9d21579ca24448ae8b3de38221d7ce749b8a90e48f52bdffc3d801b6d0bc7cab7010abaa873c4c8ea7b086f9e3589bd3c8f8a2ec9d0c502a5300d7e47fd0562c7f85a1936c3b339339ecdc4547cd6ada6df7ea64bbbcad52801c02ef59ac403b5279a4477d07e0b1c8a0077fef8fbd2afd5624aa695ee74f0add3d9416121aefedffa62cbf36d25d785f70dad42a30b2bcb91f96d";
}
