use std::path::PathBuf;

use iced::{
    Alignment, Element, Length,
    widget::{button, column, container, row, space::horizontal, text},
};
use iced::widget::image as iced_image;
use iced::widget::image::Handle;

use crate::{
    components::{form_control::form_control, toast::Status, typography::typography},
    data::{
        responses::Response,
        sessions::Session,
        users::{UpdateProfilePayload, User, update_user},
    },
};

#[derive(Default)]
pub struct Profile {
    session: Session,
    pending_picture: Option<PathBuf>,
    pending_picture_handle: Option<Handle>,
    picture_handle: Option<Handle>,
}

/// Crops to a centered square and clears the alpha of every pixel outside
/// the inscribed circle, so the avatar renders round regardless of backend
/// clipping support (iced's `Container::clip` only clips to a rectangle).
fn circular_png_bytes(bytes: &[u8]) -> Option<Vec<u8>> {
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

fn fetch_picture_handle(url: &str) -> Option<Handle> {
    let bytes = reqwest::blocking::get(url).ok()?.bytes().ok()?;
    let masked = circular_png_bytes(&bytes)?;
    Some(Handle::from_bytes(masked))
}

fn load_picture_handle(path: &std::path::Path) -> Option<Handle> {
    let bytes = std::fs::read(path).ok()?;
    let masked = circular_png_bytes(&bytes)?;
    Some(Handle::from_bytes(masked))
}

#[derive(Clone)]
pub enum Message {
    LinkChanged(String),
    Submit,
    PickPicture,
}

pub enum Action {
    Updated(User),
    Toast(String, String, Status),
    None,
}

impl Profile {
    pub fn new(session: Session) -> Self {
        let picture_handle = session
            .user
            .picture
            .as_ref()
            .and_then(|picture| picture.url.as_ref())
            .and_then(|url| fetch_picture_handle(url));

        Self {
            session,
            pending_picture: None,
            pending_picture_handle: None,
            picture_handle,
        }
    }

    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::LinkChanged(value) => {
                self.session.user.link = if value.is_empty() { None } else { Some(value) };
                Action::None
            }
            Message::PickPicture => {
                if let Some(path) = rfd::FileDialog::new()
                    .add_filter("Image", &["png", "jpg", "jpeg", "gif", "webp"])
                    .pick_file()
                {
                    self.pending_picture_handle = load_picture_handle(&path);
                    self.pending_picture = Some(path);
                }
                Action::None
            }
            Message::Submit => {
                let picture_id = if let Some(path) = &self.pending_picture {
                    match crate::data::medias::upload_media(
                        "Profile picture",
                        path,
                        self.session.token.clone(),
                    ) {
                        Ok(id) => Some(id),
                        Err(e) => {
                            return Action::Toast("Error".to_string(), e, Status::Danger);
                        }
                    }
                } else {
                    None
                };

                let payload = UpdateProfilePayload {
                    link: self.session.user.link.clone(),
                    picture_id,
                };

                match update_user(self.session.user.id, payload, self.session.token.clone()) {
                    Ok(Response::Success(user)) => {
                        self.pending_picture = None;
                        self.pending_picture_handle = None;
                        self.picture_handle = user
                            .picture
                            .as_ref()
                            .and_then(|picture| picture.url.as_ref())
                            .and_then(|url| fetch_picture_handle(url));
                        self.session.user = user.clone();
                        Action::Updated(user)
                    }
                    Ok(Response::Error(e)) => {
                        Action::Toast("Error".to_string(), e.message, Status::Danger)
                    }
                    Err(e) => Action::Toast("Error".to_string(), e, Status::Danger),
                }
            }
        }
    }
    pub fn view(&self) -> Element<'_, Message> {
        let title = typography(
            String::from("My profile"),
            crate::components::typography::TypographyStyle::Title,
        );

        let picture_preview: Element<'_, Message> =
            match self.pending_picture_handle.as_ref().or(self.picture_handle.as_ref()) {
                Some(handle) => iced_image(handle.clone()).width(120).height(120).into(),
                None => container(text("No picture")).width(120).height(120).into(),
            };
        let picture_frame = container(picture_preview)
            .width(Length::Fixed(120.0))
            .height(Length::Fixed(120.0));

        let picture_section = column![
            picture_frame,
            button("Change picture").on_press(Message::PickPicture),
        ]
        .spacing(10)
        .align_x(Alignment::Center);

        let link_input = form_control(
            "Link",
            "https://...",
            &self.session.user.link.as_ref().unwrap_or(&"".to_string()),
            Some(Message::LinkChanged),
            Length::Fill,
            None,
            None,
        );

        let submit_row = row![horizontal(), button("Save").on_press(Message::Submit)];

        column![title, picture_section, link_input, submit_row]
            .spacing(20)
            .padding(20)
            .into()
    }
}
