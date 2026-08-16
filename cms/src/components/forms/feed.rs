use iced::{
    Element, Length,
    widget::column,
};

use crate::{
    components::form_control::{form_control, form_control_switch},
    data::feeds::Feed,
};

#[derive(Default)]
pub struct FeedForm {
    pub item: Feed,
}

#[derive(Debug, Clone)]
pub enum Message {
    NameChanged(String),
    LinkChanged(String),
    IsRssFeedChanged(bool),
}

impl FeedForm {
    pub fn new(item: Feed) -> Self {
        Self { item }
    }

    pub fn get_feed(&self) -> Feed {
        self.item.clone()
    }

    pub fn update(&mut self, message: Message) {
        match message {
            Message::NameChanged(value) => {
                self.item = Feed {
                    name: value,
                    ..self.item.clone()
                };
            }
            Message::LinkChanged(value) => {
                self.item = Feed {
                    link: value,
                    ..self.item.clone()
                };
            }
            Message::IsRssFeedChanged(value) => {
                self.item = Feed {
                    is_rss_feed: value,
                    ..self.item.clone()
                };
            }
        }
    }

    pub fn view(&self) -> Element<'_, Message> {
        let name_input = form_control(
            "Name",
            "name",
            &self.item.name.to_string(),
            Some(Message::NameChanged),
            Length::Fill,
            None,
            None,
        );

        let link_input = form_control(
            "Link",
            "link",
            &self.item.link.to_string(),
            Some(Message::LinkChanged),
            Length::Fill,
            None,
            None,
        );

        let is_rss_feed_input = form_control_switch(
            "Is RSS feed",
            self.item.is_rss_feed,
            Some(Message::IsRssFeedChanged),
        );

        column![name_input, link_input, is_rss_feed_input]
            .spacing(10)
            .padding(20)
            .into()
    }
}
