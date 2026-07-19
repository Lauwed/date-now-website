use iced::Alignment::Center;
use iced::widget::{Column, Row, button, column, container, scrollable, text};
use iced::{Background, Element, Length, Task, Theme, border};

use crate::components::typography::{TypographyStyle, typography};
use crate::data::traits;

#[derive(Clone)]
pub struct TableColumn<'a> {
    pub key: &'a str,
    pub name: &'a str,
    pub width: Option<u64>,
    pub render: bool,
}

#[derive(Clone)]
pub struct TableActions {
    pub edit: bool,
    pub delete: bool,
}

#[derive(Copy, Clone, Debug)]
pub enum Message {
    EditPressed(Option<u32>),
    DeletePressed(Option<u32>),
}

pub enum Action {
    None,
    Edit(u32),
    Delete(u32),
}

#[derive(Clone)]
pub struct Table<T> {
    pub data: Vec<T>,
    pub loading_id: Option<u32>,
    columns: Vec<TableColumn<'static>>,
    actions: Option<TableActions>,
}

impl<T> Table<T>
where
    T: traits::Table,
{
    pub fn new(columns: Vec<TableColumn<'static>>, actions: Option<TableActions>) -> Self {
        Self {
            columns,
            actions,
            data: Vec::new(),
            loading_id: None,
        }
    }

    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::EditPressed(Some(id)) => {
                self.loading_id = Some(id);
                Action::Edit(id)
            }
            _ => Action::None,
        }
    }

    pub fn view<'a>(&self) -> Element<'a, Message> {
        const ACTIONS_WIDTH: Length = Length::Fixed(100.0);

        let mut header = self
            .columns
            .iter()
            .map(|col| {
                container(typography(col.name.to_string(), TypographyStyle::Body))
                    .padding([0, 10])
                    .width(match col.width {
                        Some(w) => Length::Fixed(w as f32),
                        None => Length::Fill,
                    })
                    .into()
            })
            .collect::<Row<'_, Message>>();

        if let Some(_) = self.actions {
            header = header.push(
                container(typography(String::from("Actions"), TypographyStyle::Body))
                    .padding([0, 10])
                    .width(ACTIONS_WIDTH),
            );
        }

        let header_container = container(header).padding(10).style(|theme: &Theme| {
            let palette = theme.extended_palette();

            container::Style {
                background: Some(Background::Color(palette.background.weak.color)),
                ..container::Style::default()
            }
        });

        let content = match self.data.len() {
            0 => column![text("No issues yet.")],
            _ => self
                .data
                .iter()
                .map(|d| {
                    let mut content = self
                        .columns
                        .iter()
                        .map(|col| {
                            container(if col.render {
                                d.render(col.key)
                            } else {
                                typography(d.value_from_key(col.key), TypographyStyle::Body)
                            })
                            .padding([0, 10])
                            .width(match col.width {
                                Some(w) => Length::Fixed(w as f32),
                                None => Length::Fill,
                            })
                            .into()
                        })
                        .collect::<Row<'_, Message>>()
                        .align_y(Center);

                    if let Some(TableActions { edit, delete }) = &self.actions {
                        if *edit == true {
                            content = content.push(
                                container(button("Edit").on_press(Message::EditPressed(
                                    match d.value_from_key("id").parse::<u32>() {
                                        Ok(id) => Some(id),
                                        Err(_) => {
                                            eprintln!(
                                                "Error while try parsing ID: {}",
                                                d.value_from_key("id")
                                            );
                                            None
                                        }
                                    },
                                )))
                                .padding([0, 10])
                                .width(ACTIONS_WIDTH),
                            );
                        }

                        if *delete == true {
                            content = content.push(
                                container(
                                    button("Delete")
                                        .on_press(Message::DeletePressed(
                                            match d.value_from_key("id").parse::<u32>() {
                                                Ok(id) => Some(id),
                                                Err(_) => None,
                                            },
                                        ))
                                        .style(button::danger),
                                )
                                .padding([0, 10])
                                .width(ACTIONS_WIDTH),
                            );
                        }
                    }

                    container(content).padding(10).width(Length::Fill).into()
                })
                .collect::<Column<'_, Message>>(),
        };

        let table_container = column![header_container, content];

        container(scrollable(table_container))
            .width(Length::Fill)
            .style(|theme: &Theme| {
                let palette = theme.extended_palette();

                container::Style::default().border(
                    border::rounded(10)
                        .width(2)
                        .color(palette.primary.base.color),
                )
            })
            .clip(true)
            .into()
    }
}
