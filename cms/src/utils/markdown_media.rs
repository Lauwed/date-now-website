use frostmark::MarkState;

use crate::components::markdown_editor::{LOCAL_SCHEME, local_path};
use crate::data::medias::{delete_media, find_media_by_url, upload_media_blocking};

/// URLs d'images citées par un markdown. Passe par le parseur de frostmark
/// plutôt que par une expression régulière, pour suivre exactement la syntaxe
/// que la prévisualisation interprète.
fn image_links(markdown: &str) -> Vec<String> {
    let mut links: Vec<String> = MarkState::with_html_and_markdown(markdown)
        .find_image_links()
        .into_iter()
        .collect();
    links.sort();
    links
}

/// Texte alternatif associé à une url dans `![alt](url)`, vide s'il n'y en a
/// pas. Sert de `textAlternatif` à l'envoi, que l'API exige non vide.
fn alt_for(markdown: &str, url: &str) -> Option<String> {
    let closing = format!("]({})", url);
    let close_at = markdown.find(&closing)?;
    let open_at = markdown[..close_at].rfind("![")?;

    Some(markdown[open_at + 2..close_at].to_string())
}

/// Envoie les images encore locales et renvoie le markdown pointant vers
/// leurs urls définitives.
///
/// Un seul échec interrompt tout et laisse le texte d'origine intact : mieux
/// vaut ne rien enregistrer qu'un contenu à moitié converti.
pub fn upload_local_images(markdown: &str, token: &str) -> Result<String, String> {
    let mut converted = markdown.to_string();

    for url in image_links(markdown) {
        let Some(path) = local_path(&url) else {
            continue;
        };

        let alt = alt_for(markdown, &url)
            .filter(|alt| !alt.trim().is_empty())
            .or_else(|| {
                path.file_stem()
                    .and_then(|stem| stem.to_str())
                    .map(|stem| stem.to_string())
            })
            .unwrap_or_else(|| String::from("Image"));

        let media = upload_media_blocking(&alt, path, token)
            .map_err(|e| format!("{}: {}", path.display(), e))?;

        let final_url = media
            .url
            .ok_or_else(|| format!("{}: the server returned no URL.", path.display()))?;

        converted = converted.replace(&url, &final_url);
    }

    Ok(converted)
}

/// Supprime les images qui étaient référencées avant l'édition et ne le sont
/// plus après. À n'appeler qu'**après** l'enregistrement : le garde-fou de
/// l'API lit le contenu en base pour décider si une image sert encore.
///
/// Les erreurs sont journalisées sans être remontées : le contenu, lui, est
/// déjà sauvegardé, et une image restée en trop ne casse rien.
pub fn delete_removed_images(before: &str, after: &str, token: &str) {
    let still_there = image_links(after);

    for url in image_links(before) {
        // Les liens locaux n'ont jamais atteint le serveur.
        if url.starts_with(LOCAL_SCHEME) || still_there.contains(&url) {
            continue;
        }

        match find_media_by_url(&url, token) {
            Ok(Some(media)) => {
                if let Err(e) = delete_media(media.id, token) {
                    eprintln!("Could not delete media {}: {}", media.id, e);
                }
            }
            Ok(None) => {}
            Err(e) => eprintln!("Could not resolve media {}: {}", url, e),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn finds_the_alt_text_of_a_link() {
        let md = "intro\n\n![Une photo](file:///tmp/a.png)\n";
        assert_eq!(
            alt_for(md, "file:///tmp/a.png").as_deref(),
            Some("Une photo")
        );
    }

    #[test]
    fn alt_is_none_when_the_url_is_absent() {
        assert_eq!(alt_for("![a](x.png)", "y.png"), None);
    }

    #[test]
    fn collects_image_links_from_markdown() {
        let md = "![a](file:///tmp/a.png)\n\ntexte [lien](https://ex.com)\n\n![b](https://ex.com/b.webp)";
        let links = image_links(md);

        assert!(links.contains(&String::from("file:///tmp/a.png")));
        assert!(links.contains(&String::from("https://ex.com/b.webp")));
        // Un lien ordinaire n'est pas une image.
        assert!(!links.contains(&String::from("https://ex.com")));
    }
}
