use iced::widget::{button, container, row, text};
use iced::{Element, Font, Length, Settings, Task, Theme};
use std::borrow::Cow;
use std::env;
use std::sync::OnceLock;
use sysuri::{UriScheme, is_registered, parse_args, register};
use url::Url;

mod components;
mod data;
mod screens;
mod utils;

use components::nav::{self, Nav};
use components::toast::{Status, Toast};
use data::users::{User, get_current_user};
use screens::dashboard::Dashboard;
use screens::issue::Issue;
use screens::issues::Issues;
use screens::login::Login;

use crate::components::toast;
use crate::data::config::Config;
use crate::data::sessions::{
    Session, clear_session, load_session, refresh_access_token, save_session,
};
use crate::screens::new_issue::{self, NewIssue};
use crate::screens::{issue, issues};

static CONFIG: OnceLock<Config> = OnceLock::new();
pub fn g_config() -> &'static Config {
    CONFIG.get().unwrap()
}

#[derive(Default)]
struct State {
    dashboard: Dashboard,
    issues: Issues,
    issue: Issue,
    new_issue: NewIssue,
    login: Login,
    current_user: Option<Session>,
    nav: Nav,
    toasts: Vec<Toast>,
}

impl State {
    fn new() -> State {
        let mut state = State::default();
        let mut session_from_deep_link = false;

        if let Some(uri) = parse_args() {
            println!("Received URI: {}", uri);

            let url_result = Url::parse(&uri);

            match url_result {
                Ok(url) => {
                    if url.path() == "/login" {
                        let mut access_token = None::<String>;
                        let mut refresh_token = None::<String>;

                        let mut pairs = url.query_pairs();
                        if pairs.count() == 2 {
                            for _ in 1..3 {
                                match pairs.next() {
                                    Some((Cow::Borrowed("token"), token)) => {
                                        println!("token = {}", token);
                                        access_token = Some(token.to_string());
                                    }
                                    Some((Cow::Borrowed("refresh_token"), token)) => {
                                        println!("refresh_token = {}", token);
                                        refresh_token = Some(token.to_string());
                                    }
                                    Some((key, value)) => {
                                        println!("c pas bon fieu : {} = {}", key, value);
                                    }
                                    None => println!("nope"),
                                };
                            }

                            // Get current user
                            if let Some(at) = access_token
                                && let Some(rt) = refresh_token
                            {
                                if let Ok(user) = get_current_user(&at) {
                                    println!("USER CONNECTED: {}", user.email);
                                    let session = Session {
                                        token: at,
                                        refresh_token: rt,
                                        user,
                                    };

                                    save_session(&session);
                                    session_from_deep_link = true;

                                    state.current_user = Some(session);
                                } else {
                                    state.current_user = None;
                                }
                            }
                        } else {
                            eprintln!("manque des trucs dans ton url");
                        }
                    }
                }
                Err(e) => println!("parse url err {}", e),
            }
        }

        if !session_from_deep_link {
            if let Some(stored) = load_session() {
                match refresh_access_token(&stored.get_refresh_token()) {
                    Ok(pair) => match get_current_user(&pair.token) {
                        Ok(user) => {
                            let session = Session {
                                token: pair.token,
                                refresh_token: pair.refresh_token,
                                user,
                            };

                            save_session(&session);
                            state.current_user = Some(session);
                        }
                        Err(e) => {
                            eprintln!("user pas recup fieu: {}", e);
                            clear_session();
                        }
                    },
                    Err(e) => {
                        println!("refresh token plus frais mdr: {}", e);
                        clear_session();
                    }
                }
            }
        }

        state
    }
}

#[derive(Debug, Clone)]
enum Message {
    Dashboard(screens::dashboard::Message),
    Issues(screens::issues::Message),
    Issue(screens::issue::Message),
    NewIssue(screens::new_issue::Message),
    Login(screens::login::Message),
    Nav(components::nav::Message),
    DismissToast(usize),
}

fn push_toast(state: &mut State, title: String, content: String, status: Status) {
    state.toasts.push(Toast {
        title: title.into(),
        body: content.into(),
        status: status,
    });
}

fn update(state: &mut State, message: Message) -> Task<Message> {
    match message {
        Message::Dashboard(message) => {
            state.dashboard.update(message);
            Task::none()
        }
        Message::Issues(message) => {
            if let Some(session) = &state.current_user {
                match state.issues.update(message) {
                    issues::Action::None => Task::none(),
                    issues::Action::Run(task) => task.map(Message::Issues),
                    issues::Action::OpenIssue(id) => {
                        state.issue = Issue::new(id, session.clone());
                        state.nav.current_screen = nav::Screen::Issue(id);
                        Task::none()
                    }
                    issues::Action::NewIssue => {
                        state.new_issue = NewIssue::new(session.clone());
                        state.nav.current_screen = nav::Screen::NewIssue;
                        Task::none()
                    }
                }
            } else {
                Task::none()
            }
        }
        Message::Issue(message) => match state.issue.update(message) {
            issue::Action::BackToList => {
                state.issues = Issues::default();
                state.nav.current_screen = nav::Screen::Issues;
                Task::none()
            }
            issue::Action::Toast(title, content, status) => {
                push_toast(state, title, content, status);
                Task::none()
            }
            issue::Action::None => Task::none(),
        },
        Message::NewIssue(message) => match state.new_issue.update(message) {
            new_issue::Action::BackToList => {
                state.issues = Issues::default();
                state.nav.current_screen = nav::Screen::Issues;
                Task::none()
            }
            new_issue::Action::Toast(title, content, status) => {
                push_toast(state, title, content, status);
                Task::none()
            }
            new_issue::Action::None => Task::none(),
        },
        Message::Nav(message) => {
            let _ = state.nav.update(message);
            Task::none()
        }
        Message::Login(message) => {
            state.login.update(message);
            Task::none()
        }
        Message::DismissToast(index) => {
            state.toasts.remove(index);
            Task::none()
        }
    }
}

fn view(state: &State) -> Element<'_, Message> {
    match &state.current_user {
        Some(current_user) => {
            // Content
            let main_content = match state.nav.current_screen {
                nav::Screen::Dashboard => state.dashboard.view().map(Message::Dashboard),
                nav::Screen::Issues => state.issues.view().map(Message::Issues),
                nav::Screen::Issue(_) => state.issue.view().map(Message::Issue),
                nav::Screen::NewIssue => state.new_issue.view().map(Message::NewIssue),
                nav::Screen::Login => state.login.view().map(Message::Login),
            };
            let main_container = container(main_content).width(Length::FillPortion(5));

            // Return composed layout
            let content = row![
                state.nav.view(current_user).map(Message::Nav),
                main_container,
            ];

            toast::Manager::new(content, &state.toasts, Message::DismissToast)
                .timeout(toast::DEFAULT_TIMEOUT)
                .into()
        }
        None => state.login.view().map(Message::Login),
    }
}

fn theme(_state: &State) -> Theme {
    Theme::CatppuccinLatte
}

fn main() -> iced::Result {
    // Init global config instance
    match CONFIG.set(Config::default()) {
        Ok(()) => println!("Config successfully initiated."),
        Err(_) => panic!("An error occurred while initiating the global configuration."),
    };

    let config = g_config();
    match is_registered(&config.uri_scheme) {
        Ok(true) => println!("URI Scheme is already registered."),
        Ok(false) => {
            let exe = env::current_exe().unwrap();
            let scheme = UriScheme::new(
                &config.uri_scheme,
                &String::from("CMS of Date Now Newsletters"),
                exe,
            );
            register(&scheme).unwrap();
        }
        Err(e) => println!("Error while is_registered: {}", e),
    };

    let settings = Settings {
        default_font: Font::with_name("General Sans Variable"),
        ..Settings::default()
    };

    iced::application(State::new, update, view)
        .window(iced::window::Settings {
            size: iced::Size::new(1440.0, 900.0),
            ..iced::window::Settings::default()
        })
        .font(include_bytes!("../assets/fonts/GeneralSans-Variable.ttf").as_slice())
        .theme(theme)
        .settings(settings)
        .run()
}
