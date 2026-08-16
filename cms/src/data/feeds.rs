use std::fmt;

use iced::Element;
use iced::widget::text;
use reqwest::header::{AUTHORIZATION, CONTENT_TYPE, HeaderMap};
use serde;
use serde::{Deserialize, Serialize};

use crate::components::badge::{BadgeStyle, badge};
use crate::data::responses::{Response, ResponseMany, ResponseMessage};
use crate::data::traits::Table;
use crate::g_config;

#[derive(Serialize, Default, Deserialize, Debug, Clone)]
#[serde(rename_all = "camelCase")]
pub struct Feed {
    pub id: u32,
    pub name: String,
    pub link: String,
    pub is_rss_feed: bool,
}

impl Table for Feed {
    fn value_from_key(&self, key: &str) -> String {
        match key {
            "id" => self.id.to_string(),
            "name" => self.name.clone(),
            "link" => self.link.clone(),
            "is_rss_feed" => self.is_rss_feed.to_string(),
            _ => String::from("Key not found"),
        }
    }
    fn render<'a, M: 'a>(&self, key: &str) -> Element<'a, M> {
        match key {
            "is_rss_feed" => {
                if self.is_rss_feed {
                    badge(String::from("RSS"), BadgeStyle::Primary)
                } else {
                    badge(String::from("Web"), BadgeStyle::Secondary)
                }
            }
            _ => text("No render set").into(),
        }
    }
}

impl fmt::Display for Feed {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.name)
    }
}

pub fn get_feed(id: &str) -> Result<Response<Feed>, String> {
    let config = g_config();
    let url = format!("{}/api/feed/{}", config.api_url, id);

    let client = reqwest::blocking::Client::new();
    let res = client.get(url).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Feed>>>(&text) {
                Ok(Response::Success(feed)) => Ok(feed),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Feed>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing feed: {}", user_parse_err);
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

pub fn get_feeds() -> Result<ResponseMany<Feed>, String> {
    let config = g_config();
    let url = format!("{}/api/feed", config.api_url);

    let client = reqwest::blocking::Client::new();
    let res = client.get(url).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMany<Feed>>>(&text) {
                Ok(Response::Success(feeds)) => Ok(feeds),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<ResponseMany<Feed>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing feed: {}", user_parse_err);
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

pub fn create_feed(item: Feed, token: String) -> Result<Response<Feed>, String> {
    let config = g_config();
    let url = format!("{}/api/feed", config.api_url);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let json = serde_json::to_string(&item).unwrap();
    println!("feed: {}", json);

    let client = reqwest::blocking::Client::new();
    let res = client.post(url).headers(headers).body(json).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Feed>>>(&text) {
                Ok(Response::Success(feed)) => Ok(feed),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Feed>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing feed: {}", user_parse_err);
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

pub fn update_feed(id: &str, item: Feed, token: String) -> Result<Response<Feed>, String> {
    let config = g_config();
    let url = format!("{}/api/feed/{}", config.api_url, id);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let json = serde_json::to_string(&item).unwrap();
    println!("feed: {}", json);

    let client = reqwest::blocking::Client::new();
    let res = client.put(url).headers(headers).body(json).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Feed>>>(&text) {
                Ok(Response::Success(feed)) => Ok(feed),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Feed>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing feed: {}", user_parse_err);
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

pub fn delete_feed(id: String, token: String) -> Result<ResponseMessage, String> {
    let config = g_config();
    let url = format!("{}/api/feed/{}", config.api_url, id);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let client = reqwest::blocking::Client::new();
    let res = client.delete(url).headers(headers).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMessage>>(&text) {
                Ok(Response::Success(feed)) => Ok(feed),
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
                            eprintln!("Error parsing feed: {}", user_parse_err);
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
