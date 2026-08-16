use std::str::FromStr;

use iced::{
    Alignment::Center,
    Element, Length,
    widget::{button, column, combo_box, container, row, scrollable, space::horizontal, text},
};

use crate::{
    components::{
        badge::{BadgeStyle, badge},
        typography::{TypographyStyle, typography},
    },
    data::tags::Tag,
};

#[derive(Default)]
pub struct IssueTagsForm {
    pub issue_id: u32,
    pub tags: Vec<Tag>, // tags actuellement liés à l'issue (affichés dans la liste)
    all_tags: Vec<Tag>, // tous les tags dispo, pour reconstruire `available` après reset
    available: combo_box::State<Tag>,
    selected: Option<Tag>,
}

#[derive(Debug, Clone)]
pub enum Message {
    TagSelected(Tag),
    AddPressed,
    DeletePressed(String),
}

pub enum Action {
    None,
    Add(String),    // tag name à lier
    Delete(String), // tag name à délier
}

fn available_options(all_tags: &[Tag], linked: &[Tag]) -> Vec<Tag> {
    all_tags
        .iter()
        .filter(|t| !linked.iter().any(|linked_tag| linked_tag.name == t.name))
        .cloned()
        .collect()
}

impl IssueTagsForm {
    pub fn new(issue_id: u32, tags: Vec<Tag>, all_tags: Vec<Tag>) -> Self {
        let available = combo_box::State::new(available_options(&all_tags, &tags));

        Self {
            issue_id,
            tags,
            available,
            all_tags,
            selected: None,
        }
    }

    /// Met à jour les tags liés à l'issue (après un add/delete côté API) et
    /// retire du dropdown les tags désormais déjà liés.
    pub fn set_tags(&mut self, tags: Vec<Tag>) {
        self.tags = tags;
        self.available = combo_box::State::new(available_options(&self.all_tags, &self.tags));
        self.selected = None;
    }

    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::TagSelected(tag) => {
                self.selected = Some(tag);
                Action::None
            }
            Message::AddPressed => match self.selected.take() {
                Some(tag) => {
                    // reset du champ de recherche du dropdown
                    self.available =
                        combo_box::State::new(available_options(&self.all_tags, &self.tags));
                    Action::Add(tag.name)
                }
                None => Action::None,
            },
            Message::DeletePressed(name) => Action::Delete(name),
        }
    }

    pub fn view(&self) -> Element<'_, Message> {
        let add_row = row![
            combo_box(
                &self.available,
                "Search a tag...",
                self.selected.as_ref(),
                Message::TagSelected,
            )
            .width(Length::Fill),
            button("Add").on_press_maybe(self.selected.as_ref().map(|_| Message::AddPressed)),
        ]
        .align_y(Center)
        .spacing(10);

        let rows = self.tags.iter().map(|tag| {
            row![
                badge(
                    tag.name.clone(),
                    BadgeStyle::FromColor(match iced::Color::from_str(&tag.color) {
                        Ok(c) => c,
                        Err(_) => iced::Color::from_rgb(15.0 / 255.0, 15.0 / 255.0, 15.0 / 255.0),
                    }),
                ),
                horizontal(),
                button("Delete")
                    .on_press(Message::DeletePressed(tag.name.clone()))
                    .style(button::danger),
            ]
            .align_y(Center)
            .spacing(10)
            .into()
        });

        let list: Element<'_, Message> = if self.tags.is_empty() {
            typography("No tags linked yet.".to_string(), TypographyStyle::Small)
        } else {
            scrollable(column(rows).spacing(8)).into()
        };

        column![add_row, list]
            .spacing(16)
            .width(Length::Fixed(400.0))
            .into()
    }
}
