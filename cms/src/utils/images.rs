use std::path::Path;

use iced::widget::image::Handle;

/// Rogne au carré centré et efface l'alpha hors du cercle inscrit, pour que
/// l'avatar s'affiche rond quel que soit le support de clipping du backend
/// (`Container::clip` d'iced ne découpe qu'un rectangle).
pub fn circular_png_bytes(bytes: &[u8]) -> Option<Vec<u8>> {
    let img = image::load_from_memory(bytes).ok()?.to_rgba8();
    let (width, height) = img.dimensions();
    let size = width.min(height);
    let x_offset = (width - size) / 2;
    let y_offset = (height - size) / 2;
    let mut cropped = image::imageops::crop_imm(&img, x_offset, y_offset, size, size).to_image();

    let center = size as f32 / 2.0;
    for (x, y, pixel) in cropped.enumerate_pixels_mut() {
        let dx = x as f32 + 0.5 - center;
        let dy = y as f32 + 0.5 - center;
        let dist = (dx * dx + dy * dy).sqrt();
        let alpha = ((center - dist).clamp(0.0, 1.0) * 255.0) as u8;
        pixel[3] = pixel[3].min(alpha);
    }

    let mut png_bytes = Vec::new();
    image::DynamicImage::ImageRgba8(cropped)
        .write_to(
            &mut std::io::Cursor::new(&mut png_bytes),
            image::ImageFormat::Png,
        )
        .ok()?;
    Some(png_bytes)
}

/// Décode l'image telle quelle. L'API sert du WebP, que le renderer d'iced ne
/// sait pas lire directement : on repasse donc systématiquement par le crate
/// `image` pour produire un PNG.
pub fn plain_png_bytes(bytes: &[u8]) -> Option<Vec<u8>> {
    let img = image::load_from_memory(bytes).ok()?;

    let mut png_bytes = Vec::new();
    img.write_to(
        &mut std::io::Cursor::new(&mut png_bytes),
        image::ImageFormat::Png,
    )
    .ok()?;
    Some(png_bytes)
}

fn handle_from(bytes: Vec<u8>, circular: bool) -> Option<Handle> {
    let decoded = if circular {
        circular_png_bytes(&bytes)?
    } else {
        plain_png_bytes(&bytes)?
    };
    Some(Handle::from_bytes(decoded))
}

/// Télécharge une image et en fait un handle iced. Bloquant, comme le reste
/// des accès réseau du CMS.
pub fn fetch_handle(url: &str, circular: bool) -> Option<Handle> {
    let bytes = reqwest::blocking::get(url).ok()?.bytes().ok()?;
    handle_from(bytes.to_vec(), circular)
}

/// Charge une image depuis le disque et en fait un handle iced.
pub fn load_handle(path: &Path, circular: bool) -> Option<Handle> {
    let bytes = std::fs::read(path).ok()?;
    handle_from(bytes, circular)
}

/// Extensions acceptées par l'API (le type réel est vérifié côté serveur par
/// magic bytes, ceci ne filtre que les évidences côté client).
pub const IMAGE_EXTENSIONS: [&str; 7] = ["png", "jpg", "jpeg", "gif", "webp", "bmp", "tiff"];

pub fn has_image_extension(path: &Path) -> bool {
    match path.extension().and_then(|ext| ext.to_str()) {
        Some(ext) => IMAGE_EXTENSIONS.contains(&ext.to_lowercase().as_str()),
        None => false,
    }
}
