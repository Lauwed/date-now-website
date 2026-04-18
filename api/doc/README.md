# API Documentation — `api.json` Schema Reference

This file documents the structure of `api.json`, which drives the interactive API documentation UI. Edit this file whenever endpoints are added, removed, or modified.

---

## Table of contents

- [Validation](#validation)
- [Top-level structure](#top-level-structure)
- [defaultErrors object](#defaulterrors-object)
- [Table object](#table-object)
- [UriParameter object](#uriparameter-object)
- [Schema object](#schema-object)
- [Endpoints object](#endpoints-object)
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
- `defaultErrors`: valid HTTP codes, unique codes, matching `response.code`, non-empty `appliesTo` with recognized method values
- Unique `name.singular` across all tables
- `uriParameters`: valid types, presence of `defaultValue`, at most one `isPrimary`
- Coherence between `includedEndpoints` and `isPrimary`: if `GET-single`, `PUT` or `DELETE` are active, a primary parameter must be declared
- `includedEndpoints`: only recognized values
- `schema` and `response` fields: valid type notation, `foreignKey-X` and `array-X` references pointing to existing tables
- `endpoints` overrides: valid method keys, `includedErrors` references existing `defaultErrors` codes, no shorthand integers in `responses` (use `includedErrors` instead)
- `customEndpoints.responses`: integer shorthands must reference a code declared in `defaultErrors`; full objects must have matching `code` and non-empty `message`
- `body` (legacy): deprecated warning, multipart fields are complete, `defaultValue` is valid JSON
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

The workflow at `.github/workflows/lint.yml` runs the linter automatically on every push or pull request that touches `api.json` or `validate-api.mjs`.

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

---

## Top-level structure

```json
{
  "defaultErrors": [
    /* array of DefaultError objects */
  ],
  "tables": [
    /* array of Table objects */
  ]
}
```

---

## defaultErrors object

Defines error responses that are automatically applied to standard endpoints across all tables. This avoids repeating `401`, `500`, etc. on every single endpoint.

| Field       | Type       | Required | Description                                                        |
| ----------- | ---------- | -------- | ------------------------------------------------------------------ |
| `code`      | `integer`  | yes      | HTTP status code — must be unique across all entries               |
| `response`  | `object`   | yes      | Response body shape — must contain `code` (matching) and `message` |
| `appliesTo` | `string[]` | yes      | Standard endpoint methods this error applies to by default         |

`appliesTo` accepts: `"GET-list"`, `"GET-single"`, `"POST"`, `"PUT"`, `"DELETE"`.

```json
"defaultErrors": [
  {
    "code": 400,
    "response": { "code": 400, "message": "Invalid or missing fields" },
    "appliesTo": ["POST", "PUT"]
  },
  {
    "code": 401,
    "response": { "code": 401, "message": "Unauthorized, valid token required" },
    "appliesTo": ["POST", "PUT", "DELETE"]
  },
  {
    "code": 403,
    "response": { "code": 403, "message": "Forbidden, insufficient permissions" },
    "appliesTo": ["POST", "PUT", "DELETE"]
  },
  {
    "code": 404,
    "response": { "code": 404, "message": "Resource not found" },
    "appliesTo": ["GET-single", "PUT", "DELETE"]
  },
  {
    "code": 500,
    "response": { "code": 500, "message": "Internal server error" },
    "appliesTo": ["GET-list", "GET-single", "POST", "PUT", "DELETE"]
  }
]
```

> **Note** — `defaultErrors` apply automatically to standard endpoints (declared via `includedEndpoints`). For `customEndpoints`, use integer shorthands in `responses` to include them explicitly (see [CustomEndpoint object](#customendpoint-object)).

---

## Table object

Each table represents a resource (e.g. `user`, `issue`, `media`).

| Field               | Type                       | Required | Description                                                                                  |
| ------------------- | -------------------------- | -------- | -------------------------------------------------------------------------------------------- |
| `name`              | `{ singular, plural }`     | yes      | Display name used in endpoint titles                                                         |
| `uri`               | `string`                   | yes      | Base URI for the resource (e.g. `/user`)                                                     |
| `uriParameters`     | `UriParameter[]`           | no       | Path parameters, including the primary key                                                   |
| `schema`            | `object`                   | no       | Shape of the resource returned by the API                                                    |
| `description`       | `string`                   | yes      | Human-readable description of the resource                                                   |
| `endpoints`         | `object`                   | no       | Per-endpoint overrides for standard endpoints (description, body, includedErrors, responses) |
| `body`              | `Body`                     | no       | **Deprecated** — use `endpoints.POST.body` / `endpoints.PUT.body` instead                    |
| `response`          | `object`                   | no       | Override the default response shape per method                                               |
| `listParameters`    | `QueryParameter[]`         | no       | Query parameters available on GET-list                                                       |
| `includedEndpoints` | `string[]`                 | no       | Whitelist of auto-generated endpoints to include. Omit to include all five                   |
| `customEndpoints`   | `CustomEndpoint[]`         | no       | Extra endpoints that don't fit the standard CRUD pattern                                     |
| `tokenRequired`     | `{ post?, put?, delete? }` | no       | Per-method auth override                                                                     |

---

## UriParameter object

Defines a segment of the URI path (e.g. `{ID}`, `{NAME}`).

| Field          | Type                    | Required | Description                                                           |
| -------------- | ----------------------- | -------- | --------------------------------------------------------------------- |
| `name`         | `string`                | yes      | Parameter name                                                        |
| `type`         | `"integer" \| "string"` | yes      | Input type rendered in the UI                                         |
| `defaultValue` | `string \| number`      | yes      | Pre-filled value in the form                                          |
| `isPrimary`    | `boolean`               | no       | Marks the primary key parameter; required for GET-single, PUT, DELETE |

```json
"uriParameters": [
  { "name": "id", "type": "integer", "defaultValue": 1, "isPrimary": true }
]
```

---

## Schema object

A flat key/value map that describes the shape of the resource.

### Type notation

| Notation                  | Meaning                                                                                   |
| ------------------------- | ----------------------------------------------------------------------------------------- |
| `"string"`                | Required string                                                                           |
| `"string?"`               | Optional string (nullable)                                                                |
| `"integer"`               | Required integer                                                                          |
| `"boolean"`               | Boolean                                                                                   |
| `"timestamp"`             | Unix timestamp                                                                            |
| `"datetime"`              | ISO 8601 datetime                                                                         |
| `"double?"`               | Optional float                                                                            |
| `"foreignKey-{resource}"` | Nested object of the given resource — `{resource}` must match an existing `name.singular` |
| `"array-{resource}"`      | Array of the given resource — `{resource}` must match an existing `name.singular`         |
| `"VALUE_A \| VALUE_B"`    | Enum — one of the listed values                                                           |

---

## Endpoints object

Overrides description, body, error selection, and extra responses for any standard endpoint, without converting it to a `customEndpoint`.

```json
"endpoints": {
  "GET-list":   { ... },
  "GET-single": { ... },
  "POST":       { ... },
  "PUT":        { ... },
  "DELETE":     { ... }
}
```

Each key is optional. Only specify the endpoints you need to customize.

### EndpointOverride fields

| Field            | Type         | Required | Description                                                                                                                                                                          |
| ---------------- | ------------ | -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `description`    | `string`     | no       | Human-readable description of this endpoint, displayed in the UI                                                                                                                     |
| `body`           | `Body`       | no       | Request body definition — replaces the deprecated `body.post` / `body.put`                                                                                                           |
| `includedErrors` | `integer[]`  | no       | Subset of `defaultErrors` codes to apply to this endpoint. Omit to include all applicable defaults                                                                                   |
| `responses`      | `Response[]` | no       | Additional responses for this endpoint (success codes or extra errors not in `defaultErrors`). Integer shorthands are **not** allowed here — use `includedErrors` to select defaults |

### How errors are resolved for a standard endpoint

Given a method (e.g. `POST`):

1. Collect all `defaultErrors` whose `appliesTo` includes `"POST"`.
2. If the endpoint override has `includedErrors`, keep only default errors whose `code` is in that list.
3. Append any `responses` defined on the endpoint override.

| Scenario                                           | Result                                                  |
| -------------------------------------------------- | ------------------------------------------------------- |
| No `endpoints` key                                 | All applicable default errors, no extra responses       |
| `endpoints.POST` without `includedErrors`          | All applicable default errors + extra `responses`       |
| `endpoints.POST` with `includedErrors: [401, 500]` | Only 401 and 500 from defaults + extra `responses`      |
| `endpoints.POST` with `includedErrors: []`         | No default errors (suppressed) + extra `responses` only |

```json
"endpoints": {
  "POST": {
    "description": "Uploads an image. The file is automatically converted to WebP.",
    "includedErrors": [401, 500],
    "body": {
      "multipart": true,
      "fields": [
        { "name": "textAlternatif", "label": "Alt text", "type": "string", "required": true },
        { "name": "file", "label": "Image (JPEG, PNG — max 5 MB)", "type": "file", "required": true }
      ]
    },
    "responses": [
      { "code": 201, "response": { "id": "integer", "textAlternatif": "string", "url": "string" } },
      { "code": 413, "response": { "code": 413, "message": "File too large, maximum size is 5 MB" } },
      { "code": 415, "response": { "code": 415, "message": "Unsupported media type" } }
    ]
  }
}
```

---

## Body object

> **Deprecated** at the table level — prefer `endpoints.POST.body` and `endpoints.PUT.body`.  
> Still supported for backwards compatibility. Do not use for new tables.

### JSON body

| Field          | Type     | Required | Description                                                 |
| -------------- | -------- | -------- | ----------------------------------------------------------- |
| `schema`       | `object` | no       | Displayed as the body schema in the UI                      |
| `defaultValue` | `string` | no       | JSON string pre-filled in the textarea — must be valid JSON |

### Multipart body

| Field       | Type               | Required | Description                         |
| ----------- | ------------------ | -------- | ----------------------------------- |
| `multipart` | `true`             | yes      | Switches the UI to a multipart form |
| `fields`    | `MultipartField[]` | yes      | List of form fields                 |

#### MultipartField

| Field      | Type                 | Required | Description                    |
| ---------- | -------------------- | -------- | ------------------------------ |
| `name`     | `string`             | yes      | Field name sent in the request |
| `label`    | `string`             | yes      | Label displayed in the form    |
| `type`     | `"string" \| "file"` | yes      | Input type                     |
| `required` | `boolean`            | no       | Marks the field as required    |

---

## Custom responses

Override the auto-generated response shape for specific methods using the `response` key on the table.

Supported keys: `"get-single"`, `"get-list"`.

```json
"response": {
  "get-single": {
    "id": "integer",
    "title": "string",
    "authors": "array-user",
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

---

## List parameters (pagination & filters)

Query parameters available on **GET-list** endpoints.

```json
"listParameters": [
  { "name": "page",   "label": "Page",   "type": "integer", "defaultValue": 1,  "min": 1 },
  { "name": "limit",  "label": "Limit",  "type": "integer", "defaultValue": 20, "min": 1, "max": 100 },
  { "name": "status", "label": "Status", "type": "select",
    "options": [
      { "name": "DRAFT",     "label": "Draft"     },
      { "name": "PUBLISHED", "label": "Published" }
    ]
  }
]
```

---

## CustomEndpoint object

Use for endpoints that don't map to standard CRUD operations.

| Field             | Type                                   | Required | Description                                                                 |
| ----------------- | -------------------------------------- | -------- | --------------------------------------------------------------------------- |
| `name`            | `string`                               | yes      | Display name — must be unique within the table                              |
| `description`     | `string`                               | no       | Human-readable description                                                  |
| `uri`             | `string`                               | yes      | Path relative to the table `uri`                                            |
| `method`          | `"GET" \| "POST" \| "PUT" \| "DELETE"` | yes      | HTTP method                                                                 |
| `tokenRequired`   | `boolean`                              | no       | Defaults: `false` for GET, `true` for POST/PUT/DELETE                       |
| `responses`       | `Response[]`                           | no       | Expected HTTP responses — supports both full objects and integer shorthands |
| `body`            | `object`                               | no       | JSON body schema                                                            |
| `queryParameters` | `QueryParameter[]`                     | no       | URL query parameters                                                        |
| `uriParameters`   | `UriParameter[]`                       | no       | Additional path parameters                                                  |

### Response entries

Each entry in a `customEndpoint.responses` array can be either:

**A full response object:**

```json
{ "code": 200, "response": { "message": "Issue published" } }
```

Error responses (code ≥ 400) must include a `response` object with matching `code` and a non-empty `message`.

**An integer shorthand** referencing a `defaultErrors` entry by code:

```json
401
```

The linter verifies that the referenced code exists in `defaultErrors`. At runtime, the UI resolves the shorthand to the full `response` object declared there.

**Mixing both forms is allowed:**

```json
"responses": [
  { "code": 200, "response": { "message": "Issue published and newsletter sent" } },
  401,
  403,
  { "code": 404, "response": { "code": 404, "message": "Issue not found" } },
  { "code": 409, "response": { "code": 409, "message": "Issue is already published" } },
  500
]
```

> **Note** — Integer shorthands are only available in `customEndpoints.responses`. They are not allowed in `endpoints` overrides, where `includedErrors` serves the same purpose.

---

## Authentication defaults

| Method | `tokenRequired` default |
| ------ | ----------------------- |
| GET    | `false`                 |
| POST   | `true`                  |
| PUT    | `true`                  |
| DELETE | `true`                  |

Override per endpoint using `tokenRequired` on a `CustomEndpoint`, or per method using the table-level `tokenRequired` object:

```json
"tokenRequired": { "post": false }
```

---

## Full example

```json
{
  "defaultErrors": [
    {
      "code": 400,
      "response": { "code": 400, "message": "Invalid or missing fields" },
      "appliesTo": ["POST", "PUT"]
    },
    {
      "code": 401,
      "response": {
        "code": 401,
        "message": "Unauthorized, valid token required"
      },
      "appliesTo": ["POST", "PUT", "DELETE"]
    },
    {
      "code": 403,
      "response": {
        "code": 403,
        "message": "Forbidden, insufficient permissions"
      },
      "appliesTo": ["POST", "PUT", "DELETE"]
    },
    {
      "code": 404,
      "response": { "code": 404, "message": "Resource not found" },
      "appliesTo": ["GET-single", "PUT", "DELETE"]
    },
    {
      "code": 500,
      "response": { "code": 500, "message": "Internal server error" },
      "appliesTo": ["GET-list", "GET-single", "POST", "PUT", "DELETE"]
    }
  ],
  "tables": [
    {
      "name": { "singular": "post", "plural": "posts" },
      "description": "Posts published in the newsletter.",
      "uri": "/post",
      "uriParameters": [
        {
          "name": "id",
          "type": "integer",
          "defaultValue": 1,
          "isPrimary": true
        }
      ],
      "schema": {
        "id": "integer",
        "title": "string",
        "status": "DRAFT | PUBLISHED",
        "cover": "foreignKey-media"
      },
      "response": {
        "get-list": {
          "data": "array-post",
          "total": "integer",
          "page": "integer",
          "pageSize": "integer"
        }
      },
      "listParameters": [
        {
          "name": "page",
          "label": "Page",
          "type": "integer",
          "defaultValue": 1
        },
        {
          "name": "limit",
          "label": "Limit",
          "type": "integer",
          "defaultValue": 20
        }
      ],
      "includedEndpoints": ["GET-list", "GET-single", "POST", "PUT", "DELETE"],
      "endpoints": {
        "GET-list": { "description": "Returns the paginated list of posts." },
        "GET-single": { "description": "Returns a post by its ID." },
        "POST": {
          "description": "Creates a new post. The slug is auto-generated if omitted.",
          "body": {
            "schema": { "title": "string", "status": "DRAFT | PUBLISHED" },
            "defaultValue": "{\n\t\"title\": \"Hello\",\n\t\"status\": \"DRAFT\"\n}"
          },
          "responses": [
            {
              "code": 201,
              "response": {
                "id": "integer",
                "title": "string",
                "status": "DRAFT | PUBLISHED"
              }
            },
            {
              "code": 409,
              "response": { "code": 409, "message": "Slug already exists" }
            }
          ]
        },
        "PUT": {
          "description": "Updates an existing post.",
          "body": {
            "schema": { "title": "string", "status": "DRAFT | PUBLISHED" }
          },
          "responses": [
            {
              "code": 200,
              "response": {
                "id": "integer",
                "title": "string",
                "status": "DRAFT | PUBLISHED"
              }
            }
          ]
        },
        "DELETE": { "description": "Permanently deletes a post." }
      },
      "customEndpoints": [
        {
          "name": "publish",
          "description": "Sets the post status to PUBLISHED and sends the newsletter.",
          "uri": "/{ID}/publish",
          "method": "POST",
          "responses": [
            { "code": 200, "response": { "message": "Post published" } },
            401,
            403,
            {
              "code": 404,
              "response": { "code": 404, "message": "Post not found" }
            },
            {
              "code": 409,
              "response": { "code": 409, "message": "Post already published" }
            },
            500
          ]
        }
      ]
    }
  ]
}
```

---

## Checklist before adding a table

**Caught by the linter**

- [ ] `name.singular` is unique across all tables
- [ ] All `foreignKey-X` / `array-X` references point to an existing `name.singular`
- [ ] `includedEndpoints` contains only recognized values
- [ ] A `uriParameter` with `"isPrimary": true` exists when `GET-single`, `PUT` or `DELETE` are active
- [ ] `endpoints.POST.body` / `endpoints.PUT.body` `defaultValue` fields are valid JSON strings
- [ ] `endpoints.*.includedErrors` codes exist in `defaultErrors`
- [ ] Integer shorthands in `customEndpoints.responses` reference existing `defaultErrors` codes
- [ ] `select` query parameters have a non-empty `options` array
- [ ] Empty `{}` / `[]` fields are flagged

**Requires manual review**

- [ ] `endpoints.POST.body.schema` does not include auto-managed fields (`id`, `createdAt`, `updatedAt`…)
- [ ] `response["get-list"]` is defined if the list returns a paginated or reduced shape
- [ ] All `customEndpoints` declare their full `responses` array (using full objects and/or integer shorthands)
- [ ] `description` is filled on all `endpoints` keys and `customEndpoints`
