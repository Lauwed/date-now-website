# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**date-now** is a newsletter platform with two components:
- `api/` — C HTTP server using [Mongoose](https://mongoose.ws/) and SQLite
- `front/` — SvelteKit 5 frontend using TypeScript and SCSS

## API (C backend)

### Build & Run

```sh
cd api
make              # compiles to bin/serv_api
./bin/serv_api    # runs on http://0.0.0.0:8000
make clean        # removes compiled artifacts
```

The server opens `uwu.db` (SQLite) from the working directory — always run from `api/`.

### Database Migrations

Migration SQL files are in `api/migrations/`. Apply them manually with sqlite3:

```sh
sqlite3 uwu.db < migrations/000-init.sql
sqlite3 uwu.db < migrations/001-issue-author.sql
```

### Architecture

The C API uses a layered structure:
- `src/main.c` — Mongoose event loop + URL routing (manual pattern matching via `mg_match`)
- `src/endpoints/<entity>.c` — HTTP method dispatch (GET/POST/PUT/DELETE) and response serialization
- `src/sql/<entity>.c` — SQLite query logic
- `include/structs.h` — all shared data structures (`struct issue`, `struct user`, `struct tag`, etc.)
- `include/macros/endpoints.h` — error reply macros (`ERROR_REPLY_400`, `ERROR_REPLY_404`, etc.)
- `include/enums.h` — shared enums (HTTP status codes used internally)
- `src/lib/` — vendored libraries: Mongoose, SQLite3, validatejson

**Entities:** `user`, `tag`, `sponsor`, `issue`, `issue_tag`, `issue_author`, `issue_sponsor`, `view`

**API base path:** `/api/<entity>` — all other paths are served as static files from `.`.

**Response pattern:** List endpoints support `?q=` (search), `?sort=`, `?page=`, and `?page_size=` query params. Responses use `struct list_reply` (with pagination metadata) or `struct error_reply`.

## Frontend (SvelteKit)

### Commands

```sh
cd front
bun install       # install dependencies
bun run dev       # dev server
bun run build     # production build
bun run preview   # preview build on port 8083
bun run check     # TypeScript + Svelte type checking
bun run lint      # prettier check
bun run format    # prettier write
```

### Architecture

- `src/routes/` — SvelteKit file-based routing: `/`, `/archive`, `/issue`, `/about`, `/legal-notice`, `/used-newsletters`
- `src/lib/components/` — reusable Svelte components (Article, ArticlePreview, Nav, Footer, Tag, etc.)
- `src/lib/layout/` — layout-level components (Hero)
- `src/lib/styles/` — SCSS partials: `_variables.scss`, `_mixins.scss`, `_typography.scss`, `global.scss`
- `src/types.ts` — shared TypeScript interfaces (`Article`, `Tag`, `Block`, `Color`)

**Content rendering:** Article content is stored as a `Block[]` tree (each block has `tag`, `content`, optional `children`, `class`, and `attributes`). Rendered by `BlockRenderer.svelte` / `Block.svelte`.

**Styling:** SCSS with global variables and mixins. Font families: General Sans (sans) and JetBrains Mono (mono).
