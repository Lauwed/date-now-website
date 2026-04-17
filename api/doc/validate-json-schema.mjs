#!/usr/bin/env node

/**
 * validate-api.mjs
 * Linter for api.json — no dependencies, pure Node.js
 * Usage: node validate-api.mjs [path/to/api.json]
 */

import { readFileSync } from "fs";
import { resolve } from "path";

// ─── Config ──────────────────────────────────────────────────────────────────

const filePath = resolve(process.argv[2] ?? "api.json");

const VALID_INCLUDED_ENDPOINTS = [
	"GET-list",
	"GET-single",
	"POST",
	"PUT",
	"DELETE",
];
const VALID_METHODS = ["GET", "POST", "PUT", "DELETE"];
const VALID_PARAM_TYPES = ["integer", "string"];
const VALID_QUERY_TYPES = ["integer", "string", "select"];
const VALID_MULTIPART_TYPES = ["string", "file"];
const VALID_TYPE_PRIMITIVES = [
	"string",
	"string?",
	"integer",
	"integer?",
	"boolean",
	"boolean?",
	"timestamp",
	"timestamp?",
	"datetime",
	"datetime?",
	"double",
	"double?",
];

// Endpoints that require a primary uriParameter to build the URI
const ENDPOINTS_NEEDING_PRIMARY = ["GET-single", "PUT", "DELETE"];

// ─── Reporter ────────────────────────────────────────────────────────────────

let errors = 0;
let warnings = 0;

function error(path, message) {
	console.error(`  ❌  [${path}] ${message}`);
	errors++;
}

function warn(path, message) {
	console.warn(`  ⚠️   [${path}] ${message}`);
	warnings++;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

function isValidTypeString(value) {
	if (VALID_TYPE_PRIMITIVES.includes(value)) return true;
	if (/^foreignKey-(.+)$/.test(value)) return true;
	if (/^array-(.+)$/.test(value)) return true;
	// Enum: "A | B | C"
	if (/^[A-Z_]+(\s*\|\s*[A-Z_]+)+$/.test(value)) return true;
	return false;
}

function extractResourceRef(value) {
	const fk = value.match(/^foreignKey-(.+)$/);
	if (fk) return fk[1];
	const arr = value.match(/^array-(.+)$/);
	if (arr) return arr[1];
	return null;
}

function isValidJson(str) {
	try {
		JSON.parse(str);
		return true;
	} catch {
		return false;
	}
}

// ─── Validators ──────────────────────────────────────────────────────────────

function validateUriParameters(
	params,
	path,
	{ needsPrimary = false } = {},
) {
	if (!Array.isArray(params)) {
		error(path, `uriParameters must be an array`);
		return;
	}

	const hasPrimary = params.some((p) => p.isPrimary === true);
	const primaryCount = params.filter((p) => p.isPrimary === true).length;

	if (needsPrimary && !hasPrimary) {
		error(
			path,
			`This table includes GET-single, PUT or DELETE but no uriParameter has "isPrimary": true`,
		);
	}
	if (primaryCount > 1) {
		error(
			path,
			`Only one uriParameter can have "isPrimary": true — found ${primaryCount}`,
		);
	}

	params.forEach((param, i) => {
		const p = `${path}.uriParameters[${i}]`;

		if (!param.name || typeof param.name !== "string") {
			error(p, `Missing or invalid "name"`);
		}
		if (!VALID_PARAM_TYPES.includes(param.type)) {
			error(
				p,
				`"type" must be one of: ${VALID_PARAM_TYPES.join(", ")} — got "${param.type}"`,
			);
		}
		if (param.defaultValue === undefined || param.defaultValue === null) {
			error(p, `Missing "defaultValue"`);
		}
	});
}

function validateSchemaObject(schema, path, knownTableNames) {
	if (
		typeof schema !== "object" ||
		Array.isArray(schema) ||
		schema === null
	) {
		error(path, `Schema must be a plain object`);
		return;
	}

	for (const [key, value] of Object.entries(schema)) {
		const p = `${path}.${key}`;

		if (typeof value !== "string") {
			error(p, `Type value must be a string — got ${typeof value}`);
			continue;
		}

		if (!isValidTypeString(value)) {
			error(p, `Unknown type notation "${value}"`);
			continue;
		}

		const ref = extractResourceRef(value);
		if (ref && !knownTableNames.has(ref)) {
			error(p, `Reference "${value}" points to unknown table "${ref}"`);
		}
	}
}

function validateQueryParameters(params, path) {
	if (!Array.isArray(params)) {
		error(path, `queryParameters must be an array`);
		return;
	}

	params.forEach((param, i) => {
		const p = `${path}[${i}]`;

		if (!param.name || typeof param.name !== "string")
			error(p, `Missing or invalid "name"`);
		if (!param.label || typeof param.label !== "string")
			error(p, `Missing or invalid "label"`);

		if (!VALID_QUERY_TYPES.includes(param.type)) {
			error(
				p,
				`"type" must be one of: ${VALID_QUERY_TYPES.join(", ")} — got "${param.type}"`,
			);
		}

		if (param.type === "select") {
			if (!Array.isArray(param.options) || param.options.length === 0) {
				error(p, `"select" type requires a non-empty "options" array`);
			} else {
				param.options.forEach((opt, j) => {
					if (!opt.name) error(`${p}.options[${j}]`, `Missing "name"`);
					if (!opt.label) error(`${p}.options[${j}]`, `Missing "label"`);
				});
			}
		}

		if (
			param.min !== undefined &&
			param.max !== undefined &&
			param.min > param.max
		) {
			error(
				p,
				`"min" (${param.min}) cannot be greater than "max" (${param.max})`,
			);
		}
	});
}

function validateBody(body, path, knownTableNames) {
	if (typeof body !== "object" || Array.isArray(body) || body === null) {
		error(path, `body must be a plain object`);
		return;
	}

	const allowedKeys = ["post", "put"];

	for (const key of Object.keys(body)) {
		if (!allowedKeys.includes(key)) {
			warn(
				path,
				`Unexpected key "${key}" in body — expected "post" or "put"`,
			);
		}
	}

	for (const method of allowedKeys) {
		if (!body[method]) continue;

		const b = body[method];
		const p = `${path}.${method}`;

		if (b.multipart === true) {
			if (!Array.isArray(b.fields) || b.fields.length === 0) {
				error(p, `Multipart body requires a non-empty "fields" array`);
			} else {
				b.fields.forEach((field, i) => {
					const fp = `${p}.fields[${i}]`;
					if (!field.name) error(fp, `Missing "name"`);
					if (!field.label) error(fp, `Missing "label"`);
					if (!VALID_MULTIPART_TYPES.includes(field.type)) {
						error(
							fp,
							`"type" must be one of: ${VALID_MULTIPART_TYPES.join(", ")} — got "${field.type}"`,
						);
					}
				});
			}
		} else {
			if (b.schema) {
				validateSchemaObject(b.schema, `${p}.schema`, knownTableNames);
			}
			if (b.defaultValue !== undefined) {
				if (typeof b.defaultValue !== "string") {
					error(p, `"defaultValue" must be a JSON string`);
				} else if (!isValidJson(b.defaultValue)) {
					error(p, `"defaultValue" is not valid JSON`);
				}
			}
		}
	}
}

function validateCustomEndpoints(endpoints, path) {
	if (!Array.isArray(endpoints)) {
		error(path, `customEndpoints must be an array`);
		return;
	}

	const names = endpoints.map((e) => e.name).filter(Boolean);
	const duplicateNames = names.filter((n, i) => names.indexOf(n) !== i);

	if (duplicateNames.length > 0) {
		error(
			path,
			`Duplicate customEndpoint names: ${[...new Set(duplicateNames)].join(", ")}`,
		);
	}

	endpoints.forEach((endpoint, i) => {
		const p = `${path}[${i}] ("${endpoint.name ?? "unnamed"}")`;

		if (!endpoint.name || typeof endpoint.name !== "string") {
			error(p, `Missing or invalid "name"`);
		}
		if (!endpoint.uri || typeof endpoint.uri !== "string") {
			error(p, `Missing or invalid "uri"`);
		}
		if (!VALID_METHODS.includes(endpoint.method)) {
			error(
				p,
				`"method" must be one of: ${VALID_METHODS.join(", ")} — got "${endpoint.method}"`,
			);
		}

		if (endpoint.responses !== undefined) {
			if (!Array.isArray(endpoint.responses)) {
				error(p, `"responses" must be an array`);
			} else {
				endpoint.responses.forEach((r, j) => {
					if (typeof r.code !== "number") {
						error(
							`${p}.responses[${j}]`,
							`Missing or invalid "code" (must be a number)`,
						);
					}
				});
			}
		}

		if (endpoint.queryParameters) {
			validateQueryParameters(
				endpoint.queryParameters,
				`${p}.queryParameters`,
			);
		}
		if (endpoint.uriParameters) {
			validateUriParameters(endpoint.uriParameters, p);
		}
		if (endpoint.body !== undefined) {
			if (
				typeof endpoint.body !== "object" ||
				Array.isArray(endpoint.body)
			) {
				error(p, `"body" must be a plain object`);
			}
		}
	});
}

function validateTable(table, index, knownTableNames) {
	const label = table?.name?.singular ?? `tables[${index}]`;
	const path = `tables[${index}] ("${label}")`;

	// name
	if (!table.name?.singular || !table.name?.plural) {
		error(path, `"name" must have "singular" and "plural" fields`);
	}

	// uri
	if (!table.uri || typeof table.uri !== "string") {
		error(path, `Missing or invalid "uri"`);
	}

	// includedEndpoints
	const included = table.includedEndpoints;
	if (included !== undefined) {
		if (!Array.isArray(included)) {
			error(path, `"includedEndpoints" must be an array`);
		} else {
			included.forEach((ep) => {
				if (!VALID_INCLUDED_ENDPOINTS.includes(ep)) {
					error(
						path,
						`Unknown includedEndpoint "${ep}" — valid values: ${VALID_INCLUDED_ENDPOINTS.join(", ")}`,
					);
				}
			});
		}
	}

	// uriParameters + primary key coherence
	const activeEndpoints = included ?? VALID_INCLUDED_ENDPOINTS;
	const needsPrimary = ENDPOINTS_NEEDING_PRIMARY.some((ep) =>
		activeEndpoints.includes(ep),
	);

	if (table.uriParameters !== undefined) {
		validateUriParameters(table.uriParameters, path, { needsPrimary });
	} else if (needsPrimary) {
		error(
			path,
			`This table includes GET-single, PUT or DELETE but "uriParameters" is missing`,
		);
	}

	// schema
	if (table.schema !== undefined) {
		if (Object.keys(table.schema).length === 0) {
			warn(
				path,
				`"schema" is an empty object — omit it if the table has no schema`,
			);
		} else {
			validateSchemaObject(
				table.schema,
				`${path}.schema`,
				knownTableNames,
			);
		}
	}

	// response overrides
	if (table.response !== undefined) {
		const allowedResponseKeys = ["get-single", "get-list"];
		for (const key of Object.keys(table.response)) {
			if (!allowedResponseKeys.includes(key)) {
				warn(
					`${path}.response`,
					`Unexpected key "${key}" — expected "get-single" or "get-list"`,
				);
			} else {
				validateSchemaObject(
					table.response[key],
					`${path}.response.${key}`,
					knownTableNames,
				);
			}
		}
	}

	// body
	if (table.body !== undefined) {
		if (Object.keys(table.body).length === 0) {
			warn(
				path,
				`"body" is an empty object — omit it if the table has no body`,
			);
		} else {
			validateBody(table.body, `${path}.body`, knownTableNames);
		}
	}

	// listParameters
	if (table.listParameters !== undefined) {
		validateQueryParameters(
			table.listParameters,
			`${path}.listParameters`,
		);
	}

	// customEndpoints
	if (table.customEndpoints !== undefined) {
		validateCustomEndpoints(
			table.customEndpoints,
			`${path}.customEndpoints`,
			knownTableNames,
		);
	}

	// tokenRequired
	if (table.tokenRequired !== undefined) {
		const validTokenKeys = ["post", "put", "delete"];
		for (const key of Object.keys(table.tokenRequired)) {
			if (!validTokenKeys.includes(key)) {
				warn(
					`${path}.tokenRequired`,
					`Unexpected key "${key}" — expected "post", "put" or "delete"`,
				);
			}
			if (typeof table.tokenRequired[key] !== "boolean") {
				error(`${path}.tokenRequired.${key}`, `Value must be a boolean`);
			}
		}
	}
}

// ─── Main ────────────────────────────────────────────────────────────────────

console.log(`\nValidating ${filePath}\n`);

let raw;
try {
	raw = readFileSync(filePath, "utf-8");
} catch {
	console.error(`Cannot read file: ${filePath}`);
	process.exit(1);
}

let json;
try {
	json = JSON.parse(raw);
} catch (e) {
	console.error(`Invalid JSON: ${e.message}`);
	process.exit(1);
}

if (!json.tables || !Array.isArray(json.tables)) {
	console.error(`  ❌  Root "tables" key is missing or not an array`);
	process.exit(1);
}

// Build the set of known table names for cross-reference validation
const knownTableNames = new Set(
	json.tables.map((t) => t?.name?.singular).filter(Boolean),
);

// Check for duplicate singular names across all tables
const allSingulars = json.tables
	.map((t) => t?.name?.singular)
	.filter(Boolean);
const duplicates = allSingulars.filter(
	(n, i) => allSingulars.indexOf(n) !== i,
);
if (duplicates.length > 0) {
	error(
		"tables",
		`Duplicate table name.singular: ${[...new Set(duplicates)].join(", ")}`,
	);
}

// Validate each table
json.tables.forEach((table, i) => {
	validateTable(table, i, knownTableNames);
});

// ─── Summary ─────────────────────────────────────────────────────────────────

console.log("");
if (errors === 0 && warnings === 0) {
	console.log("  ✅  All checks passed\n");
	process.exit(0);
} else {
	if (warnings > 0) console.warn(`  ${warnings} warning(s)`);
	if (errors > 0) console.error(`  ${errors} error(s)\n`);
	process.exit(errors > 0 ? 1 : 0);
}
