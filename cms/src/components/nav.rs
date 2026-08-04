use iced::alignment::Vertical;
use iced::font::Weight;
use iced::widget::{button, column, container, image, row, text};
use iced::{Color, Element, Font, Length, Shadow, Task, Theme};

use crate::Screen;
use crate::data::sessions::Session;

#[derive(Debug, Clone)]
pub enum Message {
    OnScreenPressed(Screen),
}

pub enum Action {
    GoToScreen(Screen),
    None,
}

#[derive(Default, Clone)]
pub struct Nav {
    pub session: Session,
}

impl Nav {
    fn nav_button(
        &self,
        label: &'static str,
        screen: &Screen,
        current_screen: &Screen,
    ) -> Element<'_, Message> {
        let is_active = screen == current_screen;

        let button_style = if is_active == true {
            button::subtle
        } else {
            button::text
        };

        button(label)
            .on_press_maybe(if is_active {
                None
            } else {
                Some(Message::OnScreenPressed(screen.clone()))
            })
            .width(Length::Fill)
            .style(button_style)
            .into()
    }

    pub fn update(&mut self, message: Message) -> Action {
        let Message::OnScreenPressed(screen) = message;
        Action::GoToScreen(screen)
    }
    pub fn view(&self, current_screen: &Screen) -> Element<'_, Message> {
        let title_font = Font {
            weight: Weight::Bold,
            ..Default::default()
        };

        let title = text("Date.now()")
            .size(20)
            .line_height(1.0)
            .font(title_font);
        let subtitle = text("CMS").size(16);
        let header = column![title, subtitle];

        let dashboard_button = self.nav_button("Dashboard", &Screen::Dashboard, current_screen);
        let issues_button = self.nav_button("Issues", &Screen::Issues, current_screen);
        let listing_button = self.nav_button("Listing", &Screen::Listing, current_screen);
        let tags_button = self.nav_button("Tags", &Screen::Tags, current_screen);
        let sponsors_button = self.nav_button("Sponsors", &Screen::Sponsors, current_screen);
        let nav_buttons = column![
            dashboard_button,
            issues_button,
            listing_button,
            tags_button,
            sponsors_button
        ]
        .height(Length::Fill);

        let username = match self.session.user.username.clone() {
            Some(u) => u,
            None => self.session.user.email.clone(),
        };
        let current_user_button = text(username)
            .width(Length::Fill)
            .size(14)
            .wrapping(text::Wrapping::Glyph);
        let settings_button = button(
            row![
                image("assets/icons/settings_w.png").height(18).width(18),
                text("Settings")
            ]
            .spacing(5)
            .align_y(Vertical::Center),
        )
        .width(Length::Fill);

        container(column![header, nav_buttons, current_user_button, settings_button].spacing(32))
            .height(Length::Fill)
            .width(Length::Fixed(200.0))
            .padding(20)
            .style(|theme: &Theme| {
                let palette = theme.extended_palette();

                container::Style::default()
                    .background(palette.background.weak.color)
                    .shadow(Shadow {
                        color: Color::BLACK,
                        offset: [0.0, 4.0].into(),
                        blur_radius: 1.0,
                    })
            })
            .into()
    }
}
