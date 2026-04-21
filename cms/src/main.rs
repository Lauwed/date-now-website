use iced::widget::{container, row};
use iced::{Element, Font, Length, Settings, Task, Theme};

mod components;
mod screens;

use components::nav::{self, Nav};
use screens::dashboard::Dashboard;
use screens::issues::Issues;

#[derive(Default)]
struct State {
    nav: Nav,
    dashboard: Dashboard,
    issues: Issues,
}

#[derive(Debug, Clone, Copy)]
enum Message {
    Dashboard(screens::dashboard::Message),
    Issues(screens::issues::Message),
    Nav(components::nav::Message),
}

fn update(state: &mut State, message: Message) -> Task<Message> {
    match message {
        Message::Dashboard(message) => {
            state.dashboard.update(message);
            Task::none()
        }
        Message::Issues(message) => {
            state.issues.update(message);
            Task::none()
        }
        Message::Nav(message) => {
            state.nav.update(message);
            Task::none()
        }
    }
}

fn view(state: &State) -> Element<'_, Message> {
    // Content
    let main_content = match state.nav.current_screen {
        nav::Screen::Dashboard => state.dashboard.view().map(Message::Dashboard),
        nav::Screen::Issues => state.issues.view().map(Message::Issues),
    };
    let main_container = container(main_content).width(Length::FillPortion(5));

    // Return composed layout
    row![state.nav.view().map(Message::Nav), main_container].into()
}

fn theme(_state: &State) -> Theme {
    Theme::CatppuccinLatte
}

fn main() -> iced::Result {
    let settings = Settings {
        default_font: Font::with_name("General Sans Variable"),
        ..Settings::default()
    };

    iced::application(State::default, update, view)
        .font(include_bytes!("../assets/fonts/GeneralSans-Variable.ttf").as_slice())
        .theme(theme)
        .settings(settings)
        .run()
}
