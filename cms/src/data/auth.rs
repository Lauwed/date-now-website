use reqwest;
use std::collections::HashMap;

pub fn send_login_confirmation_mail(
    email: &str,
) -> Result<reqwest::blocking::Response, reqwest::Error> {
    let url = "http://localhost:8000/api/auth/login";

    let mut map = HashMap::new();
    map.insert("email", email);

    let client = reqwest::blocking::Client::new();
    client.post(url).json(&map).send()
}
