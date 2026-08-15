use iced::widget::{button, center, column, container, mouse_area, opaque, row, stack, text};
use iced::{Color, Element, Font, Length, Settings, Task, Theme};
use lucide_icons::LUCIDE_FONT_BYTES;
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

use crate::components::forms::tag::TagForm;
use crate::components::toast;
use crate::data::config::Config;
use crate::data::sessions::{
    Session, clear_session, load_session, refresh_access_token, save_session,
};
use crate::data::tags::Tag;
use crate::screens::listing::{self, Listing};
use crate::screens::new_issue::{self, NewIssue};
use crate::screens::sponsors::{self, Sponsors};
use crate::screens::tags::{self, Tags};
use crate::screens::{issue, issues};

static CONFIG: OnceLock<Config> = OnceLock::new();
pub fn g_config() -> &'static Config {
    CONFIG.get().unwrap()
}

#[derive(Debug, Clone, Default)]
enum ModalKind {
    #[default]
    None,
    ConfirmDeleteTag(String),
    EditTag(String),
    NewTag,
    ConfirmPublishIssue(u32),
    ConfirmArchiveIssue(u32),
    ConfirmDeleteUser(String),
}

#[derive(Default, Debug, Clone, PartialEq, Eq)]
pub enum Screen {
    #[default]
    Dashboard,
    Issues,
    Issue(u32),
    NewIssue,
    Listing,
    Login,
    Tags,
    Sponsors,
}

#[derive(Default)]
struct State {
    dashboard: Dashboard,
    issues: Issues,
    issue: Issue,
    new_issue: NewIssue,
    listing: Listing,
    login: Login,
    current_user: Option<Session>,
    current_screen: Screen,
    nav: Nav,
    tags: Tags,
    sponsors: Sponsors,
    toasts: Vec<Toast>,
    modal: ModalKind,
    tag_form: TagForm,
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
    Listing(screens::listing::Message),
    Login(screens::login::Message),
    Tags(screens::tags::Message),
    TagForm(components::forms::tag::Message),
    Sponsors(screens::sponsors::Message),
    Nav(components::nav::Message),
    DismissToast(usize),
    CloseModal,
    ConfirmDeleteTag(String),
    ConfirmEditTag(String),
    ConfirmNewTag,
    ConfirmPublishIssue(u32),
    ConfirmSendIssue(u32),
    ConfirmArchiveIssue(u32),
    ConfirmDeleteUser(String),
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
        Message::Tags(message) => match state.tags.update(message) {
            tags::Action::None => Task::none(),
            tags::Action::OpenTag(id, item) => {
                state.tag_form = TagForm::new(item.clone());
                state.modal = ModalKind::EditTag(id);
                Task::none()
            }
            tags::Action::NewTag => {
                state.tag_form = TagForm::new(Tag::default());
                state.modal = ModalKind::NewTag;
                Task::none()
            }
            tags::Action::DeleteTag(id) => {
                state.modal = ModalKind::ConfirmDeleteTag(id);
                Task::none()
            }
        },
        Message::TagForm(message) => {
            state.tag_form.update(message);
            Task::none()
        }
        Message::Issues(message) => {
            if let Some(session) = &state.current_user {
                match state.issues.update(message) {
                    issues::Action::None => Task::none(),
                    issues::Action::Run(task) => task.map(Message::Issues),
                    issues::Action::OpenIssue(id) => {
                        state.issue = Issue::new(id, session.clone());
                        state.current_screen = Screen::Issue(id);
                        Task::none()
                    }
                    issues::Action::NewIssue => {
                        state.new_issue = NewIssue::new(session.clone());
                        state.current_screen = Screen::NewIssue;
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
                state.current_screen = Screen::Issues;
                Task::none()
            }
            issue::Action::Toast(title, content, status) => {
                push_toast(state, title, content, status);
                Task::none()
            }
            issue::Action::ConfirmPublish(id) => {
                state.modal = ModalKind::ConfirmPublishIssue(id);
                Task::none()
            }
            issue::Action::ConfirmArchive(id) => {
                state.modal = ModalKind::ConfirmArchiveIssue(id);
                Task::none()
            }
            issue::Action::None => Task::none(),
        },
        Message::NewIssue(message) => match state.new_issue.update(message) {
            new_issue::Action::BackToList => {
                state.issues = Issues::default();
                state.current_screen = Screen::Issues;
                Task::none()
            }
            new_issue::Action::Toast(title, content, status) => {
                push_toast(state, title, content, status);
                Task::none()
            }
            new_issue::Action::None => Task::none(),
        },
        Message::Sponsors(message) => match state.sponsors.update(message) {
            sponsors::Action::None => Task::none(),
            sponsors::Action::OpenSponsor(_) => Task::none(),
            sponsors::Action::NewSponsor => Task::none(),
            sponsors::Action::Toast(title, content, status) => {
                push_toast(state, title, content, status);
                Task::none()
            }
        },
        Message::Listing(message) => match state.listing.update(message) {
            listing::Action::None => Task::none(),
            listing::Action::DeleteUser(id) => {
                state.modal = ModalKind::ConfirmDeleteUser(id);
                Task::none()
            }
        },
        Message::Nav(message) => {
            if let Some(session) = &state.current_user {
                match state.nav.update(message) {
                    nav::Action::GoToScreen(screen) => {
                        if screen == Screen::Sponsors {
                            state.sponsors = Sponsors::new(session.clone());
                        }
                        if screen == Screen::Tags {
                            state.tags = Tags::new();
                        }
                        if screen == Screen::Listing {
                            state.listing = Listing::new();
                        }
                        state.current_screen = screen;
                    }
                    nav::Action::None => (),
                }
            }
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
        Message::CloseModal => {
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmDeleteTag(id) => {
            if let Some(session) = &state.current_user {
                match crate::data::tags::delete_tag(id, session.token.clone()) {
                    Ok(_) => {
                        push_toast(
                            state,
                            "Success".to_string(),
                            "Le tag a été supprimé.".to_string(),
                            Status::Success,
                        );

                        state.tags.reload_data();
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmEditTag(id) => {
            if let Some(session) = &state.current_user {
                match crate::data::tags::update_tag(
                    &id,
                    state.tag_form.get_tag(),
                    session.token.clone(),
                ) {
                    Ok(_) => {
                        push_toast(
                            state,
                            "Success".to_string(),
                            "The tag has been successfully updated".to_string(),
                            Status::Success,
                        );

                        state.tags.reload_data();
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmNewTag => {
            if let Some(session) = &state.current_user {
                match crate::data::tags::create_tag(state.tag_form.get_tag(), session.token.clone())
                {
                    Ok(_) => {
                        push_toast(
                            state,
                            "Success".to_string(),
                            "The tag has been successfully created".to_string(),
                            Status::Success,
                        );

                        state.tags.reload_data();
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmPublishIssue(id) => {
            if let Some(session) = state.current_user.clone() {
                match crate::data::issues::publish_issue(id, session.token.clone()) {
                    Ok(res) => {
                        push_toast(state, "Success".to_string(), res.message, Status::Success);
                        state.issue = Issue::new(id, session);
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmSendIssue(id) => {
            if let Some(session) = state.current_user.clone() {
                match crate::data::issues::publish_issue(id, session.token.clone()) {
                    Ok(res) => {
                        push_toast(state, "Success".to_string(), res.message, Status::Success);
                        state.issue = Issue::new(id, session);
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmArchiveIssue(id) => {
            if let Some(session) = &state.current_user {
                if let Some(mut item) = state.issue.item.clone() {
                    item.status = crate::data::issues::IssueStatus::Archive;
                    match crate::data::issues::update_issue(id, item, session.token.clone()) {
                        Ok(crate::data::responses::Response::Success(new_issue)) => {
                            state.issue.item = Some(new_issue);
                            push_toast(
                                state,
                                "Success".to_string(),
                                "L'issue a été archivée.".to_string(),
                                Status::Success,
                            );
                        }
                        Ok(crate::data::responses::Response::Error(e)) => {
                            push_toast(state, "Error".to_string(), e.message, Status::Danger)
                        }
                        Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                    }
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmDeleteUser(id) => {
            if let Some(session) = &state.current_user {
                match crate::data::users::delete_user(id, session.token.clone()) {
                    Ok(_) => {
                        push_toast(
                            state,
                            "Success".to_string(),
                            "L'utilisateur a été supprimé.".to_string(),
                            Status::Success,
                        );

                        state.listing.reload_data();
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
    }
}

fn view(state: &State) -> Element<'_, Message> {
    match &state.current_user {
        Some(_) => {
            // Content
            let main_content = match state.current_screen {
                Screen::Dashboard => state.dashboard.view().map(Message::Dashboard),
                Screen::Issues => state.issues.view().map(Message::Issues),
                Screen::Issue(_) => state.issue.view().map(Message::Issue),
                Screen::NewIssue => state.new_issue.view().map(Message::NewIssue),
                Screen::Listing => state.listing.view().map(Message::Listing),
                Screen::Login => state.login.view().map(Message::Login),
                Screen::Tags => state.tags.view().map(Message::Tags),
                Screen::Sponsors => state.sponsors.view().map(Message::Sponsors),
            };
            let main_container = container(main_content).width(Length::FillPortion(5));

            // Return composed layout
            let content = row![
                state.nav.view(&state.current_screen).map(Message::Nav),
                main_container,
            ];

            // Layer the modal (if any) underneath the toast overlay, so that
            // `toast::Manager` stays the root widget across frames — keeping
            // a modal open/close from reshaping the tree at the exact moment
            // a toast is pushed, which was breaking the toast's close button.
            let content: Element<'_, Message> = match &state.modal {
                ModalKind::None => content.into(),
                ModalKind::ConfirmDeleteTag(id) => {
                    let modal_content: Element<'_, Message> =
                        column![text(format!("Supprimer le tag « {} » ?", id.clone())),].into();

                    components::modal::modal(
                        content,
                        Some("Confirmation".to_string()),
                        modal_content,
                        Message::CloseModal,
                        Some(Message::CloseModal),
                        Some(Message::ConfirmDeleteTag(id.clone())),
                    )
                }
                ModalKind::EditTag(id) => components::modal::modal(
                    content,
                    Some(format!("Tag {}", id)),
                    state.tag_form.view().map(Message::TagForm),
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::ConfirmEditTag(id.clone())),
                ),
                ModalKind::NewTag => components::modal::modal(
                    content,
                    Some("New tag".to_string()),
                    state.tag_form.view().map(Message::TagForm),
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::ConfirmNewTag),
                ),
                ModalKind::ConfirmPublishIssue(id) => {
                    let message = format!(
                        "Publish issue #{}? The newsletter will be automatically send",
                        id
                    );
                    components::modal::modal(
                        content,
                        Some("Confirmation".to_string()),
                        text(message).into(),
                        Message::CloseModal,
                        Some(Message::CloseModal),
                        Some(Message::ConfirmPublishIssue(*id)),
                    )
                }
                ModalKind::ConfirmArchiveIssue(id) => components::modal::modal(
                    content,
                    Some("Confirmation".to_string()),
                    text(format!("Archive issue #{}?", id)).into(),
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::ConfirmArchiveIssue(*id)),
                ),
                ModalKind::ConfirmDeleteUser(id) => {
                    let modal_content: Element<'_, Message> =
                        column![text(format!("Delete user #{}?", id.clone())),].into();

                    components::modal::modal(
                        content,
                        Some("Confirmation".to_string()),
                        modal_content,
                        Message::CloseModal,
                        Some(Message::CloseModal),
                        Some(Message::ConfirmDeleteUser(id.clone())),
                    )
                }
            };

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
        fonts: vec![LUCIDE_FONT_BYTES.into()],
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
