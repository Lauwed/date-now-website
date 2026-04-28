use super::medias::Media;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Default, Debug)]
pub enum UserRole {
    Author,
    #[default]
    User,
}

#[derive(Serialize, Default, Deserialize, Debug)]
pub struct User {
    id: u8,
    username: Option<String>,
    email: String,
    role: UserRole,
    link: Option<String>,
    picture: Option<Media>,
    is_supporter: bool,
    totp_seed: Option<String>,

    created_at: u16,
    subscribed_at: u16,
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

pub fn get_nb_subscribers() -> Result<Vec<User>, String> {
    let url = "http://localhost:8000/api/user";

    let res = reqwest::blocking::get(url);

    match res {
        Ok(r) => match r.text() {
            Ok(b) => Ok(get_users_vec(b)),
            Err(b_err) => Err(format!("body failed: {}", b_err)),
        },
        Err(req_err) => Err(format!("request failed: {}", req_err)),
    }
}
