-- Migration 003 - Email validation schema
-- Adds the BlockedEmailDomain table and email flag columns on User.
-- Run 003-email-validation-seed.sql after this to populate the blocklist.

-- Blocklist of disposable/temporary email domains
CREATE TABLE IF NOT EXISTS BlockedEmailDomain (
    domain TEXT PRIMARY KEY
);

-- Flag column: 1 if the user's email belongs to a known disposable domain
ALTER TABLE User ADD COLUMN isEmailFlagged BOOLEAN NOT NULL DEFAULT 0;

-- Reason for the flag (static string: "blocked_domain" or "manual_override")
ALTER TABLE User ADD COLUMN emailFlagReason TEXT;
