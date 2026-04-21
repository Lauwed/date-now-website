# Incohérences entre la documentation et l'implémentation de l'API

> Généré le 2026-04-21. Basé sur la comparaison entre `api/docs/api.json` et les sources `api/src/`.

---

## Critique

### 1. `POST /auth/register` — Implémentation incomplète

**Fichier :** `src/endpoints/auth.c` lignes 258-270

- **Docs :** Crée un compte auteur, renvoie HTTP 201 avec `{ message, totpseed }`.
- **Code :** La fonction `register_user()` vérifie uniquement si l'utilisateur est connecté et renvoie 401 sinon. Aucune logique d'inscription, aucun corps de réponse en cas de succès.

---

### 2. `POST /view` — Authentification non documentée

**Fichier :** `src/endpoints/view.c` lignes 118-126

- **Docs :** Aucune authentification requise (`tokenRequired` absent).
- **Code :** Appelle `is_user_logged()` et renvoie 401 si l'utilisateur n'est pas connecté. Le tracking de vues anonymes est donc bloqué.

---

### 3. `POST /view` — `return` manquant après l'erreur 400 sur `issueId`

**Fichier :** `src/endpoints/view.c` lignes 148-152

- **Code :** Lorsque `issueId` est absent, `ERROR_REPLY_400` est envoyé mais l'exécution continue sans `return`. La vue est créée avec `issueId = 0`.

---

### 4. `POST /issue/{ID}/publish` — Endpoint non implémenté

**Fichier :** `src/main.c` / `src/endpoints/issue.c`

- **Docs :** `POST /issue/{ID}/publish` publie un numéro et envoie la newsletter. Renvoie 200, 401, 403, 404 ou 409.
- **Code :** Aucune route ni handler correspondant. Retourne 404.

---

### 5. `PUT /media/{id}` — Endpoint non implémenté

**Fichier :** `src/endpoints/media.c`

- **Docs :** Met à jour le texte alternatif (`textAlternatif`). Renvoie HTTP 200 avec l'objet media.
- **Code :** Seuls GET, POST et DELETE sont gérés. Un PUT retourne 405.

---

### 6. `DELETE /media/{id}` — Vérification d'intégrité référentielle manquante

**Fichier :** `src/endpoints/media.c` lignes 390-432

- **Docs :** Renvoie HTTP 409 si le média est référencé par un numéro.
- **Code :** Aucune vérification. Le média est supprimé même s'il est référencé.

---

## Mauvais codes HTTP

### 7. `POST /media` — Fichier trop grand → 400 au lieu de 413

**Fichier :** `src/endpoints/media.c` ligne 169

- **Docs :** HTTP 413 `"File too large, maximum size is 5 MB"`
- **Code :** `ERROR_REPLY_400` avec `"File must not exceed 5MB."`

---

### 8. `POST /media` — Type de fichier invalide → 400 au lieu de 415

**Fichier :** `src/endpoints/media.c` ligne 176

- **Docs :** HTTP 415 `"Unsupported media type"`
- **Code :** `ERROR_REPLY_400` avec `"File must be an image."`

---

### 9. `GET /auth/subscribe/confirm` — 201 au lieu de 200

**Fichier :** `src/endpoints/auth.c` lignes 244-245

- **Docs :** HTTP 200 `{ "message": "User successfully subscribed" }`
- **Code :** Renvoie HTTP 201 avec le même message.

---

## Mauvais noms de champs dans les réponses

### 10. `GET /auth/seed` — `"seed"` au lieu de `"totpseed"`

**Fichier :** `src/endpoints/auth.c` ligne 345

- **Docs :** `{ "totpseed": "string" }`
- **Code :** `{ "seed": "string" }`

---

### 11. `POST /user` — Corps de réponse incomplet

**Fichier :** `src/endpoints/user.c` ligne 217

- **Docs :** Renvoie l'objet utilisateur complet (id, username, email, role, link, picture, subscribedAt, isSupporter, createdAt).
- **Code :** Renvoie uniquement `{ "message": "User successfully created" }`.

---

### 12. `POST /view` — Corps de réponse incorrect

**Fichier :** `src/endpoints/view.c` ligne 175

- **Docs :** HTTP 201 avec `{ "id", "time", "issueId" }`.
- **Code :** Renvoie `{ "message": "User successfully created" }` (message trompeur).

---

## Mauvais textes de messages

### 13. `POST /auth/subscribe` — Message de succès différent

**Fichier :** `src/endpoints/auth.c` ligne 160

- **Docs :** `"Confirmation mail sent"`
- **Code :** `"Confirmation mail has been correctly sent"`

---

### 14. `POST /auth/login/totp` — Messages d'erreur 400 différents

**Fichier :** `src/endpoints/auth.c` lignes 544, 560

- **Docs :** `"Invalid or expired TOTP code"`
- **Code :** `"NO TOTP FOUND"` / `"CODE INVALID"`

---

### 15. `POST /sponsor` — Message copy-paste incorrect

**Fichier :** `src/endpoints/sponsor.c` ligne 180

- **Code :** `"Tag successfully created"` → devrait être `"Sponsor successfully created"`

---

### 16. `PUT /sponsor/{id}` — Message copy-paste incorrect

**Fichier :** `src/endpoints/sponsor.c` ligne 283

- **Code :** `"Tag successfully edited"` → devrait être `"Sponsor successfully edited"`

---

### 17. `DELETE /sponsor/{id}` — Message copy-paste incorrect

**Fichier :** `src/endpoints/sponsor.c` ligne 316

- **Code :** `"Tag successfully deleted"` → devrait être `"Sponsor successfully deleted"`

---

## Mauvais paramètres de requête

### 18. `GET /user` — `page_size` au lieu de `limit`

**Fichier :** `src/endpoints/user.c` lignes 46-50

- **Docs :** Paramètre `limit`, valeur par défaut 20.
- **Code :** Paramètre `page_size`, valeur par défaut 10.

---

### 19. `GET /view` — Paramètre `issueId` non implémenté

**Fichier :** `src/endpoints/view.c` lignes 40-50

- **Docs :** Filtre par `issueId`.
- **Code :** Utilise un paramètre générique `q`, aucun filtre par `issueId`.

---

## Valeurs par défaut incorrectes

### 20. `GET /tag` — Page size par défaut 10 au lieu de 20

**Fichier :** `src/endpoints/tag.c` ligne 49

---

### 21. `GET /sponsor` — Page size par défaut 10 au lieu de 20

**Fichier :** `src/endpoints/sponsor.c` ligne 49

---

### 22. `GET /issue` — Page size par défaut 10 au lieu de 20

**Fichier :** `src/endpoints/issue.c` ligne 53

---

### 23. `GET /user` — Page size par défaut 10 au lieu de 20

**Fichier :** `src/endpoints/user.c` ligne 50

---

## Récapitulatif

| Catégorie | Nb |
|---|---|
| Endpoints non implémentés | 3 |
| Bugs critiques (return manquant, auth inattendue) | 2 |
| Mauvais codes HTTP | 3 |
| Mauvais noms de champs / corps de réponse | 3 |
| Mauvais textes de messages | 5 |
| Mauvais paramètres de requête | 2 |
| Valeurs par défaut incorrectes | 4 |
| **Total** | **23** |
