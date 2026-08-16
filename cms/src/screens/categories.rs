use iced::Alignment::Center;
use iced::widget::space::horizontal;
use iced::widget::{button, column, container, row};
use iced::Element;

use crate::components;
use crate::components::table::{Table, TableActions, TableColumn};
use crate::components::typography::{TypographyStyle, typography};
use crate::data::categories::{Category, get_categories, get_category};
use crate::data::responses::Response;

#[derive(Debug, Clone)]
pub enum Message {
    Table(crate::components::table::Message),
    NewCategory,
}

pub enum Action {
    None,
    OpenCategory(String, Category),
    NewCategory,
    DeleteCategory(String),
}

pub struct Categories {
    table: Table<Category>,
}

impl Default for Categories {
    fn default() -> Self {
        let table: Table<Category> = Table::new(vec![], None);

        Self { table }
    }
}

impl Categories {
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

        table.data = match get_categories() {
            Ok(i) => i.data,
            Err(e) => {
                eprintln!("Error: {}", e);
                vec![]
            }
        };

        Self { table }
    }
    pub fn reload_data(&mut self) {
        self.table.data = match get_categories() {
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
                components::table::Action::Edit(id) => match get_category(&id) {
                    Ok(Response::Success(i)) => Action::OpenCategory(id, i),
                    Ok(Response::Error(e)) => {
                        eprintln!("Error: {}", e);
                        Action::None
                    }
                    Err(e) => {
                        eprintln!("Error: {}", e);
                        Action::None
                    }
                },
                components::table::Action::Delete(id) => Action::DeleteCategory(id),
                _ => Action::None,
            },
            Message::NewCategory => Action::NewCategory,
        }
    }
    pub fn view(&self) -> Element<'_, Message> {
        let content = column![
            row![
                typography(String::from("Categories"), TypographyStyle::Title),
                horizontal(),
                button("New category").on_press(Message::NewCategory)
            ]
            .align_y(Center),
            self.table.view().map(Message::Table)
        ]
        .padding(20);

        container(content).into()
    }
}
