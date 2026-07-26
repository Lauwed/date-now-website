use iced::{
    Alignment::{self, Center},
    Background, Border, Color, Element, Length,
    border::Radius,
    widget::{button, column, container, row, space::horizontal},
};
use iced_aw::ColorPicker;
use std::str::FromStr;

use crate::{
    components::{form_control::form_control, toast::Status, typography::typography},
    data::{
        responses::Response,
        sessions::Session,
        tags::{Tag as TagType, get_tag, update_tag},
    },
};

#[derive(Default)]
pub struct Tag {
    pub id: String,
    pub item: Option<TagType>,
    pub session: Session,
    pub color: Color,
    pub show_picker: bool,
}

#[derive(Debug, Clone)]
pub enum Message {
    BackToList,
    Submit,
    NameChanged(String),
    ColorChanged(String),
    PickerOpen,
    PickerCancel,
    PickerSubmit(Color),
}

pub enum Action {
    None,
    BackToList,
    Toast(String, String, Status),
}

impl Tag {
    pub fn new(id: String, session: Session) -> Self {
        let item = match get_tag(&id) {
            Ok(Response::Success(i)) => Some(i),
            Ok(Response::Error(e)) => {
                eprintln!("Error: {}", e);
                None
            }
            Err(e) => {
                eprintln!("Error: {}", e);
                None
            }
        };

        Self {
            id,
            item: item.clone(),
            session,
            color: match item {
                Some(i) => match Color::from_str(&i.color) {
                    Ok(color) => color,
                    Err(_) => Color::from_rgb(0.0, 0.0, 0.0),
                },
                None => Color::from_rgb(0.0, 0.0, 0.0),
            },
            show_picker: false,
        }
    }

    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::BackToList => Action::BackToList,
            Message::Submit => {
                if let Some(tag) = &mut self.item {
                    // Save in API
                    match update_tag(&self.id, tag.clone(), self.session.token.clone()) {
                        Ok(Response::Success(new_tag)) => {
                            self.item = Some(new_tag);

                            Action::Toast(
                                "Success".to_string(),
                                "The tag has been successfully saved.".to_string(),
                                Status::Success,
                            )
                        }
                        Ok(Response::Error(parse_err)) => Action::Toast(
                            "Error".to_string(),
                            format!("Parsing error: {}", parse_err),
                            Status::Danger,
                        ),
                        Err(err) => Action::Toast(
                            "Error".to_string(),
                            format!("Fetch error: {}", err),
                            Status::Danger,
                        ),
                    }
                } else {
                    Action::Toast("Failed".to_string(), "No tags".to_string(), Status::Danger)
                }
            }
            Message::NameChanged(value) => {
                if let Some(item) = &self.item {
                    self.item = Some(TagType {
                        name: value,
                        ..item.clone()
                    });
                }
                Action::None
            }
            Message::ColorChanged(value) => {
                if let Some(item) = &self.item {
                    self.item = Some(TagType {
                        color: value,
                        ..item.clone()
                    });
                }
                Action::None
            }
            Message::PickerOpen => {
                self.show_picker = true;
                Action::None
            }
            Message::PickerCancel => {
                self.show_picker = false;
                Action::None
            }
            Message::PickerSubmit(color) => {
                self.color = color;
                self.show_picker = false;

                self.item = match &self.item {
                    Some(i) => Some(TagType {
                        color: color.to_string(),
                        ..i.clone()
                    }),
                    None => None,
                };

                Action::None
            }
        }
    }

    pub fn view(&self) -> Element<'_, Message> {
        if let Some(item) = &self.item {
            let back_button = button("← Back to tags").on_press(Message::BackToList);
            let submit = button("Save").on_press(Message::Submit);
            let header = row![back_button, horizontal(), submit].align_y(Center);

            let title = typography(
                format!("{}", item.name),
                crate::components::typography::TypographyStyle::Title,
            );

            let name_input = form_control(
                "Tag Number",
                "tag number",
                &item.name.to_string(),
                Some(Message::NameChanged),
                Length::Fixed(400.0),
                None,
            );

            let color_input = form_control(
                "Color",
                "color",
                &item.color,
                Some(Message::ColorChanged),
                Length::Fill,
                None,
            );
            let color_picker = ColorPicker::new(
                self.show_picker,
                self.color,
                button("Select color").on_press(Message::PickerOpen),
                Message::PickerCancel,
                Message::PickerSubmit,
            );
            let color_dot = container("")
                .width(Length::Fixed(30.0))
                .height(Length::Fixed(30.0))
                .style(|_theme| container::Style {
                    background: Some(Background::Color(self.color)),
                    border: Border {
                        color: Color::from_rgb(0.0, 0.0, 0.0),
                        width: 1.0,
                        radius: Radius::new(100.0),
                    },
                    ..container::Style::default()
                });
            let color = row![color_picker, color_dot, color_input]
                .align_y(Alignment::End)
                .spacing(10);

            let form = row![name_input, color].spacing(10);

            return container(column![header, title, form].spacing(6))
                .padding(20)
                .into();
        } else {
            // TODO Redirection
            return container("Error").into();
        }
    }
}
