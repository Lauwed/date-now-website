# API Documentation — `api.json` Schema Reference

This file documents the structure of `api.json`, which drives the interactive API documentation UI. Edit this file whenever endpoints are added, removed, or modified.

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
| `uriParameters` | `UriParameter[]` | yes | Path parameters, including the primary key |
| `schema` | `object` | yes | Shape of the resource returned by the API |
| `body` | `Body` | no | Request body definitions for POST and/or PUT |
| `response` | `object` | no | Override the default response shape per method (see [Custom responses](#custom-responses)) |
| `includedEndpoints` | `string[]` | no | Whitelist of auto-generated endpoints to include. Omit to include all five. |
| `customEndpoints` | `CustomEndpoint[]` | no | Extra endpoints that don't fit the standard CRUD pattern |
| `tokenRequired` | `{ post?, put?, delete? }` | no | Per-method auth override (defaults documented below) |

### `name`

```json
"name": { "singular": "issue", "plural": "issues" }
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
| `"foreignKey-{resource}"` | Nested object of the given resource (e.g. `"foreignKey-media"`) |
| `"array-{resource}"` | Array of the given resource (e.g. `"array-tag"`) |
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

### JSON body (default)

| Field | Type | Required | Description |
|---|---|---|---|
| `schema` | `object` | no | Displayed as the body schema in the UI. Falls back to the table `schema` if absent. |
| `defaultValue` | `string` | no | JSON string pre-filled in the textarea |

```json
"post": {
  "schema": {
    "title": "string",
    "status": "DRAFT | PUBLISHED | ARCHIVED"
  },
  "defaultValue": "{\n\t\"title\": \"My issue\",\n\t\"status\": \"DRAFT\"\n}"
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
    { "name": "textAlternatif", "label": "Alt text", "type": "string", "required": true },
    { "name": "file", "label": "Image (JPEG, PNG — max 5 MB)", "type": "file", "required": true }
  ]
}
```

---

## Custom responses

Override the auto-generated response shape for a specific method using the `response` key on the table.

Currently supported key: `"get-single"`.

```json
"response": {
  "get-single": {
    "id": "integer",
    "title": "string",
    "authors": "array-author",
    "tags": "array-tag"
  }
}
```

If `response["get-single"]` is absent, the GET-single endpoint falls back to `schema`.

---

## CustomEndpoint object

Use for endpoints that don't map to standard CRUD operations (e.g. `/auth/login`, `/auth/subscribe`).

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | `string` | yes | Display name |
| `uri` | `string` | yes | Path **relative to the table `uri`** (e.g. `"/login"` on table `/auth` → `/auth/login`) |
| `method` | `"GET" \| "POST" \| "PUT" \| "DELETE"` | yes | HTTP method |
| `tokenRequired` | `boolean` | no | Defaults: `false` for GET, `true` for POST/PUT/DELETE |
| `responses` | `Response[]` | no | Expected HTTP responses |
| `body` | `object` | no | JSON body schema (displayed as-is) |
| `queryParameters` | `QueryParameter[]` | no | URL query parameters |
| `uriParameters` | `UriParameter[]` | no | Additional path parameters |

### Response object

```json
{ "code": 200, "response": "Email sent" }
```

`response` can be a string, an object, or an array.

### QueryParameter object

| Field | Type | Required | Description |
|---|---|---|---|
| `name` | `string` | yes | Query string key |
| `label` | `string` | yes | Form label |
| `type` | `"string" \| "integer" \| "select"` | yes | Input type |
| `required` | `boolean` | no | Whether the parameter is mandatory |
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

Set `tokenRequired` explicitly on a `CustomEndpoint` or via the table-level `tokenRequired` object to override.

---

## Full minimal example

```json
{
  "tables": [
    {
      "name": { "singular": "post", "plural": "posts" },
      "uri": "/post",
      "uriParameters": [
        { "name": "id", "type": "integer", "defaultValue": 1, "isPrimary": true }
      ],
      "schema": {
        "id": "integer",
        "title": "string",
        "status": "DRAFT | PUBLISHED"
      },
      "body": {
        "post": {
          "schema": { "title": "string", "status": "DRAFT | PUBLISHED" },
          "defaultValue": "{\n\t\"title\": \"Hello\",\n\t\"status\": \"DRAFT\"\n}"
        },
        "put": {
          "schema": { "title": "string", "status": "DRAFT | PUBLISHED" }
        }
      },
      "includedEndpoints": ["GET-list", "GET-single", "POST", "PUT", "DELETE"]
    }
  ]
}
```
