use iced::Alignment::Center;
use iced::widget::space::horizontal;
use iced::widget::{button, column, container, row};
use iced::{Element, Task};

use crate::components;
use crate::components::table::{Table, TableActions, TableColumn};
use crate::components::typography::{TypographyStyle, typography};
use crate::data::issues::{Issue, get_issues};

#[derive(Debug, Clone)]
pub enum Message {
    Table(crate::components::table::Message),
    NewIssue,
}

pub struct Issues {
    table: Table<Issue>,
}

pub enum Action {
    None,
    Run(Task<Message>),
    OpenIssue(u32),
    NewIssue,
}

impl Default for Issues {
    fn default() -> Self {
        let columns: Vec<TableColumn> = vec![
            TableColumn {
                key: "title",
                name: "Title",
                width: None,
                render: false,
            },
            TableColumn {
                key: "issue_number",
                name: "Issue Number",
                width: Some(75),
                render: false,
            },
            TableColumn {
                key: "created_at",
                name: "Created at",
                width: Some(150),
                render: false,
            },
            TableColumn {
                key: "published_at",
                name: "Published at",
                width: Some(150),
                render: false,
            },
            TableColumn {
                key: "status",
                name: "Status",
                width: Some(100),
                render: true,
            },
            TableColumn {
                key: "tags",
                name: "tags",
                width: Some(200),
                render: true,
            },
            TableColumn {
                key: "views",
                name: "Views",
                width: Some(60),
                render: false,
            },
        ];

        let actions = Some(TableActions {
            edit: Some(String::from("id")),
            delete: None,
        });

        let mut table = Table::new(columns, actions);

        table.data = match get_issues() {
            Ok(i) => i.data,
            Err(e) => {
                eprintln!("Error: {}", e);
                vec![]
            }
        };

        Self { table }
    }
}

impl Issues {
    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::Table(table_msg) => {
                println!("message table");
                match self.table.update(table_msg) {
                    components::table::Action::Edit(val) => match val.parse::<u32>() {
                        Ok(id) => Action::OpenIssue(id),
                        Err(_) => Action::None,
                    },
                    _ => Action::None,
                }
            }
            Message::NewIssue => Action::NewIssue,
        }
    }
    pub fn view(&self) -> Element<'_, Message> {
        let content = column![
            row![
                typography(String::from("Issues"), TypographyStyle::Title),
                horizontal(),
                button("New issue").on_press(Message::NewIssue)
            ]
            .align_y(Center),
            self.table.view().map(Message::Table)
        ]
        .padding(20);

        container(content).into()
    }
}
