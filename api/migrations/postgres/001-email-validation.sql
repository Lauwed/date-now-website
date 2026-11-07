-- Email validation (Postgres).
--
-- Adds the blocklist of disposable email domains and the email flag of a user.
-- Run it after 000-init.sql, then 002-email-validation-seed.sql to fill the
-- blocklist. Safe to run again.

-- BLOCKED EMAIL DOMAIN
-- A domain is stored in lowercase, without a trailing dot, so that a lookup by
-- the normalised domain of an email always finds it.
CREATE TABLE IF NOT EXISTS BlockedEmailDomain(
	domain VARCHAR(253) PRIMARY KEY CHECK(domain = LOWER(domain)),
	createdAt TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- USER
-- isEmailFlagged: the email belongs to a blocked domain (mode "flag") or an
-- author flagged it by hand. emailFlagReason: "blocked_domain" or
-- "manual_override".
ALTER TABLE AppUser ADD COLUMN IF NOT EXISTS isEmailFlagged BOOLEAN NOT NULL DEFAULT FALSE;
ALTER TABLE AppUser ADD COLUMN IF NOT EXISTS emailFlagReason TEXT;
