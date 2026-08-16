use serde::Deserialize;

use crate::data::issues::{Issue, IssueStatus, get_issues};
use crate::data::responses::{Response, ResponseMany};
use crate::data::sponsors::get_sponsors;
use crate::g_config;

#[derive(Deserialize)]
struct CountResponse {
    count: i64,
}

fn get_count(url: String, token: &str) -> Result<i64, String> {
    let client = reqwest::blocking::Client::new();
    let res = client
        .get(url)
        .header(reqwest::header::AUTHORIZATION, format!("Bearer {}", token))
        .send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<Response<CountResponse>>(&text) {
                Ok(Response::Success(c)) => Ok(c.count),
                Ok(Response::Error(e)) => Err(format!("{} - {}", e.code, e.message)),
                Err(e) => Err(format!("Error parsing count: {}", e)),
            },
            Err(e) => Err(format!("Error parsing text response: {}", e)),
        },
        Err(e) => Err(format!("Error request: {}", e)),
    }
}

pub fn get_subscriber_count(token: &str) -> Result<i64, String> {
    let config = g_config();
    get_count(
        format!("{}/api/user/count?type=subscriber", config.api_url),
        token,
    )
}

pub fn get_author_count(token: &str) -> Result<i64, String> {
    let config = g_config();
    get_count(
        format!("{}/api/user/count?type=author", config.api_url),
        token,
    )
}

pub fn get_issue_count(status: Option<&str>, token: &str) -> Result<i64, String> {
    let config = g_config();
    let url = match status {
        Some(s) => format!("{}/api/issue/count?status={}", config.api_url, s),
        None => format!("{}/api/issue/count", config.api_url),
    };
    get_count(url, token)
}

pub fn get_sponsor_count() -> Result<u32, String> {
    let config = g_config();
    let url = format!("{}/api/sponsor?limit=1", config.api_url);

    let client = reqwest::blocking::Client::new();
    let res = client.get(url).send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => match serde_json::from_str::<
                Response<ResponseMany<crate::data::sponsors::Sponsor>>,
            >(&text)
            {
                Ok(Response::Success(res)) => Ok(res.total),
                Ok(Response::Error(e)) => Err(format!("{} - {}", e.code, e.message)),
                Err(e) => Err(format!("Error parsing sponsor count: {}", e)),
            },
            Err(e) => Err(format!("Error parsing text response: {}", e)),
        },
        Err(e) => Err(format!("Error request: {}", e)),
    }
}

pub fn get_total_view_count(token: &str) -> Result<u32, String> {
    let config = g_config();
    let url = format!("{}/api/view?limit=1", config.api_url);

    let client = reqwest::blocking::Client::new();
    let res = client
        .get(url)
        .header(reqwest::header::AUTHORIZATION, format!("Bearer {}", token))
        .send();

    match res {
        Ok(r) => match r.text() {
            Ok(text) => {
                match serde_json::from_str::<Response<ResponseMany<serde_json::Value>>>(&text) {
                    Ok(Response::Success(res)) => Ok(res.total),
                    Ok(Response::Error(e)) => Err(format!("{} - {}", e.code, e.message)),
                    Err(e) => Err(format!("Error parsing view count: {}", e)),
                }
            }
            Err(e) => Err(format!("Error parsing text response: {}", e)),
        },
        Err(e) => Err(format!("Error request: {}", e)),
    }
}

/// Les N dernières issues, triées par numéro décroissant — sert au bar chart
/// et au top 5. Réutilise `get_issues()` tel quel (pas de nouvel appel réseau
/// dédié : le dashboard n'a pas besoin d'un endpoint séparé pour ça).
pub fn get_recent_issues_for_dashboard(limit: usize) -> Vec<Issue> {
    let mut issues = match get_issues() {
        Ok(res) => res.data,
        Err(e) => {
            eprintln!("Error: {}", e);
            vec![]
        }
    };
    issues.sort_by(|a, b| b.issue_number.cmp(&a.issue_number));
    issues.truncate(limit);
    issues
}
