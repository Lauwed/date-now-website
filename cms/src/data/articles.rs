use reqwest::header::{AUTHORIZATION, CONTENT_TYPE, HeaderMap};
use serde::{Deserialize, Serialize};

use crate::data::responses::{Response, ResponseMessage};
use crate::g_config;

#[derive(Serialize, Deserialize, Default, Debug, Clone)]
#[serde(rename_all = "camelCase")]
pub struct Article {
    pub id: u32,
    pub section_id: u32,
    pub position: i32,
    pub title: String,
    pub source_name: String,
    pub source_url: String,
    /// Markdown body. Embeds use the `::youtube[url]` / `::tweet[url]` syntax.
    pub summary: String,
}

/// Body of both POST and PUT — the API requires all four fields on each.
#[derive(Serialize, Debug, Clone, Default)]
#[serde(rename_all = "camelCase")]
pub struct ArticlePayload {
    pub title: String,
    pub source_name: String,
    pub source_url: String,
    pub summary: String,
}

impl From<&Article> for ArticlePayload {
    fn from(article: &Article) -> Self {
        Self {
            title: article.title.clone(),
            source_name: article.source_name.clone(),
            source_url: article.source_url.clone(),
            summary: article.summary.clone(),
        }
    }
}

fn articles_url(issue_id: u32, section_id: u32) -> String {
    let config = g_config();
    format!(
        "{}/api/issue/{}/section/{}/article",
        config.api_url, issue_id, section_id
    )
}

fn auth_headers(token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());
    headers
}

fn parse<T: serde::de::DeserializeOwned>(res: Result<reqwest::blocking::Response, reqwest::Error>) -> Result<T, String> {
    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<T>>(&text) {
                Ok(Response::Success(value)) => Ok(value),
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

pub fn create_article(
    issue_id: u32,
    section_id: u32,
    payload: ArticlePayload,
    token: String,
) -> Result<Article, String> {
    let json = serde_json::to_string(&payload).unwrap();

    let client = reqwest::blocking::Client::new();
    let res = client
        .post(articles_url(issue_id, section_id))
        .headers(auth_headers(&token))
        .body(json)
        .send();

    parse(res)
}

pub fn update_article(
    issue_id: u32,
    section_id: u32,
    article_id: u32,
    payload: ArticlePayload,
    token: String,
) -> Result<Article, String> {
    let json = serde_json::to_string(&payload).unwrap();

    let client = reqwest::blocking::Client::new();
    let res = client
        .put(format!(
            "{}/{}",
            articles_url(issue_id, section_id),
            article_id
        ))
        .headers(auth_headers(&token))
        .body(json)
        .send();

    parse(res)
}

pub fn delete_article(
    issue_id: u32,
    section_id: u32,
    article_id: u32,
    token: String,
) -> Result<ResponseMessage, String> {
    let client = reqwest::blocking::Client::new();
    let res = client
        .delete(format!(
            "{}/{}",
            articles_url(issue_id, section_id),
            article_id
        ))
        .headers(auth_headers(&token))
        .send();

    parse(res)
}

/// `order` must list every article id of the section, in the wanted order —
/// a partial list is rejected with a 400 by the API.
pub fn reorder_articles(
    issue_id: u32,
    section_id: u32,
    order: Vec<u32>,
    token: String,
) -> Result<ResponseMessage, String> {
    let json = serde_json::json!({ "order": order }).to_string();

    let client = reqwest::blocking::Client::new();
    let res = client
        .put(format!("{}/reorder", articles_url(issue_id, section_id)))
        .headers(auth_headers(&token))
        .body(json)
        .send();

    parse(res)
}
