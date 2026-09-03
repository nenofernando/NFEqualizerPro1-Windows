#pragma once

/** Chave pública RSA-2048 do NF License System, em PEM padrão - só pra
    referência/leitura humana e pra ferramentas auxiliares (ex: reextrair
    os componentes crus se precisar). O código de verificação de verdade
    (NFLicenseVerify.h) usa a versão já extraída em NFLicensePublicKeyRaw.h
    (expoente/módulo em hex direto), não faz parsing de PEM em tempo de
    execução.

    A mesma chave serve pra todos os plugins da linha NF (Vocal
    Compressor, Pro EQ, Pro Verb, Loudness Meter, Tape Machine...). A
    chave privada correspondente mora exclusivamente como secret nas Edge
    Functions do Supabase (projeto nf-license-system), nunca em
    código-fonte nenhum.

    Se a chave privada algum dia for comprometida ou perdida, é preciso
    gerar um par novo (scripts/generate-keypair.js no nf-license-system) e
    atualizar esta constante E NFLicensePublicKeyRaw.h em TODOS os
    plugins da linha.
*/
namespace NFLicense
{
    static constexpr const char* publicKeyPem =
        "-----BEGIN PUBLIC KEY-----\n"
        "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAv1xmWpVSJLYZQ8YlGFWL\n"
        "0iwUPhhm/LyTFe2jt78SRwRhNxUoal4tJwNvctZSZbhsA6NWqTLnhdSQRDM3W1Ww\n"
        "Ju/jZueGS2RHG8fzb7zZsh+0PGb8D5gYlPMfTiFr/ouYthsOnjEAsmWB4lep0hV5\n"
        "yiREiuiz3jgiHXznSbipDkj1K9/8PYAbbQvHyrcBCrqoc8TI6nsIb541ib08j4ou\n"
        "ydDFAqUwDX5H/QVix/haGTbDszkzns3EVHzWrabffqZLu8rVKAHALvWaxAO1J5pE\n"
        "d9B+CxyKAHf++PvSr9ViSqaV7nTwrdPZQWEhrv7f+mLL820l14X3Da1Cowsry5H5\n"
        "bQIDAQAB\n"
        "-----END PUBLIC KEY-----\n";
}
