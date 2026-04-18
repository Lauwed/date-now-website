const resolveDefaultErrors = (allDefaultErrors, method, includedErrors) => {
	if (!allDefaultErrors || allDefaultErrors.length === 0) return [];

	let applicable = allDefaultErrors.filter((e) => e.appliesTo.includes(method));

	if (includedErrors !== undefined) {
		applicable = applicable.filter((e) => includedErrors.includes(e.code));
	}

	return applicable.map((e) => ({ code: e.code, response: e.response }));
};

const resolveResponses = (responses, allDefaultErrors) => {
	if (!responses) return [];
	return responses
		.map((r) => {
			if (typeof r === "number") {
				const found = allDefaultErrors?.find((e) => e.code === r);
				return found ? { code: found.code, response: found.response } : null;
			}
			return r;
		})
		.filter(Boolean);
};

const getParametersURIArr = (data) => {
	const uriParameters = [];
	data.forEach((key) => {
		uriParameters.push(key);
	});
	return uriParameters;
};

const generateGETListEndpoint = (data, defaultErrors) => {
	const override = data.endpoints?.["GET-list"];
	const description = override?.description ?? data.description;

	const resolvedDefaults = resolveDefaultErrors(
		defaultErrors,
		"GET-list",
		override?.includedErrors,
	);
	const extraResponses = resolveResponses(override?.responses, defaultErrors);

	const has200 = extraResponses.some((r) => r.code === 200);
	const successResponse = has200
		? []
		: [{ code: 200, response: data.response?.["get-list"] ?? [data.schema] }];

	return {
		method: "GET",
		name: `${data.name.plural} List`,
		description,
		queryParameters: data.listParameters ?? [],
		responses: [...successResponse, ...extraResponses, ...resolvedDefaults],
		uri: data.uri,
		uriParameters: getParametersURIArr(data.uriParameters ?? [])
			.filter((p) => !p.isPrimary)
			.map((key) => ({
				defaultValue: key.defaultValue,
				name: `${key.name}`,
				parameter: `{${key.name.toUpperCase()}}`,
				type: key.type,
			})),
	};
};

const generateGETSingleEndpoint = (data, defaultErrors) => {
	const override = data.endpoints?.["GET-single"];
	const description = override?.description ?? data.description;

	const resolvedDefaults = resolveDefaultErrors(
		defaultErrors,
		"GET-single",
		override?.includedErrors,
	);
	const extraResponses = resolveResponses(override?.responses, defaultErrors);

	const has200 = extraResponses.some((r) => r.code === 200);
	const successResponse = has200
		? []
		: [
				{
					code: 200,
					response: data.response?.["get-single"] ?? data.schema,
				},
			];

	return {
		method: "GET",
		name: `${data.name.singular}`,
		description,
		responses: [...successResponse, ...extraResponses, ...resolvedDefaults],
		tokenRequired: false,
		uri: `${data.uri}/{${data.uriParameters.find((p) => p.isPrimary)?.name.toUpperCase()}}`,
		uriParameters: getParametersURIArr(data.uriParameters).map((key) => ({
			defaultValue: key.defaultValue,
			name: `${data.name.singular} ${key.name}`,
			parameter: `{${key.name.toUpperCase()}}`,
			type: key.type,
		})),
	};
};

const generatePOSTEndpoint = (data, defaultErrors) => {
	const override = data.endpoints?.POST;
	const description = override?.description ?? data.description;

	const bodyDef = override?.body ?? data.body?.post;
	const isMultipart = bodyDef?.multipart ?? false;

	const resolvedDefaults = resolveDefaultErrors(
		defaultErrors,
		"POST",
		override?.includedErrors,
	);
	const extraResponses = resolveResponses(override?.responses, defaultErrors);

	return {
		body: isMultipart ? null : (bodyDef?.schema ?? null),
		defaultBody: bodyDef?.defaultValue,
		description,
		multipart: isMultipart,
		multipartFields: bodyDef?.fields ?? [],
		method: "POST",
		name: `Add ${data.name.singular}`,
		responses: [...extraResponses, ...resolvedDefaults],
		tokenRequired: data.tokenRequired?.post ?? true,
		uri: data.uri,
		uriParameters: getParametersURIArr(data.uriParameters ?? [])
			.filter((p) => !p.isPrimary)
			.map((key) => ({
				defaultValue: key.defaultValue,
				name: `${key.name}`,
				parameter: `{${key.name.toUpperCase()}}`,
				type: key.type,
			})),
	};
};

const generatePUTEndpoint = (data, defaultErrors) => {
	const override = data.endpoints?.PUT;
	const description = override?.description ?? data.description;

	const bodyDef = override?.body ?? data.body?.put;
	const isMultipart = bodyDef?.multipart ?? false;

	const resolvedDefaults = resolveDefaultErrors(
		defaultErrors,
		"PUT",
		override?.includedErrors,
	);
	const extraResponses = resolveResponses(override?.responses, defaultErrors);

	return {
		body: isMultipart ? null : (bodyDef?.schema ?? null),
		defaultBody: bodyDef?.defaultValue,
		description,
		multipart: isMultipart,
		multipartFields: bodyDef?.fields ?? [],
		method: "PUT",
		name: `Edit ${data.name.singular}`,
		responses: [...extraResponses, ...resolvedDefaults],
		tokenRequired: data.tokenRequired?.put ?? true,
		uri: `${data.uri}/{${data.uriParameters.find((p) => p.isPrimary)?.name.toUpperCase()}}`,
		uriParameters: getParametersURIArr(data.uriParameters).map((key) => ({
			defaultValue: key.defaultValue,
			name: `${data.name.singular} ${key.name}`,
			parameter: `{${key.name.toUpperCase()}}`,
			type: key.type,
		})),
	};
};

const generateDELETEEndpoint = (data, defaultErrors) => {
	const override = data.endpoints?.DELETE;
	const description = override?.description ?? data.description;

	const resolvedDefaults = resolveDefaultErrors(
		defaultErrors,
		"DELETE",
		override?.includedErrors,
	);
	const extraResponses = resolveResponses(override?.responses, defaultErrors);

	return {
		method: "DELETE",
		name: `Delete ${data.name.singular}`,
		description,
		responses: [...extraResponses, ...resolvedDefaults],
		tokenRequired: data.tokenRequired?.delete ?? true,
		uri: `${data.uri}/{${data.uriParameters.find((p) => p.isPrimary)?.name.toUpperCase()}}`,
		uriParameters: getParametersURIArr(data.uriParameters).map((key) => ({
			defaultValue: key.defaultValue,
			name: `${data.name.singular} ${key.name}`,
			parameter: `{${key.name.toUpperCase()}}`,
			type: key.type,
		})),
	};
};

const generateCustomEndpoint = (e, uri, defaultErrors) => {
	let tokenRequired = e.tokenRequired;

	if (tokenRequired === undefined) {
		switch (e.method) {
			case "POST":
			case "PUT":
			case "DELETE":
				tokenRequired = true;
				break;
			default:
				tokenRequired = false;
		}
	}

	return {
		method: e.method,
		description: e.description,
		name: e.name,
		body: e.body,
		responses: resolveResponses(e.responses ?? [], defaultErrors),
		tokenRequired,
		uri: `${uri}${e.uri}`,
		uriParameters: getParametersURIArr(e.uriParameters ?? []).map((key) => ({
			defaultValue: key.defaultValue,
			name: key.name,
			parameter: `{${key.name.toUpperCase()}}`,
			type: key.type,
		})),
		queryParameters: e.queryParameters,
	};
};

const endpointGenerators = {
	"GET-list": generateGETListEndpoint,
	"GET-single": generateGETSingleEndpoint,
	POST: generatePOSTEndpoint,
	PUT: generatePUTEndpoint,
	DELETE: generateDELETEEndpoint,
};

const generateTablesEndpoints = (data) => {
	const tables = [];
	const defaultErrors = data.defaultErrors ?? [];

	data.tables.forEach((el) => {
		const table = {
			name: el.name.plural,
			endpoints: [],
		};

		let includedEndpointsGenerators = Object.values(endpointGenerators);

		if (el.includedEndpoints !== undefined) {
			includedEndpointsGenerators = [];

			el.includedEndpoints.forEach((i) => {
				includedEndpointsGenerators.push(endpointGenerators[i]);
			});
		}

		const includedEndpoints = includedEndpointsGenerators.map((g) =>
			g(el, defaultErrors),
		);

		const norm = (uri) => uri.toLowerCase();

		const endpointByKey = new Map();
		includedEndpoints.forEach((e) => {
			endpointByKey.set(`${e.method}:${norm(e.uri)}`, e);
		});

		const primaryParam = el.uriParameters?.find((p) => p.isPrimary);
		const pivotSuffix = primaryParam
			? `/${norm(`{${primaryParam.name}}`)}`
			: null;

		if (el.customEndpoints !== undefined && el.customEndpoints.length > 0) {
			el.customEndpoints.forEach((i) => {
				const resolvedUri = `${el.uri}${i.uri}`;
				const key = `${i.method}:${norm(resolvedUri)}`;
				const existing = endpointByKey.get(key);

				if (existing) {
					existing.responses = resolveResponses(
						i.responses ?? [],
						defaultErrors,
					);
					if (i.description) existing.description = i.description;
					if (i.tokenRequired !== undefined)
						existing.tokenRequired = i.tokenRequired;
				} else {
					const standardKey = `${i.method}:${norm(el.uri)}`;
					const standard = endpointByKey.get(standardKey);
					const isPivotReplacement =
						pivotSuffix !== null && key === standardKey + pivotSuffix;

					if (standard && isPivotReplacement) {
						const custom = generateCustomEndpoint(i, el.uri, defaultErrors);
						includedEndpoints[includedEndpoints.indexOf(standard)] = custom;
						endpointByKey.delete(standardKey);
						endpointByKey.set(key, custom);
					} else {
						includedEndpoints.push(
							generateCustomEndpoint(i, el.uri, defaultErrors),
						);
					}
				}
			});
		}

		includedEndpoints.forEach((e) => {
			table.endpoints.push(e);
		});

		tables.push(table);
	});

	return tables;
};
