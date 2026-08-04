use iced::Alignment::Center;
use iced::widget::{column, container, row};
use iced::{Element, Task};

use crate::components;
use crate::components::table::{Table, TableActions, TableColumn};
use crate::components::typography::{TypographyStyle, typography};
use crate::data::users::{User, get_users};

#[derive(Debug, Clone)]
pub enum Message {
    Table(crate::components::table::Message),
}

pub enum Action {
    None,
    DeleteUser(String),
}

pub struct Listing {
    table: Table<User>,
}

impl Default for Listing {
    fn default() -> Self {
        let table: Table<User> = Table::new(vec![], None);
        Self { table }
    }
}

impl Listing {
    pub fn new() -> Self {
        let columns: Vec<TableColumn> = vec![
            TableColumn {
                key: "email",
                name: "Email",
                width: None,
                render: false,
            },
            TableColumn {
                key: "role",
                name: "Role",
                width: None,
                render: false,
            },
            TableColumn {
                key: "subscribed_at",
                name: "Subscribed at",
                width: None,
                render: false,
            },
            TableColumn {
                key: "tracker_pixel_consent_date",
                name: "Pixel consent",
                width: None,
                render: false,
            },
        ];

        let actions = Some(TableActions {
            edit: None,
            delete: Some(String::from("id")),
        });

        let mut table = Table::new(columns, actions);
        table.data = match get_users() {
            Ok(i) => i.data,
            Err(e) => {
                eprintln!("Error: {}", e);
                vec![]
            }
        };

        Self { table }
    }

    pub fn reload_data(&mut self) {
        self.table.data = match get_users() {
            Ok(i) => i.data,
            Err(e) => {
                eprintln!("Error: {}", e);
                vec![]
            }
        };
    }

    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::Table(table_msg) => match self.table.update(table_msg) {
                components::table::Action::Delete(id) => Action::DeleteUser(id),
                _ => Action::None,
            },
        }
    }

    pub fn view(&self) -> Element<'_, Message> {
        let content = column![
            row![typography(String::from("Users"), TypographyStyle::Title)].align_y(Center),
            self.table.view().map(Message::Table)
        ]
        .padding(20);

        container(content).into()
    }
}
