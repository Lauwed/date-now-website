use crate::g_config;

use super::medias::Media;
use super::responses::Response;
use reqwest::header::AUTHORIZATION;
use serde;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Default, Debug, Clone)]
pub enum UserRole {
    AUTHOR,
    #[default]
    USER,
}

#[derive(Serialize, Default, Deserialize, Debug, Clone)]
#[serde(rename_all = "camelCase")]
pub struct User {
    id: u8,
    pub username: Option<String>,
    pub email: String,
    role: UserRole,
    link: Option<String>,
    picture: Option<Media>,
    is_supporter: bool,
    totp_seed: Option<String>,

    created_at: u64,
    subscribed_at: u64,
}

pub fn get_users_vec(users_str: String) -> Vec<User> {
    let deserializer = serde_json::Deserializer::from_str(&users_str).into_iter::<User>();

    let mut users = Vec::new();
    for value in deserializer {
        // println!("{:?}", value.unwrap());

        match value {
            Ok(user) => users.push(user),
            Err(e) => println!("Error deserialization: {}", e),
        }
    }

    users
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
            Ok(text) => {
                println!("response: {}", text);

                match serde_json::from_str::<Response<User>>(&text) {
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
                }
            }
            Err(text_err) => Err(format!("Error parsing text response: {}", text_err)),
        },
        Err(req_err) => {
            println!("error request: {}", req_err);
            Err(format!("Error request: {}", req_err))
        }
    }
}

pub fn get_nb_subscribers() -> Result<Vec<User>, String> {
    let config = g_config();
    let url = format!("{}/api/user", config.api_url);

    let res = reqwest::blocking::get(url);

    match res {
        Ok(r) => match r.text() {
            Ok(b) => Ok(get_users_vec(b)),
            Err(b_err) => Err(format!("body failed: {}", b_err)),
        },
        Err(req_err) => Err(format!("request failed: {}", req_err)),
    }
}
