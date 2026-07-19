use serde;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Default, Deserialize, Debug, Clone)]
#[serde(rename_all = "camelCase")]
pub struct Sponsor {
    name: String,
    link: String,
}
