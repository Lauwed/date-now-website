use iced::alignment::Vertical;
use iced::font::Weight;
use iced::widget::{button, column, container, image, row, text};
use iced::{Color, Element, Font, Length, Shadow, Theme};

#[derive(Default, Debug, Clone, Copy, PartialEq, Eq)]
pub enum Screen {
    #[default]
    Dashboard,
    Issues,
}

#[derive(Debug, Clone, Copy)]
pub enum Message {
    OnScreenPressed(Screen),
}

#[derive(Default, Debug, Clone, Copy)]
pub struct Nav {
    pub current_screen: Screen,
}

impl Nav {
    fn nav_button(&self, label: &'static str, screen: Screen) -> Element<'_, Message> {
        let is_active = screen == self.current_screen;

        let button_style = if is_active == true {
            button::subtle
        } else {
            button::text
        };

        button(label)
            .on_press_maybe(if is_active {
                None
            } else {
                Some(Message::OnScreenPressed(screen))
            })
            .width(Length::Fill)
            .style(button_style)
            .into()
    }

    pub fn update(&mut self, message: Message) {
        match message {
            Message::OnScreenPressed(screen) => {
                self.current_screen = screen;
            }
        }
    }
    pub fn view(&self) -> Element<'_, Message> {
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

        let dashboard_button = self.nav_button("Dashboard", Screen::Dashboard);
        let issues_button = self.nav_button("Issues", Screen::Issues);
        let nav_buttons = column![dashboard_button, issues_button].height(Length::Fill);

        let settings_button = button(
            row![
                image("assets/icons/settings_w.png").height(18).width(18),
                text("Settings")
            ]
            .spacing(5)
            .align_y(Vertical::Center),
        )
        .width(Length::Fill);

        container(column![header, nav_buttons, settings_button].spacing(32))
            .height(Length::Fill)
            .width(Length::FillPortion(1))
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
