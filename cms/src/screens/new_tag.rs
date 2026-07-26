use iced::{
    Alignment::{self, Center},
    Background, Border, Color, Element, Length,
    border::Radius,
    widget::{button, column, container, row, space::horizontal},
};
use iced_aw::ColorPicker;

use crate::{
    components::{form_control::form_control, toast::Status, typography::typography},
    data::{
        responses::Response,
        sessions::Session,
        tags::{Tag as TagType, create_tag, get_tag, update_tag},
    },
};

#[derive(Default)]
pub struct NewTag {
    pub item: TagType,
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

impl NewTag {
    pub fn new(session: Session) -> Self {
        Self {
            item: TagType {
                color: String::from("#000000"),
                name: String::from(""),
            },
            session,
            color: Color::from_rgb(0.0, 0.0, 0.0),
            show_picker: false,
        }
    }

    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::BackToList => Action::BackToList,
            Message::Submit => match create_tag(self.item.clone(), self.session.token.clone()) {
                Ok(Response::Success(new_tag)) => {
                    self.item = new_tag;

                    Action::BackToList
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
            },
            Message::NameChanged(value) => {
                self.item = TagType {
                    name: value,
                    ..self.item.clone()
                };
                Action::None
            }
            Message::ColorChanged(value) => {
                self.item = TagType {
                    color: value,
                    ..self.item.clone()
                };
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

                self.item = TagType {
                    color: color.to_string(),
                    ..self.item.clone()
                };

                Action::None
            }
        }
    }

    pub fn view(&self) -> Element<'_, Message> {
        let back_button = button("← Back to tags").on_press(Message::BackToList);
        let submit = button("Save").on_press(Message::Submit);
        let header = row![back_button, horizontal(), submit].align_y(Center);

        let title = typography(
            format!("New tag"),
            crate::components::typography::TypographyStyle::Title,
        );

        let name_input = form_control(
            "Name",
            "name",
            &self.item.name.to_string(),
            Some(Message::NameChanged),
            Length::Fixed(500.0),
            None,
        );

        let color_input = form_control(
            "Color",
            "color",
            &self.item.color,
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
    }
}
