-- Postgres schema for the Date.now() API.
-- This is a fresh, consolidated schema (not a replay of the incremental
-- SQLite migrations in ../*.sql) — it captures the target state directly,
-- including the User table's encrypted-at-rest columns (email, totpSeed)
-- and the email_hash blind index used for equality lookups. See the
-- migration plan for the rationale (email/totpSeed encryption design).
--
-- Legacy SQLite tables `Author` and `Subscriber` are NOT carried over —
-- confirmed unused by the current codebase (authors are Users with
-- role='AUTHOR', subscribers are Users with subscribedAt set).

-- MEDIA
DROP TABLE IF EXISTS Media CASCADE;
CREATE TABLE Media(
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	textAlternatif TEXT NOT NULL,
	url TEXT NOT NULL,
	thumbUrl TEXT,
	width DOUBLE PRECISION,
	height DOUBLE PRECISION
);

-- USER
-- email/totpSeed are stored as AES-256-GCM ciphertext (nonce || ciphertext
-- || tag) in BYTEA columns. email_hash is an HMAC-SHA256 blind index
-- (deterministic, non-reversible) used for equality lookups (login,
-- uniqueness) since the ciphertext itself cannot be queried by equality.
DROP TABLE IF EXISTS AppUser CASCADE;
CREATE TABLE AppUser(
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	username VARCHAR(255),
	email BYTEA NOT NULL,
	emailHash BYTEA NOT NULL,
	role VARCHAR(10) CHECK(role IN ('USER', 'AUTHOR')) NOT NULL DEFAULT 'USER',

	-- AUTH
	totpSeed BYTEA,

	-- AUTHOR
	picture BIGINT,
	link TEXT,

	-- USER
	subscribedAt TIMESTAMPTZ,
	isSupporter BOOLEAN NOT NULL DEFAULT FALSE,

	createdAt TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL,
	trackerPixelConsentDate TIMESTAMPTZ,

	FOREIGN KEY(picture) REFERENCES Media(id),
	UNIQUE(emailHash)
);

-- TAG
DROP TABLE IF EXISTS Tag CASCADE;
CREATE TABLE Tag(
	name VARCHAR(64) PRIMARY KEY,
	color VARCHAR(16) NOT NULL
);

-- CATEGORY
DROP TABLE IF EXISTS Category CASCADE;
CREATE TABLE Category(
	name VARCHAR(64) PRIMARY KEY,
	color VARCHAR(16) NOT NULL
);

-- SPONSOR
DROP TABLE IF EXISTS Sponsor CASCADE;
CREATE TABLE Sponsor(
	name VARCHAR(255) PRIMARY KEY,
	link TEXT NOT NULL
);

-- FEED
DROP TABLE IF EXISTS Feed CASCADE;
CREATE TABLE Feed(
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	name VARCHAR(255) NOT NULL,
	link TEXT NOT NULL,
	isRssFeed BOOLEAN NOT NULL DEFAULT FALSE
);

-- FEEDTAG
DROP TABLE IF EXISTS FeedTag CASCADE;
CREATE TABLE FeedTag(
	feedId BIGINT NOT NULL,
	tagName VARCHAR(64) NOT NULL,
	FOREIGN KEY(feedId) REFERENCES Feed(id) ON DELETE CASCADE,
	FOREIGN KEY(tagName) REFERENCES Tag(name) ON DELETE CASCADE,
	PRIMARY KEY(feedId, tagName)
);

-- ISSUE
DROP TABLE IF EXISTS Issue CASCADE;
CREATE TABLE Issue(
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	slug VARCHAR(255) NOT NULL,
	title VARCHAR(255) NOT NULL,
	subtitle VARCHAR(255) NOT NULL,
	cover BIGINT,
	createdAt TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL,
	publishedAt TIMESTAMPTZ,
	updatedAt TIMESTAMPTZ,
	issueNumber INTEGER NOT NULL CHECK(issueNumber >= 0),
	excerpt TEXT NOT NULL,
	isSponsored BOOLEAN DEFAULT FALSE NOT NULL,
	status VARCHAR(10) CHECK(status IN ('DRAFT', 'PUBLISHED', 'ARCHIVE')) DEFAULT 'DRAFT' NOT NULL,
	openedMailCount INTEGER DEFAULT 0 NOT NULL,
	vodUrl VARCHAR(2048),
	FOREIGN KEY(cover) REFERENCES Media(id)
);

-- VIEW
DROP TABLE IF EXISTS View CASCADE;
CREATE TABLE View(
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	time TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP NOT NULL,
	hashedIp TEXT NOT NULL,
	issueId BIGINT NOT NULL,
	FOREIGN KEY(issueId) REFERENCES Issue(id) ON DELETE CASCADE
);

-- ISSUEAUTHOR
DROP TABLE IF EXISTS IssueAuthor CASCADE;
CREATE TABLE IssueAuthor(
	userId BIGINT NOT NULL,
	issueId BIGINT NOT NULL,
	FOREIGN KEY(issueId) REFERENCES Issue(id) ON DELETE CASCADE,
	FOREIGN KEY(userId) REFERENCES AppUser(id) ON DELETE CASCADE,
	PRIMARY KEY(userId, issueId)
);

-- ISSUETAG
DROP TABLE IF EXISTS IssueTag CASCADE;
CREATE TABLE IssueTag(
	tagName VARCHAR(64) NOT NULL,
	issueId BIGINT NOT NULL,
	FOREIGN KEY(issueId) REFERENCES Issue(id) ON DELETE CASCADE,
	FOREIGN KEY(tagName) REFERENCES Tag(name) ON DELETE CASCADE,
	PRIMARY KEY(tagName, issueId)
);

-- ISSUESPONSOR
DROP TABLE IF EXISTS IssueSponsor CASCADE;
CREATE TABLE IssueSponsor(
	sponsorName VARCHAR(255) NOT NULL,
	issueId BIGINT NOT NULL,
	link TEXT NOT NULL,
	FOREIGN KEY(issueId) REFERENCES Issue(id) ON DELETE CASCADE,
	FOREIGN KEY(sponsorName) REFERENCES Sponsor(name) ON DELETE CASCADE,
	PRIMARY KEY(sponsorName, issueId)
);

-- ISSUESECTION
DROP TABLE IF EXISTS IssueSection CASCADE;
CREATE TABLE IssueSection(
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	issueId BIGINT NOT NULL,
	position INTEGER NOT NULL,
	type VARCHAR(10) CHECK(type IN ('CATEGORY', 'TEXT')) NOT NULL,
	categoryName VARCHAR(64),
	textBody TEXT,
	FOREIGN KEY(issueId) REFERENCES Issue(id) ON DELETE CASCADE,
	FOREIGN KEY(categoryName) REFERENCES Category(name) ON DELETE CASCADE,
	CHECK (
		(type = 'CATEGORY' AND categoryName IS NOT NULL AND textBody IS NULL) OR
		(type = 'TEXT' AND textBody IS NOT NULL AND categoryName IS NULL)
	)
);

-- ARTICLE
DROP TABLE IF EXISTS Article CASCADE;
CREATE TABLE Article(
	id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
	sectionId BIGINT NOT NULL,
	position INTEGER NOT NULL,
	title TEXT NOT NULL,
	sourceName TEXT NOT NULL,
	sourceUrl TEXT NOT NULL,
	summary TEXT NOT NULL,
	FOREIGN KEY(sectionId) REFERENCES IssueSection(id) ON DELETE CASCADE
);
