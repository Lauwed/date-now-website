use reqwest::header::{AUTHORIZATION, CONTENT_TYPE, HeaderMap};
use serde::Serialize;

use crate::data::responses::{Response, ResponseMessage};
use crate::g_config;

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct IssueTag {
    tag_name: String,
    issue_id: u32,
}

pub fn add_issue_tag(
    issue_id: u32,
    tag_name: String,
    token: String,
) -> Result<ResponseMessage, String> {
    let config = g_config();
    let url = format!("{}/api/issue/{}/tag", config.api_url, issue_id);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let body = IssueTag { tag_name, issue_id };
    let json = serde_json::to_string(&body).unwrap();

    let client = reqwest::blocking::Client::new();
    let res = client.post(url).headers(headers).body(json).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMessage>>(&text) {
                Ok(Response::Success(msg)) => Ok(msg),
                Ok(Response::Error(res_err)) => Err(format!(
                    "Error response: {} - {}",
                    res_err.code, res_err.message
                )),
                Err(json_parse_err) => Err(format!("Error parsing: {}", json_parse_err)),
            },
            Err(text_err) => Err(format!("Error parsing text response: {}", text_err)),
        },
        Err(req_err) => Err(format!("Error request: {}", req_err)),
    }
}

pub fn delete_issue_tag(
    issue_id: u32,
    tag_name: String,
    token: String,
) -> Result<ResponseMessage, String> {
    let config = g_config();
    let url = format!("{}/api/issue/{}/tag/{}", config.api_url, issue_id, tag_name);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());

    let client = reqwest::blocking::Client::new();
    let res = client.delete(url).headers(headers).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMessage>>(&text) {
                Ok(Response::Success(msg)) => Ok(msg),
                Ok(Response::Error(res_err)) => Err(format!(
                    "Error response: {} - {}",
                    res_err.code, res_err.message
                )),
                Err(json_parse_err) => Err(format!("Error parsing: {}", json_parse_err)),
            },
            Err(text_err) => Err(format!("Error parsing text response: {}", text_err)),
        },
        Err(req_err) => Err(format!("Error request: {}", req_err)),
    }
}
