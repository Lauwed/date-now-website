use iced::Alignment::Center;
use iced::widget::space::horizontal;
use iced::widget::{button, column, container, row};
use iced::{Element, Task};

use crate::components;
use crate::components::table::{Table, TableActions, TableColumn};
use crate::components::toast::Status;
use crate::components::typography::{TypographyStyle, typography};
use crate::data::sessions::Session;
use crate::data::sponsors::{Sponsor, delete_sponsor, get_sponsors};

#[derive(Debug, Clone)]
pub enum Message {
    Table(crate::components::table::Message),
    NewSponsor,
}

pub enum Action {
    None,
    OpenSponsor(String),
    NewSponsor,
    Toast(String, String, Status),
}

pub struct Sponsors {
    table: Table<Sponsor>,
    pub session: Session,
}

impl Default for Sponsors {
    fn default() -> Self {
        let table: Table<Sponsor> = Table::new(vec![], None);

        Self {
            table,
            session: Session::default(),
        }
    }
}

impl Sponsors {
    pub fn new(session: Session) -> Self {
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
        ];

        let actions = Some(TableActions {
            edit: Some(String::from("name")),
            delete: Some(String::from("name")),
        });

        let mut table = Table::new(columns, actions);

        table.data = match get_sponsors() {
            Ok(i) => i.data,
            Err(e) => {
                eprintln!("Error: {}", e);
                vec![]
            }
        };

        println!("{}", &table.data.len());
        for t in &table.data {
            println!("{}", t);
        }

        Self { table, session }
    }
    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::Table(table_msg) => {
                println!("message table");
                match self.table.update(table_msg) {
                    components::table::Action::Edit(id) => Action::OpenSponsor(id),
                    components::table::Action::Delete(id) => {
                        match delete_sponsor(id, self.session.token.clone()) {
                            Ok(res) => {
                                self.table.data = match get_sponsors() {
                                    Ok(i) => i.data,
                                    Err(e) => {
                                        eprintln!("Error: {}", e);
                                        vec![]
                                    }
                                };

                                Action::Toast("Success".to_string(), res.message, Status::Success)
                            }
                            Err(err) => Action::Toast("Fail".to_string(), err, Status::Success),
                        }
                    }
                    _ => Action::None,
                }
            }
            Message::NewSponsor => Action::NewSponsor,
        }
    }
    pub fn view(&self) -> Element<'_, Message> {
        let content = column![
            row![
                typography(String::from("Sponsor"), TypographyStyle::Title),
                horizontal(),
                button("New sponsor").on_press(Message::NewSponsor)
            ]
            .align_y(Center),
            self.table.view().map(Message::Table)
        ]
        .padding(20);

        container(content).into()
    }
}
