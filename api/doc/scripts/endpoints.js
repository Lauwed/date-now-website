const getParametersURIArr = (data) => {
	const uriParameters = [];

	data.forEach((key) => {
		uriParameters.push(key);
	});

	return uriParameters;
};

const generateGETListEndpoint = (data) => {
	return {
		method: "GET",
		name: `${data.name.plural} List`,
		queryParameters: [
			{ label: "Query", name: "q", type: "string" },
			{
				label: "Sorting",
				name: "sort",
				type: "select",
				options: [
					{
						name: null,
						label: "No sorting",
					},
					{
						name: "asc",
						label: "Ascending",
					},
					{
						name: "desc",
						label: "Descending",
					},
				],
			},
			{
				label: "Page Index",
				name: "page",
				type: "integer",
				min: 1,
			},
			{
				label: "Page Size",
				name: "page_size",
				type: "integer",
				min: 1,
			},
		],
		responses: [
			{
				code: 200,
				response: [data.schema],
			},
		],
		uri: data.uri,
		uriParameters: getParametersURIArr(data.uriParameters)
			.filter((p) => !p.isPrimary)
			.map((key) => ({
				defaultValue: key.defaultValue,
				name: `${key.name}`,
				parameter: `{${key.name.toUpperCase()}}`,
				type: key.type,
			})),
	};
};

const generateGETSingleEndpoint = (data) => {
	return {
		method: "GET",
		name: `${data.name.singular}`,
		responses: [
			{
				code: 200,
				response: data.response?.["get-single"] ?? data.schema,
			},
		],
		tokenRequired: data.tokenRequired?.post ?? false,
		uri: `${data.uri}/{${data.uriParameters.find((p) => p.isPrimary)?.name.toUpperCase()}}`,
		uriParameters: getParametersURIArr(data.uriParameters).map((key) => ({
			defaultValue: key.defaultValue,
			name: `${data.name.singular} ${key.name}`,
			parameter: `{${key.name.toUpperCase()}}`,
			type: key.type,
		})),
	};
};

const generatePOSTEndpoint = (data) => {
	const isMultipart = data.body?.post?.multipart ?? false;
	return {
		body: isMultipart ? null : (data.body?.post?.schema ?? data.schema),
		defaultBody: data.body?.post?.defaultValue,
		multipart: isMultipart,
		multipartFields: data.body?.post?.fields ?? [],
		method: "POST",
		name: `Add ${data.name.singular}`,
		responses: [
			{
				code: 201,
				response: `${data.name.singular} has been successfully created`,
			},
		],
		tokenRequired: data.tokenRequired?.post ?? true,
		uri: data.uri,
		uriParameters: getParametersURIArr(data.uriParameters)
			.filter((p) => !p.isPrimary)
			.map((key) => ({
				defaultValue: key.defaultValue,
				name: `${key.name}`,
				parameter: `{${key.name.toUpperCase()}}`,
				type: key.type,
			})),
	};
};

const generatePUTEndpoint = (data) => {
	return {
		body: data.body?.put?.schema ?? data.schema,
		defaultBody: data.body?.put?.defaultValue,
		method: "PUT",
		name: `Edit ${data.name.singular}`,
		responses: [
			{
				code: 200,
				response: `${data.name.singular} has been successfully edited`,
			},
		],
		tokenRequired: data.tokenRequired?.post ?? true,
		uri: `${data.uri}/{${data.uriParameters.find((p) => p.isPrimary)?.name.toUpperCase()}}`,
		uriParameters: getParametersURIArr(data.uriParameters).map((key) => ({
			defaultValue: key.defaultValue,
			name: `${data.name.singular} ${key.name}`,
			parameter: `{${key.name.toUpperCase()}}`,
			type: key.type,
		})),
	};
};

const generateDELETEEndpoint = (data) => {
	return {
		method: "DELETE",
		name: `Delete ${data.name.singular}`,
		responses: [
			{
				code: 200,
				response: `${data.name.singular} has been successfully deleted`,
			},
		],
		tokenRequired: data.tokenRequired?.post ?? true,
		uri: `${data.uri}/{${data.uriParameters.find((p) => p.isPrimary)?.name.toUpperCase()}}`,
		uriParameters: getParametersURIArr(data.uriParameters).map((key) => ({
			defaultValue: key.defaultValue,
			name: `${data.name.singular} ${key.name}`,
			parameter: `{${key.name.toUpperCase()}}`,
			type: key.type,
		})),
	};
};

const generateCustomEndpoint = (e, uri) => {
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
		name: e.name,
		body: e.body,
		responses: e.responses ?? [],
		tokenRequired,
		uri: `${uri}${e.uri}`,
		uriParameters: getParametersURIArr(e.uriParameters ?? []).map(
			(key) => ({
				defaultValue: key.defaultValue,
				name: `${data.name.singular} ${key.name}`,
				parameter: `{${key.name.toUpperCase()}}`,
				type: key.type,
			}),
		),
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

	data.forEach((el) => {
		const table = {
			name: el.name.plural,
			endpoints: [],
		};

		let includedEnpointsGenerators = Object.values(endpointGenerators);

		if (el.includedEndpoints !== undefined) {
			includedEnpointsGenerators = [];

			el.includedEndpoints.forEach((i) => {
				includedEnpointsGenerators.push(endpointGenerators[i]);
			});
		}

		const includedEndpoints = includedEnpointsGenerators.map((g) => g(el));

		if (
			el.customEndpoints !== undefined &&
			el.customEndpoints.length > 0
		) {
			el.customEndpoints.forEach((i) => {
				includedEndpoints.push(generateCustomEndpoint(i, el.uri));
			});
		}

		includedEndpoints.forEach((e) => {
			table.endpoints.push(e);
		});

		tables.push(table);
	});

	return tables;
};
