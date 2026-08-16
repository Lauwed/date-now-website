use iced::Alignment::Center;
use iced::widget::space::horizontal;
use iced::widget::{button, column, container, row};
use iced::Element;

use crate::components;
use crate::components::table::{Table, TableActions, TableColumn};
use crate::components::typography::{TypographyStyle, typography};
use crate::data::feeds::{Feed, get_feed, get_feeds};
use crate::data::responses::Response;

#[derive(Debug, Clone)]
pub enum Message {
    Table(crate::components::table::Message),
    NewFeed,
}

pub enum Action {
    None,
    OpenFeed(String, Feed),
    NewFeed,
    DeleteFeed(String),
}

pub struct Feeds {
    table: Table<Feed>,
}

impl Default for Feeds {
    fn default() -> Self {
        let table: Table<Feed> = Table::new(vec![], None);

        Self { table }
    }
}

impl Feeds {
    pub fn new() -> Self {
        let columns: Vec<TableColumn> = vec![
            TableColumn {
                key: "name",
                name: "Name",
                width: None,
                render: false,
            },
            TableColumn {
                key: "link",
                name: "Link",
                width: None,
                render: false,
            },
            TableColumn {
                key: "is_rss_feed",
                name: "Type",
                width: None,
                render: true,
            },
        ];

        let actions = Some(TableActions {
            edit: Some(String::from("id")),
            delete: Some(String::from("id")),
        });

        let mut table = Table::new(columns, actions);

        table.data = match get_feeds() {
            Ok(i) => i.data,
            Err(e) => {
                eprintln!("Error: {}", e);
                vec![]
            }
        };

        Self { table }
    }
    pub fn reload_data(&mut self) {
        self.table.data = match get_feeds() {
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
                components::table::Action::Edit(id) => match get_feed(&id) {
                    Ok(Response::Success(i)) => Action::OpenFeed(id, i),
                    Ok(Response::Error(e)) => {
                        eprintln!("Error: {}", e);
                        Action::None
                    }
                    Err(e) => {
                        eprintln!("Error: {}", e);
                        Action::None
                    }
                },
                components::table::Action::Delete(id) => Action::DeleteFeed(id),
                _ => Action::None,
            },
            Message::NewFeed => Action::NewFeed,
        }
    }
    pub fn view(&self) -> Element<'_, Message> {
        let content = column![
            row![
                typography(String::from("Feeds"), TypographyStyle::Title),
                horizontal(),
                button("New feed").on_press(Message::NewFeed)
            ]
            .align_y(Center),
            self.table.view().map(Message::Table)
        ]
        .padding(20);

        container(content).into()
    }
}
