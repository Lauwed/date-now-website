use core::fmt;
use iced::widget::{Row, text};
use iced::{Color, Element};
use reqwest::header::{AUTHORIZATION, CONTENT_LENGTH, CONTENT_TYPE, HeaderMap};
use serde;
use serde::{Deserialize, Serialize};
use std::str::FromStr;

use crate::components::badge::{BadgeStyle, badge};
use crate::data::medias::Media;
use crate::data::responses::{Response, ResponseMany, ResponseMessage};
use crate::data::sponsors::Sponsor;
use crate::data::tags::Tag;
use crate::data::traits;
use crate::data::users::User;
use crate::g_config;
use crate::utils::datetime::get_datetime_str;

#[derive(Serialize, Deserialize, Default, Debug, Clone, PartialEq)]
#[serde(rename_all = "UPPERCASE")]
pub enum IssueStatus {
    #[default]
    Draft,
    Published,
    Archive,
}

impl fmt::Display for IssueStatus {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                IssueStatus::Draft => String::from("Draft"),
                IssueStatus::Published => String::from("Published"),
                IssueStatus::Archive => String::from("Archive"),
            }
        )
    }
}

#[derive(Serialize, Default, Deserialize, Debug, Clone)]
#[serde(rename_all = "camelCase")]
pub struct Issue {
    pub id: u32,
    pub slug: String,
    pub title: String,
    pub subtitle: String,
    pub created_at: u64,
    pub published_at: u64,
    pub updated_at: u64,
    pub issue_number: u32,
    pub excerpt: String,
    pub is_sponsored: bool,
    pub vod_url: String,
    pub status: IssueStatus,
    pub opened_mail_count: u64,
    pub views: u64,
    pub picture: Option<Media>,

    pub authors: Vec<User>,
    pub tags: Vec<Tag>,
    pub sponsors: Vec<Sponsor>,
}

impl traits::Table for Issue {
    fn value_from_key(&self, key: &str) -> String {
        match key {
            "id" => self.id.to_string(),
            "title" => self.title.clone(),
            "issue_number" => self.issue_number.to_string(),
            "created_at" => get_datetime_str(self.created_at),
            "updated_at" => match self.updated_at {
                0 => String::from("-"),
                _ => get_datetime_str(self.updated_at),
            },
            "published_at" => match self.published_at {
                0 => String::from("-"),
                _ => get_datetime_str(self.published_at),
            },
            "status" => self.status.to_string(),
            "views" => self.views.to_string(),
            "tags" => self
                .tags
                .iter()
                .map(|t| t.name.clone())
                .collect::<Vec<String>>()
                .join(", "),
            _ => String::from("Key not found"),
        }
    }

    fn render<'a, M: 'a>(&self, key: &str) -> Element<'a, M> {
        match key {
            "status" => badge(
                self.status.to_string(),
                match self.status {
                    IssueStatus::Draft => BadgeStyle::Primary,
                    IssueStatus::Archive => BadgeStyle::Ghost,
                    IssueStatus::Published => BadgeStyle::Success,
                },
            )
            .into(),
            "tags" => self
                .tags
                .iter()
                .map(|t| {
                    badge(
                        t.name.to_string(),
                        BadgeStyle::FromColor(match Color::from_str(&t.color) {
                            Ok(c) => c,
                            Err(_) => Color::from_rgb(15.0, 15.0, 15.0),
                        }),
                    )
                })
                .collect::<Row<'_, M>>()
                .spacing(4)
                .wrap()
                .into(),
            _ => text("No render set").into(),
        }
    }
}

pub fn get_issue(id: u32) -> Result<Response<Issue>, String> {
    let config = g_config();
    let url = format!("{}/api/issue/{}", config.api_url, id);

    let client = reqwest::blocking::Client::new();
    let res = client.get(url).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Issue>>>(&text) {
                Ok(Response::Success(issue)) => Ok(issue),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Issue>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing issue: {}", user_parse_err);
                            return Err(format!("Error parsing: {}", user_parse_err));
                        }
                    }
                }
            },
            Err(text_err) => Err(format!("Error parsing text response: {}", text_err)),
        },
        Err(req_err) => {
            println!("error request: {}", req_err);
            Err(format!("Error request: {}", req_err))
        }
    }
}

pub fn get_issues() -> Result<ResponseMany<Issue>, String> {
    let config = g_config();
    let url = format!("{}/api/issue", config.api_url);

    let client = reqwest::blocking::Client::new();
    let res = client.get(url).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMany<Issue>>>(&text) {
                Ok(Response::Success(issues)) => Ok(issues),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<ResponseMany<Issue>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing issue: {}", user_parse_err);
                            return Err(format!("Error parsing: {}", user_parse_err));
                        }
                    }
                }
            },
            Err(text_err) => Err(format!("Error parsing text response: {}", text_err)),
        },
        Err(req_err) => {
            println!("error request: {}", req_err);
            Err(format!("Error request: {}", req_err))
        }
    }
}

pub fn create_issue(item: Issue, token: String) -> Result<Response<Issue>, String> {
    let config = g_config();
    let url = format!("{}/api/issue", config.api_url);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let json = serde_json::to_string(&item).unwrap();
    println!("issue: {}", json);

    let client = reqwest::blocking::Client::new();
    let res = client.post(url).headers(headers).body(json).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Issue>>>(&text) {
                Ok(Response::Success(issue)) => Ok(issue),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Issue>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing issue: {}", user_parse_err);
                            return Err(format!("Error parsing: {}", user_parse_err));
                        }
                    }
                }
            },
            Err(text_err) => Err(format!("Error parsing text response: {}", text_err)),
        },
        Err(req_err) => {
            println!("error request: {}", req_err);
            Err(format!("Error request: {}", req_err))
        }
    }
}

pub fn update_issue(id: u32, item: Issue, token: String) -> Result<Response<Issue>, String> {
    let config = g_config();
    let url = format!("{}/api/issue/{}", config.api_url, id);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let json = serde_json::to_string(&item).unwrap();
    println!("issue: {}", json);

    let client = reqwest::blocking::Client::new();
    let res = client.put(url).headers(headers).body(json).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Issue>>>(&text) {
                Ok(Response::Success(issue)) => Ok(issue),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Issue>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing issue: {}", user_parse_err);
                            return Err(format!("Error parsing: {}", user_parse_err));
                        }
                    }
                }
            },
            Err(text_err) => Err(format!("Error parsing text response: {}", text_err)),
        },
        Err(req_err) => {
            println!("error request: {}", req_err);
            Err(format!("Error request: {}", req_err))
        }
    }
}

pub fn publish_issue(id: u32, token: String) -> Result<ResponseMessage, String> {
    let config = g_config();
    let url = format!("{}/api/issue/{}/publish", config.api_url, id);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_LENGTH, "0".parse().unwrap());

    let client = reqwest::blocking::Client::new();
    let res = client.post(url).headers(headers).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMessage>>(&text) {
                Ok(Response::Success(msg)) => Ok(msg),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<ResponseMessage>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing issue: {}", user_parse_err);
                            return Err(format!("Error parsing: {}", user_parse_err));
                        }
                    }
                }
            },
            Err(text_err) => Err(format!("Error parsing text response: {}", text_err)),
        },
        Err(req_err) => {
            println!("error request: {}", req_err);
            Err(format!("Error request: {}", req_err))
        }
    }
}
