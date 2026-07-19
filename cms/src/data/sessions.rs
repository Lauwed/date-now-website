use std::collections::HashMap;
use std::io::Write;
use std::{fs, path::PathBuf};

use directories::ProjectDirs;
use serde::{Deserialize, Serialize};

use crate::data::responses::Response;
use crate::data::users::User;
use crate::g_config;

#[derive(Default, Clone)]
pub struct Session {
    pub token: String,
    pub refresh_token: String,
    pub user: User,
}

#[derive(Serialize, Deserialize)]
pub struct StoredSession {
    token: String,
    refresh_token: String,
    email: String,
}

impl StoredSession {
    pub fn get_refresh_token(self) -> String {
        self.refresh_token
    }
}

#[derive(Deserialize, Debug, Clone)]
pub struct TokenPair {
    pub token: String,
    pub refresh_token: String,
}

fn session_file_path() -> Option<PathBuf> {
    if let Some(project_dirs) = ProjectDirs::from("com", "date-now", "cms") {
        Some(project_dirs.config_dir().join("session.json"))
    } else {
        eprintln!("Error while opening project directory");
        None
    }
}

pub fn save_session(session: &Session) {
    if let Some(path) = session_file_path() {
        if let Some(parent) = path.parent() {
            if let Err(e) = fs::create_dir_all(parent) {
                eprintln!("Error while config directory creation: {}", e);
                return;
            }

            let stored = StoredSession {
                token: session.token.clone(),
                refresh_token: session.refresh_token.clone(),
                email: session.user.email.clone(),
            };

            let json = match serde_json::to_string(&stored) {
                Ok(j) => j,
                Err(e) => {
                    eprintln!("Couldn't serialize session: {}", e);
                    return;
                }
            };

            match fs::File::create(&path) {
                Ok(mut file) => {
                    if let Err(e) = file.write_all(json.as_bytes()) {
                        eprintln!("Error while writing session: {}", e);
                        return;
                    }

                    #[cfg(unix)]
                    {
                        use std::os::unix::fs::PermissionsExt;

                        let _ = fs::set_permissions(&path, fs::Permissions::from_mode(0o600));
                    }
                }
                Err(e) => {
                    eprintln!("Error while session file creation: {}", e);
                }
            }
        } else {
            eprintln!("Can't access parent directory");
        }
    } else {
        eprintln!("Can't find the config directory");
    }
}

pub fn load_session() -> Option<StoredSession> {
    if let Some(path) = session_file_path() {
        if let Ok(content) = fs::read_to_string(path) {
            match serde_json::from_str(&content) {
                Ok(c) => Some(c),
                Err(e) => {
                    eprintln!("Error while parsing config file: {}", e);
                    None
                }
            }
        } else {
            eprintln!("Can't read file.");
            None
        }
    } else {
        eprintln!("Error while retrieving configuration file");
        None
    }
}

pub fn clear_session() {
    if let Some(path) = session_file_path() {
        let _ = fs::remove_file(path);
    }
}

pub fn refresh_access_token(refresh_token: &str) -> Result<TokenPair, String> {
    let config = g_config();

    let url = format!("{}/api/auth/refresh", config.api_url);

    let mut map = HashMap::new();
    map.insert("refresh_token", refresh_token);

    let client = reqwest::blocking::Client::new();
    let res = client.post(url).json(&map).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<TokenPair>>(&text) {
                Ok(Response::Success(pair)) => Ok(pair),
                Ok(Response::Error(res_err)) => Err(format!(
                    "Error response: {} - {}",
                    res_err.code, res_err.message
                )),
                Err(parse_err) => Err(format!("Error parsing: {}", parse_err)),
            },
            Err(text_err) => Err(format!("Error parsing text response: {}", text_err)),
        },
        Err(req_err) => Err(format!("Error request: {}", req_err)),
    }
}
