use std::path::PathBuf;

use iced::{
    Alignment, Element, Length,
    widget::{button, column, container, row, space::horizontal, text},
};
use iced::widget::image as iced_image;
use iced::widget::image::Handle;

use crate::{
    components::{form_control::form_control, toast::Status, typography::typography},
    utils::images::{fetch_handle, load_handle},
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
            .and_then(|url| fetch_handle(url, true));

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
                    .add_filter("Image", &crate::utils::images::IMAGE_EXTENSIONS)
                    .pick_file()
                {
                    self.pending_picture_handle = load_handle(&path, true);
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
                            .and_then(|url| fetch_handle(url, true));
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
