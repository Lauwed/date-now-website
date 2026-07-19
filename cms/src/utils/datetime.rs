use std::time::{Duration, UNIX_EPOCH};

use chrono::{DateTime, Utc};

pub fn get_datetime_str(timestamp: u64) -> String {
    let d = UNIX_EPOCH + Duration::from_secs(timestamp);
    let date = DateTime::<Utc>::from(d);

    date.format("%Y-%m-%d %H:%M").to_string()
}
