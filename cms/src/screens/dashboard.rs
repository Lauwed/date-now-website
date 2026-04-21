use iced::alignment::{Horizontal, Vertical};
use iced::font::Weight;
use iced::widget::{button, column, row, text};
use iced::{Element, Font, Length};

use crate::components::card::Card;

#[derive(Debug, Clone, Copy)]
pub enum Message {
    Increment,
    Decrement,
}

#[derive(Default)]
pub struct Dashboard {
    value: i64,
}

impl Dashboard {
    pub fn update(&mut self, message: Message) {
        match message {
            Message::Increment => {
                self.value += 1;
            }
            Message::Decrement => {
                self.value -= 1;
            }
        }
    }
    pub fn view(&self) -> Element<'_, Message> {
        let nb_subscribers = Card {
            body: column![
                text("Subscribers").font(Font {
                    weight: Weight::Black,
                    ..Default::default()
                }),
                text(250).size(40)
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
                text(81).size(40)
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
                text(3).size(40)
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
