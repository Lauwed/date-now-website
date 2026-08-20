#[derive(Debug)]
pub struct Config {
    pub api_url: String,
    pub uri_scheme: String,
    pub front_url: String,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            api_url: String::from("http://localhost:8000"),
            uri_scheme: String::from("datenowcms"),
            front_url: String::from("http://localhost:5173"),
        }
    }
}
