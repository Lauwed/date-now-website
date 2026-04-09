#pragma once

#include <stddef.h>
#include <structs.h>

/* Retourne 0 si l'utilisateur existe et écrit le userId dans *user_id.
   Retourne -1 si non trouvé. */
int auth_get_user_by_email(const char *email, int *user_id,
                           char totp_seed[255]);

/* Stocke un email token (code hashé SHA1 hex) avec expiry dans 15 min. */
int auth_create_email_token(int user_id, const char *token_hash);

/* Vérifie le token : retourne user_id si valide et non expiré, -1 sinon.
   Invalide automatiquement le token après vérification. */
int auth_verify_email_token(const char *email, const char *token_hash,
                            int *user_id);

/* Crée une session et stocke le token hashé. Expiry = 24h. */
int auth_create_session(int user_id, const char *token_hash);

/* Vérifie une session par son token hashé.
   Retourne user_id si valide, -1 sinon. */
int auth_verify_session(const char *token_hash);

/* Révoque une session par son token hashé. */
int auth_revoke_session(const char *token_hash);
