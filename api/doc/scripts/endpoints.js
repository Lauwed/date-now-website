const getParametersURIArr = (data) => {
        const uriParameters = [];

               data.uriParameters.forEach(key => {
                       uriParameters.push(key);
               });

        return uriParameters;
}

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
                uriParameters: 
                        getParametersURIArr(data).filter(p => !p.isPrimary).map(key => ({
                                defaultValue: key.defaultValue,
                                name: `${key.name}`,
                                parameter: `{${key.name.toUpperCase()}}`,
                                type: key.type,
                        }))
        };
};

const generateGETSingleEndpoint = (data) => {
        return {
                method: "GET",
                name: `${data.name.singular}`,
                responses: [
                        {
                                code: 200,
                                response:
                                        data.response?.["get-single"] ??
                                        data.schema,
                        },
                ],
                uri: `${data.uri}/{${data.uriParameters.find(p => p.isPrimary).name.toUpperCase()}}`,
                uriParameters: 
                        getParametersURIArr(data).map(key => ({

                                defaultValue: key.defaultValue,
                                name: `${data.name.singular} ${key.name}`,
                                parameter: `{${key.name.toUpperCase()}}`,
                                type: key.type,
                        }))
        };
};

const generatePOSTEndpoint = (data) => {
        return {
                body: data.body?.post?.schema ?? data.schema,
                defaultBody: data.body?.post?.defaultValue,
                method: "POST",
                name: `Add ${data.name.singular}`,
                responses: [
                        {
                                code: 201,
                                response: `${data.name.singular} has been successfully created`,
                        },
                ],
                uri: data.uri,
                uriParameters: 
                        getParametersURIArr(data).filter(p => !p.isPrimary).map(key => ({
                                defaultValue: key.defaultValue,
                                name: `${key.name}`,
                                parameter: `{${key.name.toUpperCase()}}`,
                                type: key.type,
                        }))
        };
};

const generatePUTEndpoint = (data) => {
        return {
                body: data.body?.put?.schema ?? data.schema,
                defaultBody: data.body?.put.defaultValue,
                method: "PUT",
                name: `Edit ${data.name.singular}`,
                responses: [
                        {
                                code: 200,
                                response: `${data.name.singular} has been successfully edited`,
                        },
                ],
                uri: `${data.uri}/{${data.uriParameters.find(p => p.isPrimary).name.toUpperCase()}}`,
                uriParameters: 
                        getParametersURIArr(data).map(key => ({

                                defaultValue: key.defaultValue,
                                name: `${data.name.singular} ${key.name}`,
                                parameter: `{${key.name.toUpperCase()}}`,
                                type: key.type,
                        }))
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
                uri: `${data.uri}/{${data.uriParameters.find(p => p.isPrimary).name.toUpperCase()}}`,
                uriParameters: 
                        getParametersURIArr(data).map(key => ({
                                defaultValue: key.defaultValue,
                                name: `${data.name.singular} ${key.name}`,
                                parameter: `{${key.name.toUpperCase()}}`,
                                type: key.type,
                        }))
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

                let includedEndpoints = Object.values(endpointGenerators);

                if (
                        el.includedEndpoints !== undefined &&
                        el.includedEndpoints.length > 0
                ) {
                        includedEndpoints = [];

                        console.log(el.includedEndpoints);
                        el.includedEndpoints.forEach((i) => {
                                includedEndpoints.push(endpointGenerators[i]);
                        });
                }

                includedEndpoints.forEach((fct) => {
                        table.endpoints.push(fct(el));
                });

                console.log(table);

                tables.push(table);
        });

        return tables;
};
