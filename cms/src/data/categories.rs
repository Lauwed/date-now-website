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
pub struct Category {
    pub name: String,
    pub color: String,
}

impl Table for Category {
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
                    Err(_) => Color::from_rgb(15.0 / 255.0, 15.0 / 255.0, 15.0 / 255.0),
                }),
            ),
            _ => text("No render set").into(),
        }
    }
}

impl fmt::Display for Category {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.name)
    }
}

pub fn get_category(id: &str) -> Result<Response<Category>, String> {
    let config = g_config();
    let url = format!("{}/api/category/{}", config.api_url, id);

    let client = reqwest::blocking::Client::new();
    let res = client.get(url).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Category>>>(&text) {
                Ok(Response::Success(category)) => Ok(category),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Category>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing category: {}", user_parse_err);
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

pub fn get_categories() -> Result<ResponseMany<Category>, String> {
    let config = g_config();
    let url = format!("{}/api/category", config.api_url);

    let client = reqwest::blocking::Client::new();
    let res = client.get(url).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMany<Category>>>(&text) {
                Ok(Response::Success(categories)) => Ok(categories),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<ResponseMany<Category>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing category: {}", user_parse_err);
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

pub fn create_category(item: Category, token: String) -> Result<Response<Category>, String> {
    let config = g_config();
    let url = format!("{}/api/category", config.api_url);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let json = serde_json::to_string(&item).unwrap();
    println!("category: {}", json);

    let client = reqwest::blocking::Client::new();
    let res = client.post(url).headers(headers).body(json).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Category>>>(&text) {
                Ok(Response::Success(category)) => Ok(category),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Category>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing category: {}", user_parse_err);
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

pub fn update_category(id: &str, item: Category, token: String) -> Result<Response<Category>, String> {
    let config = g_config();
    let url = format!("{}/api/category/{}", config.api_url, id);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let json = serde_json::to_string(&item).unwrap();
    println!("category: {}", json);

    let client = reqwest::blocking::Client::new();
    let res = client.put(url).headers(headers).body(json).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Response<Category>>>(&text) {
                Ok(Response::Success(category)) => Ok(category),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);

                    match serde_json::from_str::<Response<Category>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing category: {}", user_parse_err);
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

pub fn delete_category(id: String, token: String) -> Result<ResponseMessage, String> {
    let config = g_config();
    let url = format!("{}/api/category/{}", config.api_url, id);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let client = reqwest::blocking::Client::new();
    let res = client.delete(url).headers(headers).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMessage>>(&text) {
                Ok(Response::Success(category)) => Ok(category),
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
                            eprintln!("Error parsing category: {}", user_parse_err);
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
