use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Media {
    id: u16,
    alt: String,
    url: String,
    width: Option<f32>,
    height: Option<f32>,
}
