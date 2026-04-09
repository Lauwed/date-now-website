#pragma once

#include <stdint.h>
#include <time.h>

/* Décode un secret Base32 en bytes bruts.
   Retourne la longueur des bytes décodés, ou -1 en cas d'erreur. */
int base32_decode(const char *input, unsigned char *output, int out_len);

/* Génère un OTP TOTP (RFC 6238) à partir d'un secret Base32 et d'un timestamp.
   Le résultat est un code à 6 chiffres écrit dans *otp.
   Retourne 0 en succès, -1 en erreur. */
int totp_generate(const char *base32_secret, time_t ts, uint32_t *otp);

/* Vérifie un code TOTP en testant la fenêtre [-window, +window] par rapport
   au timestamp courant. window=1 est recommandé.
   Retourne 1 si valide, 0 sinon. */
int totp_verify(const char *base32_secret, const char *code, int window);
