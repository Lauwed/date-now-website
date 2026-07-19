use core::fmt;

use serde::Deserialize;

#[derive(Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ResponseMany<T> {
    pub data: Vec<T>,
    pub count: u32,
    pub total: u32,
    pub total_pages: u32,
}

#[derive(Deserialize)]
pub struct ResponseError {
    pub code: u16,
    pub message: String,
}
impl fmt::Display for ResponseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "Response error - Code: {} - Message: {}",
            self.code, self.message
        )
    }
}

#[derive(Deserialize)]
#[serde(untagged)]
pub enum Response<T> {
    Error(ResponseError),
    Success(T),
}
