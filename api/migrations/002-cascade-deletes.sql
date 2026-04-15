-- ISSUEAUTHOR: cascade when User or Issue is deleted
ALTER TABLE IssueAuthor RENAME TO IssueAuthor_old;
CREATE TABLE IssueAuthor(
	userId INTEGER NOT NULL,
	issueId INTEGER NOT NULL,
	FOREIGN KEY(issueId) REFERENCES Issue(id) ON DELETE CASCADE,
	FOREIGN KEY(userId) REFERENCES User(id) ON DELETE CASCADE,
	PRIMARY KEY(userId, issueId)
);
INSERT INTO IssueAuthor SELECT * FROM IssueAuthor_old;
DROP TABLE IssueAuthor_old;

-- ISSUETAG: cascade when Tag or Issue is deleted
ALTER TABLE IssueTag RENAME TO IssueTag_old;
CREATE TABLE IssueTag(
	tagName CHAR(64) NOT NULL,
	issueId INTEGER NOT NULL,
	FOREIGN KEY(issueId) REFERENCES Issue(id) ON DELETE CASCADE,
	FOREIGN KEY(tagName) REFERENCES Tag(name) ON DELETE CASCADE,
	PRIMARY KEY(tagName, issueId)
);
INSERT INTO IssueTag SELECT * FROM IssueTag_old;
DROP TABLE IssueTag_old;

-- ISSUESPONSOR: cascade when Sponsor or Issue is deleted
ALTER TABLE IssueSponsor RENAME TO IssueSponsor_old;
CREATE TABLE IssueSponsor(
	sponsorName CHAR(255) NOT NULL,
	issueId INTEGER NOT NULL,
	link TEXT NOT NULL,
	FOREIGN KEY(issueId) REFERENCES Issue(id) ON DELETE CASCADE,
	FOREIGN KEY(sponsorName) REFERENCES Sponsor(name) ON DELETE CASCADE,
	PRIMARY KEY(sponsorName, issueId)
);
INSERT INTO IssueSponsor SELECT * FROM IssueSponsor_old;
DROP TABLE IssueSponsor_old;
