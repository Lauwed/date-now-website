let expandAllButton, collapseAllButton;
let authContainer, errorAuth;
let token;

window.addEventListener("load", async () => {
	const res = await fetch("/doc/api.json");
	const data = await res.json();

	console.log(data);

	const apiContainer = document.querySelector("#main");
	authContainer = apiContainer.querySelector("#auth");
	const endpointsContainer = apiContainer.querySelector("#endpoints");

	token = sessionStorage.getItem("token");
	// Init auth area
	displayAuthForm(authContainer);

	// Init endpoints
	const tables = generateTablesEndpoints(data.tables);
	appendCategories(endpointsContainer, tables);

	expandAllButton = document.querySelector("button#expand-all");
	expandAllButton.addEventListener("click", handleExpandAll);
	collapseAllButton = document.querySelector("button#collapse-all");
	collapseAllButton.addEventListener("click", handleCollapseAll);
});

const displayAuthForm = () => {
	const isUserLogged = token && token.length > 0;

	const title = '<h2 class="auth__title">Authentication</h2>';
	const authForm = `<form action="" method="" class="auth__form">
				<label for="token">Token</label>
				<input id="token" required />
				<button type="submit">Login</button>
			</form>`;
	const logoutButton = "<button>Log out</button>";

	authContainer.innerHTML = title;

	if (!isUserLogged) {
		authContainer.innerHTML += authForm;

		const authFormNode = authContainer.querySelector("form");
		errorAuth = document.createElement("p");
		errorAuth.classList.add("auth__error", "alert", "alert--danger");
		authFormNode.appendChild(errorAuth);

		authFormNode.addEventListener("submit", handleAuthFormSubmit);
	} else {
		authContainer.innerHTML += logoutButton;

		const logoutButtonNode = authContainer.querySelector("button");
		logoutButtonNode.addEventListener("click", handleClickLogout);
	}
};

const handleClickLogout = () => {
	sessionStorage.removeItem("token");
	token = null;
	displayAuthForm();
};

const handleAuthFormSubmit = (e) => {
	e.preventDefault();

	errorAuth.innerHTML = "";
	token = e.target.querySelector("input").value;

	if (token && token.length > 0) {
		sessionStorage.setItem("token", token);
		displayAuthForm();
	} else {
		errorAuth.innerHTML = "Please provide a token.";
	}
};

const handleExpandAll = () => {
	document.querySelectorAll("details").forEach((d) => {
		d.setAttribute("open", true);
	});
};

const handleCollapseAll = () => {
	document.querySelectorAll("details").forEach((d) => {
		d.removeAttribute("open");
	});
};

const getFormControl = (label, inputOpts, help = null) => {
	let {
		id,
		defaultValue,
		placeholder,
		type,
		min,
		max,
		onChange,
		required,
		options,
	} = inputOpts;

	if (type === undefined) {
		type = "text";
	}

	const container = document.createElement("div");
	container.classList.add("form-control");

	const labelNode = document.createElement("label");
	labelNode.classList.add("form-control__label");
	labelNode.innerHTML = label;
	labelNode.setAttribute("for", id);
	container.appendChild(labelNode);

	let input;
	if (type === "textarea") {
		input = document.createElement("textarea");
		input.innerHTML = defaultValue;
		if (required !== undefined && required === true)
			input.setAttribute("required", "true");
	} else if (type === "select") {
		input = document.createElement("select");

		if (options !== undefined && options.length > 0) {
			options.forEach((o) => {
				const option = document.createElement("option");
				option.value = o.name;
				option.innerHTML = o.label;

				input.appendChild(option);
			});
		} else {
			input.innerHTML = '<option name="null">No option available</option>';
		}
	} else {
		input = document.createElement("input");
		input.type = type;
		if (min !== undefined) input.setAttribute("min", min);
		if (max !== undefined) input.setAttribute("max", max);
		if (required !== undefined && required === true)
			input.setAttribute("required", "true");
		if (placeholder !== undefined)
			input.setAttribute("placeholder", placeholder);
		if (defaultValue !== undefined) {
			if (type === "checkbox" || type === "radio")
				input.checked = defaultValue;
			else input.value = defaultValue;
		}
	}

	input.id = id;
	input.classList.add("form-control__input");
	if (onChange !== undefined) {
		input.addEventListener("change", onChange);
	}

	if (help && help.length > 0) {
		const helpNode = document.createElement("p");
		helpNode.classList.add("form-control__help");
		helpNode.innerHTML = help;

		container.appendChild(helpNode);
	}

	container.appendChild(input);

	return container;
};

const appendFetchedEndpointData = (parentNode, uri, method, opts) => {
	const apiURI = `/api${uri}`;

	const fetchOpts = { method };
	if (opts !== undefined) {
		if ("body" in opts) {
			fetchOpts.body = opts.body;
		}
		if ("headers" in opts) {
			fetchOpts.headers = opts.headers;
		}
	}

	fetch(apiURI, fetchOpts)
		.then((r) => r.json())
		.then(
			(data) =>
				(parentNode.innerHTML = `<pre>${prettyPrintJson.toHtml(data)}</pre>`),
		);
};

const getResponseNode = (response) => {
	const container = document.createElement("li");
	container.classList.add("response");

	const title = document.createElement("h4");
	title.classList.add("response__title");
	title.innerHTML = `${HTTP_RESPONSE_TITLE[response.code]} <span class="tag">${response.code}</span>`;
	container.appendChild(title);

	const dataContainer = document.createElement("code");
	dataContainer.innerHTML = `<pre>${prettyPrintJson.toHtml(response.response)}</pre>`;
	container.appendChild(dataContainer);

	return container;
};

const getUriParameterId = (parameter) => {
	return `uri-parameter-${parameter.replaceAll(/{|}/g, "")}`;
};

const getQueryParameterId = (parameter) => {
	return `query-parameter-${parameter.replaceAll(/{|}/g, "")}`;
};

const getUriParameterNode = (parameter) => {
	const label = `${parameter.name} <span class="tag">${parameter.parameter}</span> <span class="tag">${parameter.type}</span>`;
	const inputOpts = {
		type: INPUT_TYPE[parameter.type],
		defaultValue: parameter.defaultValue,
		id: getUriParameterId(parameter.parameter),
		required: true,
	};

	return getFormControl(label, inputOpts);
};

const getQueryParameterNode = (parameter) => {
	const label = `${parameter.label}${parameter.type !== "select" ? `<span class="tag">${parameter.type}</span>` : ""}`;
	const inputOpts = {
		type: INPUT_TYPE[parameter.type],
		defaultValue: parameter.defaultValue,
		id: getQueryParameterId(parameter.name),
		options: parameter.options,
		min: parameter.min,
		max: parameter.max,
	};

	return getFormControl(label, inputOpts);
};

const handleFetch = (
	uri,
	uriParameters,
	method,
	fetchContainer,
	dataContainer,
	queryParameters,
	tokenRequired,
) => {
	let updatedURI = uri;

	if (uriParameters !== undefined && uriParameters.length > 0) {
		uriParameters.forEach((parameter) => {
			const inputId = getUriParameterId(parameter.parameter);
			const input = fetchContainer.querySelector(`#${inputId}`);

			let inputValue;
			if (input.type === "checkbox" || input.type === "radio") {
				inputValue = input.checked;
			} else inputValue = input.value;

			const paramRegex = new RegExp(`${parameter.parameter}`);
			updatedURI = updatedURI.replace(paramRegex, inputValue);
		});
	}

	if (queryParameters !== undefined && queryParameters.length > 0) {
		updatedURI += "?";

		queryParameters.forEach((parameter, index) => {
			const inputId = getQueryParameterId(parameter.name);
			const input = fetchContainer.querySelector(`#${inputId}`);

			let inputValue;
			if (input.type === "checkbox" || input.type === "radio") {
				inputValue = input.checked;
			} else inputValue = input.value;

			if (
				inputValue !== "" &&
				inputValue !== undefined &&
				inputValue !== null &&
				inputValue !== "null"
			) {
				updatedURI += `${index !== 0 ? "&" : ""}${parameter.name}=${inputValue}`;
			}
		});
	}

	const headers = { "Content-Type": "application/json" };
	if (tokenRequired) {
		headers.Authorization = `Bearer ${token}`;
	}

	let body;
	if (method === "POST" || method === "PUT") {
		body = fetchContainer.querySelector("textarea").value || "";
	}

	appendFetchedEndpointData(dataContainer, updatedURI, method, {
		body,
		headers,
	});
};

const getEndpointNode = (endpoint) => {
	const {
		name,
		method,
		uri,
		tokenRequired,
		responses,
		uriParameters,
		defaultBody,
		body,
		queryParameters,
	} = endpoint;

	const container = document.createElement("details");
	container.classList.add("endpoint", `endpoint--${method}`);

	const header = document.createElement("summary");
	header.classList.add("endpoint__header");
	container.appendChild(header);

	const title = document.createElement("h3");
	title.innerHTML = name;
	header.appendChild(title);

	const methodTag = document.createElement("p");
	methodTag.innerHTML = method;
	methodTag.classList.add("tag");
	header.appendChild(methodTag);

	const uriTag = document.createElement("p");
	uriTag.innerHTML = uri;
	uriTag.classList.add("tag");
	header.appendChild(uriTag);

	if (tokenRequired) {
		const requiredTag = document.createElement("p");

		// SVG
		const shieldSVG = `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="lucide lucide-shield-ellipsis-icon lucide-shield-ellipsis"><path d="M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z"/><path d="M8 12h.01"/><path d="M12 12h.01"/><path d="M16 12h.01"/></svg>`;

		// Required text
		const requiredText = "<span>Required</span>";

		requiredTag.innerHTML = `${shieldSVG}${requiredText}`;
		requiredTag.classList.add("tag");
		header.appendChild(requiredTag);
	}

	const content = document.createElement("div");
	content.classList.add("endpoint__content");
	container.appendChild(content);

	const fetchContainer = document.createElement("form");
	fetchContainer.classList.add("endpoint__fetch-container");
	fetchContainer.addEventListener("submit", (e) => {
		e.preventDefault();
		handleFetch(
			uri,
			uriParameters,
			method,
			fetchContainer,
			dataContainer,
			queryParameters,
			tokenRequired,
		);
	});
	content.appendChild(fetchContainer);

	if (uriParameters !== undefined && uriParameters.length > 0) {
		const uriParametersContainer = document.createElement("div");
		uriParametersContainer.classList.add(
			"endpoint__uri-parameters-container",
		);
		fetchContainer.appendChild(uriParametersContainer);

		const uriParametersContainerTitle = document.createElement("h4");
		uriParametersContainerTitle.innerHTML = "URI Parameters";
		uriParametersContainer.appendChild(uriParametersContainerTitle);

		uriParameters.forEach((parameter) => {
			const node = getUriParameterNode(parameter);
			uriParametersContainer.appendChild(node);
		});
	}

	if (queryParameters !== undefined && queryParameters.length > 0) {
		const queryParametersContainer = document.createElement("div");
		queryParametersContainer.classList.add(
			"endpoint__query-parameters-container",
		);
		fetchContainer.appendChild(queryParametersContainer);

		const queryParametersContainerTitle = document.createElement("h4");
		queryParametersContainerTitle.innerHTML = "Query Parameters";
		queryParametersContainer.appendChild(queryParametersContainerTitle);

		queryParameters.forEach((parameter) => {
			const node = getQueryParameterNode(parameter);
			queryParametersContainer.appendChild(node);
		});
	}

	if (method === "POST" || method === "PUT") {
		const fetchBody = document.createElement("div");
		fetchBody.classList.add("endpoint__body-value");
		fetchContainer.appendChild(fetchBody);

		const label = `Body`;
		const inputOpts = {
			type: "textarea",
			defaultValue: defaultBody || "{}",
			id: `${name}-body`,
			required: true,
		};

		fetchBody.appendChild(getFormControl(label, inputOpts));
	}

	const fetchButton = document.createElement("button");
	fetchButton.setAttribute("type", "submit");
	fetchButton.innerHTML = "Fetch";
	fetchContainer.appendChild(fetchButton);

	const dataContainer = document.createElement("code");
	fetchContainer.appendChild(dataContainer);

	const schemasContainer = document.createElement("div");
	schemasContainer.classList.add("endpoint__schemas");
	content.appendChild(schemasContainer);

	if (method === "POST" || method === "PUT") {
		const bodyContainer = document.createElement("div");
		bodyContainer.classList.add("endpoint__body");
		schemasContainer.appendChild(bodyContainer);

		const bodyTitle = document.createElement("h4");
		bodyTitle.classList.add("endpoint__body__title");
		bodyTitle.innerHTML = "Body";
		bodyContainer.appendChild(bodyTitle);

		const bodySchema = document.createElement("code");
		bodySchema.classList.add("endpoint__body__schema");
		bodySchema.innerHTML = `<pre>${prettyPrintJson.toHtml(body)}</pre>`;
		bodyContainer.appendChild(bodySchema);
	}

	const responsesContainer = document.createElement("ul");
	responsesContainer.classList.add("endpoint__responses");
	schemasContainer.appendChild(responsesContainer);

	if (responses !== undefined && responses.length > 0) {
		responses.forEach((response) => {
			const node = getResponseNode(response);
			responsesContainer.appendChild(node);
		});
	} else {
		const empty = document.createElement("p");
		empty.innerHTML = "No responses set yet.";
		responsesContainer.appendChild(empty);
	}

	return container;
};

const getCategoryNode = (category) => {
	const section = document.createElement("section");
	section.classList.add("category");

	const title = document.createElement("h2");
	title.innerHTML = category.name;
	title.classList.add("category__title");
	section.appendChild(title);

	if (category.endpoints.length > 0) {
		category.endpoints.forEach((endpoint) => {
			const endpointNode = getEndpointNode(endpoint);

			section.appendChild(endpointNode);
		});
	} else {
		section.innerHTML += "<p>No endpoints yet.</p>";
	}

	return section;
};

const appendCategories = (parentNode, categories) => {
	categories.forEach((category) => {
		const categoryNode = getCategoryNode(category);

		parentNode.appendChild(categoryNode);
	});
};
