//! Auto-linking of pasted URLs: turns a pasted `https://…` into
//! `[Page title](https://…)` once the page title has been fetched.

use std::time::Duration;

use iced::futures::channel::oneshot;
use url::Url;

const USER_AGENT: &str = concat!("date-now-cms/", env!("CARGO_PKG_VERSION"));
const TIMEOUT: Duration = Duration::from_secs(5);
/// Titles live in `<head>`; no need to download megabytes of markup.
const MAX_CHARS: usize = 256 * 1024;

/// Returns the URL when `text` is a bare http(s) link and nothing else.
pub fn as_link(text: &str) -> Option<String> {
    let trimmed = text.trim();

    if trimmed.is_empty() || trimmed.split_whitespace().count() != 1 {
        return None;
    }

    let url = Url::parse(trimmed).ok()?;

    if !matches!(url.scheme(), "http" | "https") || !url.has_host() {
        return None;
    }

    Some(trimmed.to_string())
}

/// Escapes what would break the `[label](url)` syntax.
pub fn escape_label(title: &str) -> String {
    title
        .chars()
        .map(|c| match c {
            '[' => '(',
            ']' => ')',
            '\n' | '\r' | '\t' => ' ',
            other => other,
        })
        .collect::<String>()
        .split_whitespace()
        .collect::<Vec<_>>()
        .join(" ")
}

/// Fetches the `<title>` of `url`, `None` if the page is unreachable or untitled.
///
/// The request itself is blocking — like the rest of the API layer — so it runs
/// on its own thread to keep the UI responsive.
pub async fn fetch_title(url: String) -> Option<String> {
    let (sender, receiver) = oneshot::channel();

    std::thread::spawn(move || {
        let _ = sender.send(fetch_title_blocking(&url));
    });

    receiver.await.ok().flatten()
}

fn fetch_title_blocking(url: &str) -> Option<String> {
    let client = reqwest::blocking::Client::builder()
        .user_agent(USER_AGENT)
        .timeout(TIMEOUT)
        .build()
        .ok()?;

    let response = client.get(url).send().ok()?;

    if !response.status().is_success() {
        return None;
    }

    let body = response.text().ok()?;
    let head = match body.char_indices().nth(MAX_CHARS) {
        Some((index, _)) => &body[..index],
        None => &body[..],
    };

    title_from_html(head)
}

fn title_from_html(html: &str) -> Option<String> {
    // Lowercasing ASCII keeps byte indices aligned with the original.
    let haystack = html.to_ascii_lowercase();

    let open = haystack.find("<title")?;
    let start = open + html[open..].find('>')? + 1;
    let end = start + haystack[start..].find("</title")?;

    let title = decode_entities(html.get(start..end)?);
    let title = title.split_whitespace().collect::<Vec<_>>().join(" ");

    (!title.is_empty()).then_some(title)
}

fn decode_entities(text: &str) -> String {
    let mut decoded = String::with_capacity(text.len());
    let mut rest = text;

    while let Some(start) = rest.find('&') {
        decoded.push_str(&rest[..start]);
        rest = &rest[start..];

        let Some(end) = rest.find(';').filter(|end| *end <= 12) else {
            decoded.push('&');
            rest = &rest[1..];
            continue;
        };

        match decode_entity(&rest[1..end]) {
            Some(character) => decoded.push(character),
            None => decoded.push_str(&rest[..=end]),
        }

        rest = &rest[end + 1..];
    }

    decoded.push_str(rest);
    decoded
}

fn decode_entity(name: &str) -> Option<char> {
    let code_point = match name.to_ascii_lowercase().as_str() {
        "amp" => return Some('&'),
        "lt" => return Some('<'),
        "gt" => return Some('>'),
        "quot" => return Some('"'),
        "apos" => return Some('\''),
        "nbsp" => return Some(' '),
        "hellip" => return Some('…'),
        "mdash" => return Some('—'),
        "ndash" => return Some('–'),
        "laquo" => return Some('«'),
        "raquo" => return Some('»'),
        "rsquo" => return Some('’'),
        lowered => match lowered.strip_prefix('#') {
            Some(digits) => match digits.strip_prefix('x') {
                Some(hexadecimal) => u32::from_str_radix(hexadecimal, 16).ok()?,
                None => digits.parse().ok()?,
            },
            None => return None,
        },
    };

    char::from_u32(code_point)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn detects_bare_links_only() {
        assert_eq!(
            as_link(" https://date.now/article "),
            Some("https://date.now/article".to_string())
        );
        assert_eq!(as_link("https://date.now un texte"), None);
        assert_eq!(as_link("juste du texte"), None);
        assert_eq!(as_link("mailto:hello@date.now"), None);
    }

    #[test]
    fn extracts_and_decodes_the_title() {
        let html = "<html><head><title lang=\"fr\">Rust &amp; caf&#233;\n  bien</title></head>";

        assert_eq!(
            title_from_html(html),
            Some("Rust & café bien".to_string())
        );
    }

    #[test]
    fn ignores_pages_without_a_title() {
        assert_eq!(title_from_html("<html><head></head></html>"), None);
        assert_eq!(title_from_html("<title>   </title>"), None);
    }

    #[test]
    fn escapes_brackets_in_labels() {
        assert_eq!(escape_label("[Live] Rust\nnews"), "(Live) Rust news");
    }
}
