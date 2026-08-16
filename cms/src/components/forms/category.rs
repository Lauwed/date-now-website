use iced::{
    Alignment, Background, Border, Color, Element, Length,
    border::Radius,
    widget::{button, column, container, row},
};
use iced_aw::ColorPicker;
use std::str::FromStr;

use crate::{components::form_control::form_control, data::categories::Category};

#[derive(Default)]
pub struct CategoryForm {
    pub item: Category,
    pub color: Color,
    pub show_picker: bool,
}

#[derive(Debug, Clone)]
pub enum Message {
    NameChanged(String),
    ColorChanged(String),
    PickerOpen,
    PickerCancel,
    PickerSubmit(Color),
}

impl CategoryForm {
    pub fn new(item: Category) -> Self {
        Self {
            item: item.clone(),
            color: match Color::from_str(&item.color) {
                Ok(color) => color,
                Err(_) => Color::from_rgb(0.0, 0.0, 0.0),
            },
            show_picker: false,
        }
    }

    pub fn get_category(&self) -> Category {
        self.item.clone()
    }

    pub fn update(&mut self, message: Message) {
        match message {
            Message::NameChanged(value) => {
                self.item = Category {
                    name: value,
                    ..self.item.clone()
                };
            }
            Message::ColorChanged(value) => {
                self.item = Category {
                    color: value,
                    ..self.item.clone()
                };
            }
            Message::PickerOpen => {
                self.show_picker = true;
            }
            Message::PickerCancel => {
                self.show_picker = false;
            }
            Message::PickerSubmit(color) => {
                self.color = color;
                self.show_picker = false;

                self.item = Category {
                    color: color.to_string(),
                    ..self.item.clone()
                };
            }
        }
    }

    pub fn view(&self) -> Element<'_, Message> {
        let name_input = form_control(
            "Category",
            "category",
            &self.item.name.to_string(),
            Some(Message::NameChanged),
            Length::Fixed(400.0),
            None,
            None,
        );

        let color_input = form_control(
            "Color",
            "color",
            &self.item.color,
            Some(Message::ColorChanged),
            Length::Fill,
            None,
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

        column![name_input, color].spacing(10).padding(20).into()
    }
}
