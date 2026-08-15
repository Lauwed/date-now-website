use std::fmt;

use iced::Element;
use iced::widget::{checkbox, text};
use reqwest::header::{AUTHORIZATION, CONTENT_TYPE, HeaderMap};
use serde;
use serde::{Deserialize, Serialize};

use crate::components::badge::{BadgeStyle, badge};
use crate::data::responses::{Response, ResponseMany, ResponseMessage};
use crate::data::traits::Table;
use crate::g_config;
use crate::utils::datetime::get_datetime_str;

use super::medias::Media;

#[derive(Serialize, Deserialize, Default, Debug, Clone)]
pub enum UserRole {
    AUTHOR,
    #[default]
    USER,
}

impl fmt::Display for UserRole {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}",
            match self {
                UserRole::AUTHOR => String::from("Author"),
                UserRole::USER => String::from("User"),
            }
        )
    }
}

#[derive(Serialize, Default, Deserialize, Debug, Clone)]
#[serde(rename_all = "camelCase")]
pub struct User {
    id: u32,
    pub username: Option<String>,
    pub email: String,
    role: UserRole,
    link: Option<String>,
    picture: Option<Media>,
    is_supporter: bool,
    totp_seed: Option<String>,

    created_at: u64,
    subscribed_at: u64,
    tracker_pixel_consent_date: u64,
}

impl Table for User {
    fn value_from_key(&self, key: &str) -> String {
        match key {
            "id" => self.id.to_string(),
            "email" => self.email.clone(),
            "username" => self.username.clone().unwrap_or_default(),
            "role" => format!("{:?}", self.role),
            "subscribed_at" => get_datetime_str(self.subscribed_at),
            "tracker_pixel_consent_date" => match self.tracker_pixel_consent_date {
                0 => String::from("-"),
                _ => get_datetime_str(self.tracker_pixel_consent_date),
            },
            _ => String::from("Key not found"),
        }
    }
    fn render<'a, M: 'a>(&self, key: &str) -> Element<'a, M> {
        match key {
            "is_supporter" => checkbox(self.is_supporter).into(),
            "role" => badge(
                self.role.to_string(),
                match self.role {
                    UserRole::AUTHOR => BadgeStyle::Primary,
                    UserRole::USER => BadgeStyle::Success,
                },
            )
            .into(),
            _ => text("No render set").into(),
        }
    }
}

impl fmt::Display for User {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.email)
    }
}

pub fn get_current_user(token: &str) -> Result<User, String> {
    let config = g_config();
    let url = format!("{}/api/user/current", config.api_url);

    let client = reqwest::blocking::Client::new();
    let res = client
        .get(url)
        .header(AUTHORIZATION, format!("Bearer {}", token))
        .send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<User>>(&text) {
                Ok(Response::Success(user)) => Ok(user),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    println!("Error parsing: {}", json_parse_err);
                    match serde_json::from_str::<User>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            println!("Error parsing user: {}", user_parse_err);
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

pub fn get_users() -> Result<ResponseMany<User>, String> {
    let config = g_config();
    let url = format!("{}/api/user", config.api_url);

    let client = reqwest::blocking::Client::new();
    let res = client.get(url).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMany<User>>>(&text) {
                Ok(Response::Success(users)) => Ok(users),
                Ok(Response::Error(res_err)) => {
                    println!("Error response: {} - {}", res_err.code, res_err.message);
                    return Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    ));
                }
                Err(json_parse_err) => {
                    eprintln!("Error parsing: {}", json_parse_err);
                    match serde_json::from_str::<ResponseMany<User>>(&text) {
                        Ok(json) => Ok(json),
                        Err(user_parse_err) => {
                            eprintln!("Error parsing user: {}", user_parse_err);
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

pub fn delete_user(id: String, token: String) -> Result<ResponseMessage, String> {
    let config = g_config();
    let url = format!("{}/api/user/{}", config.api_url, id);

    let mut headers = HeaderMap::new();
    headers.insert(AUTHORIZATION, format!("Bearer {}", token).parse().unwrap());
    headers.insert(CONTENT_TYPE, "application/json".parse().unwrap());

    let client = reqwest::blocking::Client::new();
    let res = client.delete(url).headers(headers).send();

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
                            eprintln!("Error parsing user: {}", user_parse_err);
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
