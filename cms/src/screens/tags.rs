use iced::Alignment::Center;
use iced::widget::space::horizontal;
use iced::widget::{button, column, container, row};
use iced::{Element, Task};

use crate::components;
use crate::components::table::{Table, TableActions, TableColumn};
use crate::components::toast::Status;
use crate::components::typography::{TypographyStyle, typography};
use crate::data::responses::Response;
use crate::data::sessions::Session;
use crate::data::tags::{Tag, delete_tag, get_tag, get_tags};

#[derive(Debug, Clone)]
pub enum Message {
    Table(crate::components::table::Message),
    NewTag,
}

pub enum Action {
    None,
    OpenTag(String, Tag),
    NewTag,
    DeleteTag(String),
}

pub struct Tags {
    table: Table<Tag>,
}

impl Default for Tags {
    fn default() -> Self {
        let table: Table<Tag> = Table::new(vec![], None);

        Self { table }
    }
}

impl Tags {
    pub fn new() -> Self {
        let columns: Vec<TableColumn> = vec![
            TableColumn {
                key: "name",
                name: "Name",
                width: None,
                render: false,
            },
            TableColumn {
                key: "color",
                name: "Color",
                width: None,
                render: true,
            },
        ];

        let actions = Some(TableActions {
            edit: Some(String::from("name")),
            delete: Some(String::from("name")),
        });

        let mut table = Table::new(columns, actions);

        table.data = match get_tags() {
            Ok(i) => i.data,
            Err(e) => {
                eprintln!("Error: {}", e);
                vec![]
            }
        };

        Self { table }
    }
    pub fn reload_data(&mut self) {
        self.table.data = match get_tags() {
            Ok(i) => i.data,
            Err(e) => {
                eprintln!("Error: {}", e);
                vec![]
            }
        };
    }
    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::Table(table_msg) => {
                println!("message table");
                match self.table.update(table_msg) {
                    components::table::Action::Edit(id) => match get_tag(&id) {
                        Ok(Response::Success(i)) => Action::OpenTag(id, i),
                        Ok(Response::Error(e)) => {
                            eprintln!("Error: {}", e);
                            Action::None
                        }
                        Err(e) => {
                            eprintln!("Error: {}", e);
                            Action::None
                        }
                    },
                    components::table::Action::Delete(id) => Action::DeleteTag(id),
                    _ => Action::None,
                }
            }
            Message::NewTag => Action::NewTag,
        }
    }
    pub fn view(&self) -> Element<'_, Message> {
        let content = column![
            row![
                typography(String::from("Tags"), TypographyStyle::Title),
                horizontal(),
                button("New tag").on_press(Message::NewTag)
            ]
            .align_y(Center),
            self.table.view().map(Message::Table)
        ]
        .padding(20);

        container(content).into()
    }
}
