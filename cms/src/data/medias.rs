use std::path::Path;

use reqwest::header::AUTHORIZATION;
use serde::{Deserialize, Serialize};

use crate::{data::responses::Response, g_config};

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Media {
    pub id: u32,
    alt: Option<String>,
    pub url: Option<String>,
    width: Option<f32>,
    height: Option<f32>,
}

#[derive(serde::Deserialize)]
struct UploadedMedia {
    pub id: u32,
}

pub fn upload_media(alt: &str, path: &Path, token: String) -> Result<u32, String> {
    let config = g_config();
    let url = format!("{}/api/media", config.api_url);

    let form = reqwest::blocking::multipart::Form::new()
        .text("textAlternatif", alt.to_string())
        .file("file", path)
        .map_err(|e| format!("Error reading file: {}", e))?;

    let client = reqwest::blocking::Client::new();
    let res = client
        .post(url)
        .header(AUTHORIZATION, format!("Bearer {}", token))
        .multipart(form)
        .send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<UploadedMedia>>(&text) {
                Ok(Response::Success(media)) => Ok(media.id),
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
