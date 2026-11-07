## Roadmap

- [] Apply a real policy for CORS origins

## Generate JWT_SECRET

Use of the `openssl` lib

```bash
openssl rand -hex 32
```
## Use of Docker

### First build and after code changes

```bash
docker compose up --build -d
docker compose exec api sh
```

## Email validation

An email is checked before a user can subscribe, an author can be registered, or the email of a user can be changed. The checks stop at the first one that refuses the email, from the cheapest to the slowest:

1. `EMAIL_VALIDATION_BYPASS=1` accepts everything.
2. The domain of the email must be a valid domain name.
3. The domain must not be `APP_DOMAIN`, the domain of the service.
4. The domain must not be in the blocklist (table `BlockedEmailDomain`).
5. The domain must be able to receive mail: an MX record, or an A or AAAA record when there is no MX. A "null MX" (`MX 0 .`, which `example.com` has) says that the domain does not receive mail.

| Variable | Default | Description |
| --- | --- | --- |
| `EMAIL_VALIDATION_BYPASS` | unset | `1` skips every check (CI, local development). |
| `EMAIL_DNS_CHECK` | `1` | `0` skips the DNS lookup. The lookup blocks until the resolver answers, so its time is set by the configuration of the resolver. |
| `EMAIL_BLOCKLIST_MODE` | `reject` | `reject` refuses an email whose domain is blocked (400). `flag` accepts it and flags the user (`isEmailFlagged`), who then does not receive the newsletter. |
| `BLOCKED_DOMAIN_LIST_PUBLIC` | unset | `1` lets anyone read `GET /api/blocked-domain`. Otherwise it needs an author. |
| `APP_DOMAIN` | unset | The domain of the service, which cannot be used for an email. |

The blocklist is managed with `GET`, `POST` and `DELETE` on `/api/blocked-domain`, and the flag of a user by hand with `PUT /api/user/:id/flag` (see `docs/api.json`). A flag that an author sets by hand is never changed by a new email.

### Migrations

Apply the Postgres migrations in order, with `psql "$DATABASE_URL" -f <file>`:

```bash
for f in migrations/postgres/*.sql; do psql "$DATABASE_URL" -v ON_ERROR_STOP=1 -f "$f"; done
```

`001-email-validation.sql` adds the blocklist and the flag columns, and `002-email-validation-seed.sql` fills the blocklist with 240 disposable domains (it can be run again). `000-init.sql` drops and creates every table: run it only on an empty database.

