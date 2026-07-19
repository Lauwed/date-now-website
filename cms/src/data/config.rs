use std::env;

#[derive(Debug)]
pub struct Config {
    pub api_url: String,
    pub uri_scheme: String,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            api_url: env::var("API_URL").unwrap(),
            uri_scheme: env::var("URI_SCHEME").unwrap(),
        }
    }
}
