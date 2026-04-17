# API Documentation — `api.json` Schema Reference

This file documents the structure of `api.json`, which drives the interactive API documentation UI. Edit this file whenever endpoints are added, removed, or modified.

---

## Table of contents

- [Validation](#validation)
- [Top-level structure](#top-level-structure)
- [Table object](#table-object)
- [UriParameter object](#uriparameter-object)
- [Schema object](#schema-object)
- [Body object](#body-object)
- [Custom responses](#custom-responses)
- [List parameters](#list-parameters-pagination--filters)
- [CustomEndpoint object](#customendpoint-object)
- [Authentication defaults](#authentication-defaults)
- [Full example](#full-example)
- [Checklist before adding a table](#checklist-before-adding-a-table)

---

## Validation

A linter script (`validate-api.mjs`) is included at the root of the repository. It runs on pure Node.js with no dependencies and checks the integrity of `api.json` before it reaches the UI.

### What it checks

- Valid JSON and presence of the root `tables` array
- Unique `name.singular` across all tables
- `uriParameters`: valid types, presence of `defaultValue`, at most one `isPrimary`
- Coherence between `includedEndpoints` and `isPrimary`: if `GET-single`, `PUT` or `DELETE` are active, a primary parameter must be declared
- `includedEndpoints`: only recognized values
- `schema` and `response` fields: valid type notation, `foreignKey-X` and `array-X` references pointing to existing tables
- `body.post` / `body.put`: `defaultValue` is valid JSON, multipart fields are complete
- `listParameters` and `queryParameters`: valid types, `options` present for `select`, `min` ≤ `max`
- `customEndpoints`: unique names within a table, valid `method`, numeric response codes
- `tokenRequired`: valid keys and boolean values
- Warnings for empty `{}` / `[]` that should be omitted

### Running locally

```bash
node validate-api.mjs
# or with an explicit path
node validate-api.mjs path/to/api.json
```

### GitHub Actions

The workflow at `.github/workflows/lint.yml` runs the linter automatically on every push or pull request that touches `api.json` or `validate-api.mjs`. It runs inside a `node:22-alpine` Docker container — no setup action required.

```yaml
name: Lint API schema

on:
  push:
    paths:
      - "api.json"
      - "validate-api.mjs"
  pull_request:
    paths:
      - "api.json"
      - "validate-api.mjs"

jobs:
  validate:
    name: Validate api.json
    runs-on: ubuntu-latest
    container:
      image: node:22-alpine
    steps:
      - uses: actions/checkout@v4
      - run: node validate-api.mjs api.json
```

The job exits with code `1` on any error, blocking the merge. Warnings do not fail the job.

> **Note on references** — `foreignKey-X` and `array-X` values are resolved against `name.singular` of other tables. If a table is renamed, all references to it must be updated manually. The linter will catch any dangling reference immediately.

---

## Top-level structure

```json
{
  "tables": [ /* array of Table objects */ ]
}
```

---

## Table object

Each table represents a resource (e.g. `user`, `issue`, `media`).

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | `{ singular, plural }` | yes | Display name used in endpoint titles |
| `uri` | `string` | yes | Base URI for the resource (e.g. `/user`) |
| `uriParameters` | `UriParameter[]` | no | Path parameters, including the primary key. Omit if the table has no URI parameters (e.g. `/auth`). |
| `schema` | `object` | no | Shape of the resource returned by the API. Omit if the table has no schema (e.g. auth-only tables). |
| `description` | `string` | no | Human-readable description of the resource, displayed in the UI |
| `body` | `Body` | no | Request body definitions for POST and/or PUT |
| `response` | `object` | no | Override the default response shape per method (see [Custom responses](#custom-responses)) |
| `listParameters` | `QueryParameter[]` | no | Query parameters available on GET-list (e.g. pagination, filters) |
| `includedEndpoints` | `string[]` | no | Whitelist of auto-generated endpoints to include. Omit to include all five. |
| `customEndpoints` | `CustomEndpoint[]` | no | Extra endpoints that don't fit the standard CRUD pattern |
| `tokenRequired` | `{ post?, put?, delete? }` | no | Per-method auth override (defaults documented below) |

### `name`

```json
"name": { "singular": "issue", "plural": "issues" }
```

`name.singular` is the key used by `foreignKey-X` and `array-X` references across all schemas. It must be unique across all tables.

### `description`

Free-text description displayed in the UI. Strongly recommended for non-CRUD tables (e.g. `/auth`).

```json
"description": "Author authentication via magic link and TOTP."
```

### `includedEndpoints`

Controls which standard endpoints are generated. Possible values:

- `"GET-list"` — paginated list
- `"GET-single"` — single resource by primary key
- `"POST"` — create
- `"PUT"` — update
- `"DELETE"` — delete

Omitting `includedEndpoints` generates **all five**. Pass an empty array `[]` to suppress all standard endpoints (use only `customEndpoints`).

```json
"includedEndpoints": ["GET-list", "GET-single", "POST", "DELETE"]
```

---

## UriParameter object

Defines a segment of the URI path (e.g. `{ID}`, `{NAME}`).

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | `string` | yes | Parameter name, used in labels and to build `{NAME}` placeholders |
| `type` | `"integer" \| "string"` | yes | Input type rendered in the UI |
| `defaultValue` | `string \| number` | yes | Pre-filled value in the form |
| `isPrimary` | `boolean` | no | Marks the primary key parameter; used by GET-single, PUT, DELETE to build the URI |

```json
"uriParameters": [
  { "name": "id", "type": "integer", "defaultValue": 1, "isPrimary": true }
]
```

For pivot/relation tables that need two parameters (e.g. `/issue/{ID}/tag/{NAME}`), define both — only the primary one is used for single-resource endpoints:

```json
"uriParameters": [
  { "name": "tagName", "type": "string", "defaultValue": "Software", "isPrimary": true },
  { "name": "id", "type": "integer", "defaultValue": 1 }
]
```

---

## Schema object

A flat key/value map that describes the shape of the resource. Values are type strings.

### Type notation

| Notation | Meaning |
|---|---|
| `"string"` | Required string |
| `"string?"` | Optional string (nullable) |
| `"integer"` | Required integer |
| `"boolean"` | Boolean |
| `"timestamp"` | Unix timestamp |
| `"datetime"` | ISO 8601 datetime |
| `"double?"` | Optional float |
| `"foreignKey-{resource}"` | Nested object of the given resource (e.g. `"foreignKey-media"`) — `{resource}` must match an existing table `name.singular` |
| `"array-{resource}"` | Array of the given resource (e.g. `"array-tag"`) — `{resource}` must match an existing table `name.singular` |
| `"VALUE_A \| VALUE_B"` | Enum — one of the listed values |

```json
"schema": {
  "id": "integer",
  "title": "string",
  "status": "DRAFT | PUBLISHED | ARCHIVED",
  "cover": "foreignKey-media",
  "publishedAt": "datetime?"
}
```

---

## Body object

Defines the request body for `POST` and/or `PUT` endpoints.

```json
"body": {
  "post": { ... },
  "put":  { ... }
}
```

Omit `body` entirely if the table has no writable endpoints. Do not leave it as `{}`.

### JSON body (default)

| Field | Type | Required | Description |
|---|---|---|---|
| `description` | `string` | no | Description displayed above the body form in the UI |
| `schema` | `object` | no | Displayed as the body schema in the UI. Falls back to the table `schema` if absent. |
| `defaultValue` | `string` | no | JSON string pre-filled in the textarea — must be valid JSON |

```json
"post": {
  "description": "Creates a new post. The slug is auto-generated if omitted.",
  "schema": {
    "title": "string",
    "status": "DRAFT | PUBLISHED | ARCHIVED"
  },
  "defaultValue": "{\n\t\"title\": \"My post\",\n\t\"status\": \"DRAFT\"\n}"
}
```

### Multipart body

Use when the endpoint accepts `multipart/form-data` (e.g. file uploads).

| Field | Type | Required | Description |
|---|---|---|---|
| `multipart` | `true` | yes | Switches the UI to a multipart form |
| `fields` | `MultipartField[]` | yes | List of form fields |

#### MultipartField

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | `string` | yes | Field name sent in the request |
| `label` | `string` | yes | Label displayed in the form |
| `type` | `"string" \| "file"` | yes | Input type |
| `required` | `boolean` | no | Marks the field as required |

```json
"post": {
  "multipart": true,
  "fields": [
    { "name": "altText", "label": "Alt text", "type": "string", "required": true },
    { "name": "file", "label": "Image (JPEG, PNG — max 5 MB)", "type": "file", "required": true }
  ]
}
```

---

## Custom responses

Override the auto-generated response shape for specific methods using the `response` key on the table.

Supported keys: `"get-single"`, `"get-list"`.

If a key is absent, the corresponding endpoint falls back to `schema`.

```json
"response": {
  "get-single": {
    "id": "integer",
    "title": "string",
    "authors": "array-author",
    "tags": "array-tag"
  },
  "get-list": {
    "data": "array-issue",
    "total": "integer",
    "page": "integer",
    "pageSize": "integer"
  }
}
```

`"get-list"` is especially useful to model paginated responses or lists that return a reduced set of fields compared to the full resource.

---

## List parameters (pagination & filters)

Query parameters available on **GET-list** endpoints, displayed in the UI alongside the list endpoint.

```json
"listParameters": [
  { "name": "page",   "label": "Page",   "type": "integer", "defaultValue": 1,  "min": 1 },
  { "name": "limit",  "label": "Limit",  "type": "integer", "defaultValue": 20, "min": 1, "max": 100 },
  { "name": "status", "label": "Status", "type": "select",
    "options": [
      { "name": "DRAFT",     "label": "Draft"     },
      { "name": "PUBLISHED", "label": "Published" },
      { "name": "ARCHIVED",  "label": "Archived"  }
    ]
  }
]
```

See [QueryParameter object](#queryparameter-object) for the full field reference.

---

## CustomEndpoint object

Use for endpoints that don't map to standard CRUD operations (e.g. `/auth/login`, `/auth/subscribe`).

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | `string` | yes | Display name — must be unique within the table |
| `description` | `string` | no | Human-readable description displayed in the UI |
| `uri` | `string` | yes | Path **relative to the table `uri`** (e.g. `"/login"` on table `/auth` → `/auth/login`) |
| `method` | `"GET" \| "POST" \| "PUT" \| "DELETE"` | yes | HTTP method |
| `tokenRequired` | `boolean` | no | Defaults: `false` for GET, `true` for POST/PUT/DELETE |
| `responses` | `Response[]` | no | Expected HTTP responses — document both success and error codes |
| `body` | `object` | no | JSON body schema (displayed as-is) |
| `queryParameters` | `QueryParameter[]` | no | URL query parameters |
| `uriParameters` | `UriParameter[]` | no | Additional path parameters |

### Response object

```json
{ "code": 200, "response": "Email sent" }
```

`response` can be a string, an object, or an array. It is recommended to document at least the common error codes (`400`, `401`, `403`, `404`):

```json
"responses": [
  { "code": 200, "response": { "message": "Successfully logged in", "token": "string" } },
  { "code": 400, "response": "Invalid or expired TOTP code" },
  { "code": 404, "response": "User not found" }
]
```

### QueryParameter object

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | `string` | yes | Query string key |
| `label` | `string` | yes | Form label |
| `type` | `"string" \| "integer" \| "select"` | yes | Input type |
| `required` | `boolean` | no | Whether the parameter is mandatory |
| `defaultValue` | `string \| number` | no | Pre-filled value in the form |
| `options` | `{ name, label }[]` | no | Options for `select` type |
| `min` / `max` | `number` | no | Bounds for `integer` type |

---

## Authentication defaults

| Method | `tokenRequired` default |
|---|---|
| GET | `false` |
| POST | `true` |
| PUT | `true` |
| DELETE | `true` |

Override per endpoint using `tokenRequired` on a `CustomEndpoint`, or per method using the table-level `tokenRequired` object:

```json
"tokenRequired": {
  "post": false
}
```

---

## Full example

```json
{
  "tables": [
    {
      "name": { "singular": "post", "plural": "posts" },
      "description": "Posts published in the newsletter.",
      "uri": "/post",
      "uriParameters": [
        { "name": "id", "type": "integer", "defaultValue": 1, "isPrimary": true }
      ],
      "schema": {
        "id": "integer",
        "title": "string",
        "status": "DRAFT | PUBLISHED",
        "cover": "foreignKey-media",
        "tags": "array-tag"
      },
      "response": {
        "get-list": {
          "data": "array-post",
          "total": "integer",
          "page": "integer",
          "pageSize": "integer"
        },
        "get-single": {
          "id": "integer",
          "title": "string",
          "status": "DRAFT | PUBLISHED",
          "cover": "foreignKey-media",
          "tags": "array-tag",
          "authors": "array-author"
        }
      },
      "listParameters": [
        { "name": "page",   "label": "Page",   "type": "integer", "defaultValue": 1  },
        { "name": "limit",  "label": "Limit",  "type": "integer", "defaultValue": 20 },
        { "name": "status", "label": "Status", "type": "select",
          "options": [
            { "name": "DRAFT",     "label": "Draft"     },
            { "name": "PUBLISHED", "label": "Published" }
          ]
        }
      ],
      "body": {
        "post": {
          "description": "Creates a new post. The slug is auto-generated if omitted.",
          "schema": { "title": "string", "status": "DRAFT | PUBLISHED" },
          "defaultValue": "{\n\t\"title\": \"Hello\",\n\t\"status\": \"DRAFT\"\n}"
        },
        "put": {
          "schema": { "title": "string", "status": "DRAFT | PUBLISHED" }
        }
      },
      "includedEndpoints": ["GET-list", "GET-single", "POST", "PUT", "DELETE"],
      "customEndpoints": [
        {
          "name": "publish",
          "description": "Sets the post status to PUBLISHED and sends the newsletter.",
          "uri": "/{ID}/publish",
          "method": "POST",
          "responses": [
            { "code": 200, "response": "Post published" },
            { "code": 404, "response": "Post not found" },
            { "code": 409, "response": "Post already published" }
          ]
        }
      ]
    }
  ]
}
```

---

## Checklist before adding a table

Run `node validate-api.mjs` after any change — the items below reflect what the linter checks automatically and what it cannot catch.

**Caught by the linter**
- [ ] `name.singular` is unique across all tables
- [ ] All `foreignKey-X` / `array-X` references in `schema`, `response`, and `body.schema` point to an existing `name.singular`
- [ ] `includedEndpoints` contains only recognized values
- [ ] A `uriParameter` with `"isPrimary": true` exists when `GET-single`, `PUT` or `DELETE` are active
- [ ] `body.post.defaultValue` and `body.put.defaultValue` are valid JSON strings
- [ ] `select` query parameters have a non-empty `options` array
- [ ] Empty `{}` / `[]` fields that should be omitted are flagged

**Requires manual review**
- [ ] `body.post.schema` and `body.put.schema` do not include auto-managed fields (`id`, `createdAt`, `updatedAt`…)
- [ ] `response["get-list"]` is defined if the list returns a paginated or reduced shape
- [ ] Common error codes (`400`, `401`, `403`, `404`) are documented in `responses` for each `customEndpoint`
- [ ] `description` is filled in for non-CRUD tables and non-obvious custom endpoints
