#pragma once

/**
 * @file email.h
 * @brief HTML email sending interface via SMTP (libcurl).
 */

/**
 * @brief Sends an HTML email through the SMTP server configured via
 *        environment variables.
 *
 * Required environment variables (defined in the @c .env file):
 * - @c SMTP_HOST : SMTP host (e.g. "smtp.example.com")
 * - @c SMTP_PORT : SMTP port
 * - @c SMTP_USER : sender email address
 * - @c SMTP_PASSWORD : SMTP password
 *
 * @param to      Recipient email address.
 * @param subject Email subject line.
 * @param html    HTML body of the message.
 * @return 0 on success, non-zero on libcurl error.
 * @note None of the parameters are freed by this function.
 */
int send_mail(const char *to, const char *subject, const char *html);
