//! Reconnaissance des URLs qui méritent un embed plutôt qu'un lien.
//!
//! Le rendu final revient aux renderers (CMS et front) : ici on se contente
//! de produire la directive markdown `::plateforme[url]`.

use url::Url;

/// Nom de directive pour une URL, `None` si la plateforme n'est pas reconnue
/// ou si l'URL ne pointe pas vers un contenu embarquable (un profil ou une
/// page d'accueil reste un lien ordinaire).
pub fn platform_for(url: &str) -> Option<&'static str> {
    let parsed = Url::parse(url).ok()?;
    let host = parsed.host_str()?.trim_start_matches("www.").to_lowercase();
    let segments: Vec<&str> = parsed
        .path_segments()
        .map(|s| s.filter(|segment| !segment.is_empty()).collect())
        .unwrap_or_default();

    match host.as_str() {
        "youtu.be" => (!segments.is_empty()).then_some("youtube"),
        "youtube.com" | "m.youtube.com" | "music.youtube.com" | "youtube-nocookie.com" => {
            match segments.first().copied() {
                Some("watch") => parsed
                    .query_pairs()
                    .any(|(key, _)| key == "v")
                    .then_some("youtube"),
                Some("shorts") | Some("live") | Some("embed") => {
                    (segments.len() > 1).then_some("youtube")
                }
                _ => None,
            }
        }
        "instagram.com" => match segments.first().copied() {
            Some("p") | Some("reel") | Some("reels") | Some("tv") => {
                (segments.len() > 1).then_some("instagram")
            }
            _ => None,
        },
        "bsky.app" => (segments.len() >= 4
            && segments[0] == "profile"
            && segments[2] == "post")
            .then_some("bluesky"),
        "twitter.com" | "x.com" => (segments.len() >= 3 && segments[1] == "status")
            .then_some("tweet"),
        // Mastodon est fédéré : aucune liste d'hôtes ne tiendrait. On
        // reconnaît la forme d'une URL de statut, qui est stable d'une
        // instance à l'autre.
        _ => mastodon_status(&segments).then_some("mastodon"),
    }
}

/// `/@utilisateur/123456789` ou `/users/utilisateur/statuses/123456789`.
fn mastodon_status(segments: &[&str]) -> bool {
    let is_status_id =
        |value: &str| value.len() >= 6 && value.chars().all(|c| c.is_ascii_digit());

    match segments {
        [user, id] => user.starts_with('@') && user.len() > 1 && is_status_id(id),
        [users, _, statuses, id] => *users == "users" && *statuses == "statuses" && is_status_id(id),
        _ => false,
    }
}

/// Directive markdown complète, prête à être insérée.
pub fn directive_for(url: &str) -> Option<String> {
    platform_for(url).map(|platform| format!("::{}[{}]", platform, url))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn recognises_youtube_in_its_many_shapes() {
        for url in [
            "https://www.youtube.com/watch?v=dQw4w9WgXcQ",
            "https://youtu.be/dQw4w9WgXcQ",
            "https://www.youtube.com/shorts/abc123",
            "https://m.youtube.com/watch?v=abc&t=42",
        ] {
            assert_eq!(platform_for(url), Some("youtube"), "{url}");
        }
    }

    #[test]
    fn recognises_the_other_platforms() {
        assert_eq!(
            platform_for("https://www.instagram.com/p/CxYzAbC/"),
            Some("instagram")
        );
        assert_eq!(
            platform_for("https://www.instagram.com/reel/CxYzAbC/"),
            Some("instagram")
        );
        assert_eq!(
            platform_for("https://bsky.app/profile/laura.bsky.social/post/3kabc"),
            Some("bluesky")
        );
        assert_eq!(
            platform_for("https://x.com/devgirl/status/1234567890"),
            Some("tweet")
        );
        assert_eq!(
            platform_for("https://mastodon.social/@devgirl/109876543210"),
            Some("mastodon")
        );
        assert_eq!(
            platform_for("https://piaille.fr/users/devgirl/statuses/109876543210"),
            Some("mastodon")
        );
    }

    /// Une page qui n'est pas un contenu embarquable reste un lien.
    #[test]
    fn leaves_ordinary_pages_alone() {
        for url in [
            "https://www.youtube.com/",
            "https://www.youtube.com/@devgirl",
            "https://www.instagram.com/devgirl/",
            "https://bsky.app/profile/laura.bsky.social",
            "https://x.com/devgirl",
            "https://lemonde.fr/article/123456",
            "https://date-now.lauradurieux.dev/issue/i-3-webassembly",
        ] {
            assert_eq!(platform_for(url), None, "{url}");
        }
    }

    #[test]
    fn builds_the_directive() {
        assert_eq!(
            directive_for("https://youtu.be/abc").as_deref(),
            Some("::youtube[https://youtu.be/abc]")
        );
        assert_eq!(directive_for("https://lemonde.fr/a"), None);
    }
}
