use std::path::Path;
use std::thread::sleep;
use std::time::{Duration, Instant};

use reqwest::header::AUTHORIZATION;
use serde::{Deserialize, Serialize};

use crate::data::responses::{Response, ResponseMany};
use crate::g_config;

#[derive(Serialize, Deserialize, Debug, Clone)]
#[serde(rename_all = "camelCase")]
pub struct Media {
    pub id: u32,
    pub alt: Option<String>,
    pub url: Option<String>,
    pub thumb_url: Option<String>,
    pub width: Option<f32>,
    pub height: Option<f32>,
}

impl Media {
    /// L'API crée la ligne avec une url vide puis la remplit une fois la
    /// conversion terminée.
    fn is_ready(&self) -> bool {
        matches!(self.url.as_deref(), Some(url) if !url.is_empty())
    }
}

/// Délai maximal d'attente de la conversion serveur d'une image.
pub const UPLOAD_TIMEOUT: Duration = Duration::from_secs(60);

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

fn auth_header(token: &str) -> String {
    format!("Bearer {}", token)
}

pub fn get_media(id: u32, token: &str) -> Result<Option<Media>, String> {
    let config = g_config();
    let url = format!("{}/api/media/{}", config.api_url, id);

    let client = reqwest::blocking::Client::new();
    let res = client
        .get(url)
        .header(AUTHORIZATION, auth_header(token))
        .send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<Media>>(&text) {
                Ok(Response::Success(media)) => Ok(Some(media)),
                // 404 : la conversion a échoué et l'API a supprimé la ligne.
                Ok(Response::Error(res_err)) if res_err.code == 404 => Ok(None),
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

/// L'upload répond 202 : la conversion WebP et l'envoi vers le stockage se
/// font dans un thread côté serveur. On interroge la ressource jusqu'à ce que
/// son url soit renseignée.
pub fn wait_for_media(id: u32, token: &str, timeout: Duration) -> Result<Media, String> {
    let started = Instant::now();

    loop {
        match get_media(id, token)? {
            Some(media) if media.is_ready() => return Ok(media),
            // La ligne a disparu : la conversion a échoué côté serveur.
            None => return Err(String::from("The server could not process this image.")),
            Some(_) => {}
        }

        if started.elapsed() >= timeout {
            return Err(String::from("Timed out waiting for the image to be processed."));
        }

        sleep(Duration::from_millis(300));
    }
}

/// Envoie l'image puis attend l'url définitive.
pub fn upload_media_blocking(alt: &str, path: &Path, token: &str) -> Result<Media, String> {
    let id = upload_media(alt, path, token.to_string())?;
    wait_for_media(id, token, UPLOAD_TIMEOUT)
}

/// Retrouve un media à partir de son url pleine taille — le markdown ne
/// contient que l'url, or la suppression réclame l'id.
pub fn find_media_by_url(url: &str, token: &str) -> Result<Option<Media>, String> {
    let config = g_config();
    let mut endpoint = url::Url::parse(&format!("{}/api/media", config.api_url))
        .map_err(|e| format!("Invalid API url: {}", e))?;
    endpoint.query_pairs_mut().append_pair("url", url);

    let client = reqwest::blocking::Client::new();
    let res = client
        .get(endpoint.as_str())
        .header(AUTHORIZATION, auth_header(token))
        .send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<ResponseMany<Media>>>(&text) {
                Ok(Response::Success(list)) => Ok(list.data.into_iter().next()),
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

/// Supprime un media et ses fichiers. Un 409 signifie que l'image est encore
/// utilisée ailleurs : c'est le résultat attendu, pas une erreur à remonter.
pub fn delete_media(id: u32, token: &str) -> Result<(), String> {
    let config = g_config();
    let url = format!("{}/api/media/{}", config.api_url, id);

    let client = reqwest::blocking::Client::new();
    let res = client
        .delete(url)
        .header(AUTHORIZATION, auth_header(token))
        .send();

    match res {
        Ok(r) => {
            if r.status() == reqwest::StatusCode::CONFLICT {
                return Ok(());
            }
            match r.text() {
                Ok(text) => match serde_json::from_str::<Response<crate::data::responses::ResponseMessage>>(&text) {
                    Ok(Response::Success(_)) => Ok(()),
                    Ok(Response::Error(res_err)) => Err(format!(
                        "Error response: {} - {}",
                        res_err.code, res_err.message
                    )),
                    Err(json_parse_err) => Err(format!("Error parsing: {}", json_parse_err)),
                },
                Err(text_err) => Err(format!("Error parsing text response: {}", text_err)),
            }
        }
        Err(req_err) => Err(format!("Error request: {}", req_err)),
    }
}
