#pragma once

/**
 * @file strings.h
 * @brief User-facing string constants: email subjects, bodies, and UI labels.
 *
 * All printf-style format strings use %s placeholders in the order documented
 * by their field names.
 */

/* Subscription confirmation email — format args: app_url, jwt_token */
#define EMAIL_SUBSCRIPTION_SUBJECT \
  "Confirm subscription to Date.now()'s Newsletter"
#define EMAIL_SUBSCRIPTION_BODY_FMT \
  "Clique ici pour confirmer : <a href='%s/confirm?token=%s'>Confirmer</a>"

/* Magic-link login email — format args: app_url, jwt_token */
#define EMAIL_LOGIN_SUBJECT "Log in request to Date.now()"
#define EMAIL_LOGIN_BODY_FMT \
  "Clique ici pour te connecter : <a href='%s/login/totp?token=%s'>Log in</a>"

/* Newsletter issue notification — format args: issue_title (subject), app_url, issue_title (body) */
#define EMAIL_NEWSLETTER_SUBJECT_FMT "Date.now() - %s"
#define EMAIL_NEWSLETTER_BODY_FMT \
  "Un nouveau numero est disponible : <a href=%s>%s</a>"
