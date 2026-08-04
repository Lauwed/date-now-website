use std::fmt;
use std::str::FromStr;

use iced::widget::text;
use iced::{Color, Element};
use reqwest::header::{AUTHORIZATION, CONTENT_TYPE, HeaderMap};
use serde;
use serde::{Deserialize, Serialize};

use crate::components::badge::{BadgeStyle, badge};
use crate::data::responses::{Response, ResponseMany, ResponseMessage};
use crate::data::traits::Table;
use crate::g_config;

#[derive(Serialize, Default, Deserialize, Debug, Clone)]
#[serde(rename_all = "camelCase")]
pub struct Tag {
    pub name: String,
    pub color: String,
}

impl Table for Tag {
    fn value_from_key(&self, key: &str) -> String {
        match key {
            "name" => self.name.clone(),
            "color" => self.color.clone(),
            _ => String::from("Key not found"),
        }
    }
    fn render<'a, M: 'a>(&self, key: &str) -> Element<'a, M> {
        match key {
            "color" => badge(
                self.color.clone(),
                BadgeStyle::FromColor(match Color::from_str(&self.color) {
                    Ok(c) => c,
                    Err(_) => Color::from_rgb(15.0, 15.0, 15.0),
                }),
            ),
            _ => text("No render set").into(),
        }
    }
}

impl fmt::Display for Tag {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.name)
    }
}

pub fn get_tag(id: &str) -> Result<Response<Tag>, String> {
    let config = g_config();
    println!("{}", id);
    let url = format!("{}/api/tag/{}", config.api_url, id);

    let client = reqwest::blocking::Client::new();
    let res = client.get(url).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Tag>>>(&text) {
                Ok(Response::Success(tag)) => Ok(tag),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Tag>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing tag: {}", user_parse_err);
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

pub fn get_tags() -> Result<ResponseMany<Tag>, String> {
    let config = g_config();
    let url = format!("{}/api/tag", config.api_url);

    let client = reqwest::blocking::Client::new();
    let res = client.get(url).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMany<Tag>>>(&text) {
                Ok(Response::Success(tags)) => Ok(tags),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<ResponseMany<Tag>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing tag: {}", user_parse_err);
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

pub fn create_tag(item: Tag, token: String) -> Result<Response<Tag>, String> {
    let config = g_config();
    let url = format!("{}/api/tag", config.api_url);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let json = serde_json::to_string(&item).unwrap();
    println!("tag: {}", json);

    let client = reqwest::blocking::Client::new();
    let res = client.post(url).headers(headers).body(json).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Tag>>>(&text) {
                Ok(Response::Success(tag)) => Ok(tag),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Tag>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing tag: {}", user_parse_err);
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

pub fn update_tag(id: &str, item: Tag, token: String) -> Result<Response<Tag>, String> {
    let config = g_config();
    let url = format!("{}/api/tag/{}", config.api_url, id);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let json = serde_json::to_string(&item).unwrap();
    println!("tag: {}", json);

    let client = reqwest::blocking::Client::new();
    let res = client.put(url).headers(headers).body(json).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Tag>>>(&text) {
                Ok(Response::Success(tag)) => Ok(tag),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Tag>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing tag: {}", user_parse_err);
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

pub fn delete_tag(id: String, token: String) -> Result<ResponseMessage, String> {
    let config = g_config();
    let url = format!("{}/api/tag/{}", config.api_url, id);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let client = reqwest::blocking::Client::new();
    let res = client.delete(url).headers(headers).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMessage>>(&text) {
                Ok(Response::Success(tag)) => Ok(tag),
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
                            eprintln!("Error parsing tag: {}", user_parse_err);
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
