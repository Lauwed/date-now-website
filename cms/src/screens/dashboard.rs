use iced::alignment::{Horizontal, Vertical};
use iced::font::Weight;
use iced::widget::{button, column, row, text};
use iced::{Element, Font, Length};

use crate::components::card::Card;
use crate::data::users::get_nb_subscribers;

#[derive(Debug, Clone, Copy)]
pub enum Message {
    Increment,
    Decrement,
}

#[derive(Default)]
pub struct Dashboard {
    nb_subscribers: u8,
    nb_published_issues: u8,
    nb_draft_issues: u8,
    loaded: bool,
}

impl Dashboard {
    pub fn update(&mut self, message: Message) {
        match message {
            Message::Increment => {}
            Message::Decrement => {}
        }
    }
    pub fn view(&self) -> Element<'_, Message> {
        // if (!self.loaded) {
        //     nb_subscribers = match xx {
        //      Ok(count) => count,
        //      Err(e) => {
        //          0
        //      }
        //
        //     self.loaded = true;
        //     }
        // }

        let nb_subscribers = Card {
            body: column![
                text("Subscribers").font(Font {
                    weight: Weight::Black,
                    ..Default::default()
                }),
                text(self.nb_subscribers).size(40),
            ]
            .align_x(Horizontal::Center)
            .into(),
        };
        let nb_published_issues = Card {
            body: column![
                text("Published Issues").font(Font {
                    weight: Weight::Black,
                    ..Default::default()
                }),
                text(self.nb_published_issues).size(40)
            ]
            .align_x(Horizontal::Center)
            .into(),
        };
        let nb_draft_issues = Card {
            body: column![
                text("Draft Issues").font(Font {
                    weight: Weight::Black,
                    ..Default::default()
                }),
                text(self.nb_draft_issues).size(40)
            ]
            .align_x(Horizontal::Center)
            .into(),
        };

        let quick_stats = column![
            row![
                nb_subscribers.view(),
                nb_published_issues.view(),
                nb_draft_issues.view()
            ]
            .align_y(Vertical::Center)
            .spacing(60)
        ]
        .width(Length::Fill)
        .align_x(Horizontal::Center);

        column![quick_stats].padding(20).into()
    }
}
