use iced::Alignment::Center;
use iced::widget::{Column, Row, button, column, container, row, scrollable, text};
use iced::{Background, Element, Length, Theme, border};

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
    pub edit: Option<String>,
    pub delete: Option<String>,
}

#[derive(Clone, Debug)]
pub enum Message {
    EditPressed(Option<String>),
    DeletePressed(Option<String>),
}

pub enum Action {
    None,
    Edit(String),
    Delete(String),
}

#[derive(Clone)]
pub struct Table<T> {
    pub data: Vec<T>,
    pub loading_id: Option<String>,
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
                self.loading_id = Some(id.clone());
                Action::Edit(id)
            }
            Message::DeletePressed(Some(id)) => {
                self.loading_id = Some(id.clone());
                Action::Delete(id)
            }
            _ => Action::None,
        }
    }

    pub fn view<'a>(&self) -> Element<'a, Message> {
        const ACTIONS_WIDTH: f32 = 75.0;
        let actions_width = if let Some(TableActions { edit, delete }) = &self.actions {
            if let Some(_) = *edit
                && let Some(_) = *delete
            {
                ACTIONS_WIDTH * 2.0
            } else {
                ACTIONS_WIDTH
            }
        } else {
            0.0
        };

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

        if let Some(_) = &self.actions {
            header = header.push(
                container(typography(String::from("Actions"), TypographyStyle::Body))
                    .padding([0, 10])
                    .width(Length::Fixed(actions_width)),
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
                        let mut actions_cell = row![].spacing(6);

                        if let Some(key) = edit {
                            actions_cell = actions_cell.push(button("Edit").on_press(
                                Message::EditPressed(Some(d.value_from_key(key).clone())),
                            ));
                        }

                        if let Some(key) = delete {
                            actions_cell = actions_cell.push(
                                button("Delete")
                                    .on_press(Message::DeletePressed(Some(
                                        d.value_from_key(key).clone(),
                                    )))
                                    .style(button::danger),
                            );
                        }

                        content = content.push(
                            container(actions_cell)
                                .padding([0, 10])
                                .width(Length::Fixed(actions_width)),
                        );
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
