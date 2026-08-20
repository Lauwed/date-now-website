use std::fmt;

use reqwest::header::{AUTHORIZATION, CONTENT_TYPE, HeaderMap};
use serde::{Deserialize, Serialize};

use crate::data::articles::Article;
use crate::data::responses::{Response, ResponseMessage};
use crate::g_config;

#[derive(Serialize, Deserialize, Default, Debug, Clone, PartialEq)]
#[serde(rename_all = "UPPERCASE")]
pub enum SectionType {
    #[default]
    Category,
    Text,
}

impl fmt::Display for SectionType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                SectionType::Category => String::from("Category"),
                SectionType::Text => String::from("Text"),
            }
        )
    }
}

#[derive(Serialize, Deserialize, Default, Debug, Clone)]
#[serde(rename_all = "camelCase")]
pub struct IssueSection {
    pub id: u32,
    pub issue_id: u32,
    pub position: i32,
    #[serde(rename = "type")]
    pub kind: SectionType,
    /// Set for CATEGORY sections only.
    pub category_name: Option<String>,
    /// Markdown body, set for TEXT sections only. Embeds use the
    /// `::youtube[url]` / `::tweet[url]` syntax.
    pub text_body: Option<String>,
    #[serde(default)]
    pub articles: Vec<Article>,
}

/// POST body. `category_name` and `text_body` are mutually exclusive and the
/// unused one must be *absent* — the API rejects the key even when it is
/// `null`, hence `skip_serializing_if`.
#[derive(Serialize, Debug, Clone, Default)]
#[serde(rename_all = "camelCase")]
pub struct NewSection {
    #[serde(rename = "type")]
    pub kind: SectionType,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub category_name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub text_body: Option<String>,
}

impl NewSection {
    pub fn category(name: String) -> Self {
        Self {
            kind: SectionType::Category,
            category_name: Some(name),
            text_body: None,
        }
    }

    pub fn text(body: String) -> Self {
        Self {
            kind: SectionType::Text,
            category_name: None,
            text_body: Some(body),
        }
    }
}

/// PUT body — the API never changes `type` nor `position`.
#[derive(Serialize, Debug, Clone, Default)]
#[serde(rename_all = "camelCase")]
pub struct UpdateSection {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub category_name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub text_body: Option<String>,
}

impl UpdateSection {
    pub fn category(name: String) -> Self {
        Self {
            category_name: Some(name),
            text_body: None,
        }
    }

    pub fn text(body: String) -> Self {
        Self {
            category_name: None,
            text_body: Some(body),
        }
    }
}

fn sections_url(issue_id: u32) -> String {
    let config = g_config();
    format!("{}/api/issue/{}/section", config.api_url, issue_id)
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

pub fn create_section(
    issue_id: u32,
    payload: NewSection,
    token: String,
) -> Result<IssueSection, String> {
    let json = serde_json::to_string(&payload).unwrap();

    let client = reqwest::blocking::Client::new();
    let res = client
        .post(sections_url(issue_id))
        .headers(auth_headers(&token))
        .body(json)
        .send();

    parse(res)
}

pub fn update_section(
    issue_id: u32,
    section_id: u32,
    payload: UpdateSection,
    token: String,
) -> Result<IssueSection, String> {
    let json = serde_json::to_string(&payload).unwrap();

    let client = reqwest::blocking::Client::new();
    let res = client
        .put(format!("{}/{}", sections_url(issue_id), section_id))
        .headers(auth_headers(&token))
        .body(json)
        .send();

    parse(res)
}

pub fn delete_section(
    issue_id: u32,
    section_id: u32,
    token: String,
) -> Result<ResponseMessage, String> {
    let client = reqwest::blocking::Client::new();
    let res = client
        .delete(format!("{}/{}", sections_url(issue_id), section_id))
        .headers(auth_headers(&token))
        .send();

    parse(res)
}

/// `order` must list every section id of the issue, in the wanted order —
/// a partial list is rejected with a 400 by the API.
pub fn reorder_sections(
    issue_id: u32,
    order: Vec<u32>,
    token: String,
) -> Result<ResponseMessage, String> {
    let json = serde_json::json!({ "order": order }).to_string();

    let client = reqwest::blocking::Client::new();
    let res = client
        .put(format!("{}/reorder", sections_url(issue_id)))
        .headers(auth_headers(&token))
        .body(json)
        .send();

    parse(res)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Le champ inutilisé doit être *absent* du corps : l'API teste la
    /// présence de la clé, pas sa valeur, et renvoie 400 si les deux sont là.
    #[test]
    fn new_category_section_omits_text_body() {
        let json = serde_json::to_string(&NewSection::category(String::from("Tech"))).unwrap();

        assert_eq!(json, r##"{"type":"CATEGORY","categoryName":"Tech"}"##);
        assert!(!json.contains("textBody"));
    }

    #[test]
    fn new_text_section_omits_category_name() {
        let json = serde_json::to_string(&NewSection::text(String::from("# Hi"))).unwrap();

        assert_eq!(json, r##"{"type":"TEXT","textBody":"# Hi"}"##);
        assert!(!json.contains("categoryName"));
    }

    #[test]
    fn update_payloads_omit_the_unused_field() {
        let category =
            serde_json::to_string(&UpdateSection::category(String::from("News"))).unwrap();
        let text = serde_json::to_string(&UpdateSection::text(String::from("body"))).unwrap();

        assert_eq!(category, r##"{"categoryName":"News"}"##);
        assert_eq!(text, r##"{"textBody":"body"}"##);
    }

    /// Forme réellement renvoyée par GET /api/issue/{id}.
    #[test]
    fn deserializes_a_text_section() {
        let section: IssueSection = serde_json::from_str(
            r##"{"id":1,"issueId":2,"position":0,"type":"TEXT",
                 "categoryName":null,"textBody":"# Titre\n\n::youtube[https://youtu.be/x]",
                 "articles":[]}"##,
        )
        .unwrap();

        assert_eq!(section.kind, SectionType::Text);
        assert_eq!(section.category_name, None);
        assert_eq!(
            section.text_body.as_deref(),
            Some("# Titre\n\n::youtube[https://youtu.be/x]")
        );
    }

    #[test]
    fn deserializes_a_category_section_with_its_articles() {
        let section: IssueSection = serde_json::from_str(
            r##"{"id":3,"issueId":2,"position":1,"type":"CATEGORY",
                 "categoryName":"Tech","textBody":null,
                 "articles":[{"id":7,"sectionId":3,"position":0,"title":"T",
                              "sourceName":"Le Monde","sourceUrl":"https://x","summary":"**bold**"}]}"##,
        )
        .unwrap();

        assert_eq!(section.kind, SectionType::Category);
        assert_eq!(section.category_name.as_deref(), Some("Tech"));
        assert_eq!(section.articles.len(), 1);
        assert_eq!(section.articles[0].summary, "**bold**");
    }
}
