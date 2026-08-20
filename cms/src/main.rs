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

use crate::components::forms::article::ArticleForm;
use crate::components::forms::category::CategoryForm;
use crate::components::forms::feed::FeedForm;
use crate::components::forms::issue_section::SectionForm;
use crate::components::forms::issue_tags::IssueTagsForm;
use crate::components::forms::tag::TagForm;
use crate::components::toast;
use crate::data::categories::Category;
use crate::data::config::Config;
use crate::data::feeds::Feed;
use crate::data::sessions::{
    Session, clear_session, load_session, refresh_access_token, save_session,
};
use crate::data::tags::Tag;
use crate::screens::categories::{self, Categories};
use crate::screens::feeds::{self, Feeds};
use crate::screens::listing::{self, Listing};
use crate::screens::new_issue::{self, NewIssue};
use crate::screens::profile::{self, Profile};
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
    EditIssueTags(u32),
    ConfirmDeleteCategory(String),
    EditCategory(String),
    NewCategory,
    ConfirmDeleteFeed(String),
    EditFeed(String),
    NewFeed,
    NewIssueSection(u32),
    EditIssueSection(u32, u32),
    ConfirmDeleteIssueSection(u32, u32),
    NewArticle(u32, u32),
    EditArticle(u32, u32, u32),
    ConfirmDeleteArticle(u32, u32, u32),
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
    Categories,
    Feeds,
    Profile,
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
    issue_tags_form: IssueTagsForm,
    categories: Categories,
    category_form: CategoryForm,
    feeds: Feeds,
    feed_form: FeedForm,
    profile: Profile,
    section_form: SectionForm,
    article_form: ArticleForm,
    /// Un fichier survole la fenêtre et un éditeur markdown peut le recevoir.
    file_hovering: bool,
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
                println!("STORED SESSION LOADED");
                match refresh_access_token(&stored.get_refresh_token()) {
                    Ok(pair) => match get_current_user(&pair.token) {
                        Ok(user) => {
                            println!("CURRENT USER SUCCESS");
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

#[derive(Clone)]
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
    Profile(screens::profile::Message),
    Nav(components::nav::Message),
    DismissToast(usize),
    CloseModal,
    ConfirmDeleteTag(String),
    ConfirmEditTag(String),
    ConfirmNewTag,
    ConfirmPublishIssue(u32),
    ConfirmArchiveIssue(u32),
    ConfirmDeleteUser(String),
    Categories(screens::categories::Message),
    CategoryForm(components::forms::category::Message),
    ConfirmDeleteCategory(String),
    ConfirmEditCategory(String),
    ConfirmNewCategory,
    Feeds(screens::feeds::Message),
    FeedForm(components::forms::feed::Message),
    ConfirmDeleteFeed(String),
    ConfirmEditFeed(String),
    ConfirmNewFeed,
    IssueTagsForm(components::forms::issue_tags::Message),
    AddIssueTag(String),
    RemoveIssueTag(String),
    SectionForm(components::forms::issue_section::Message),
    ArticleForm(components::forms::article::Message),
    SaveSection,
    DeleteSection(u32, u32),
    ReorderSections(u32, Vec<u32>),
    SaveArticle,
    DeleteArticle(u32, u32, u32),
    ReorderArticles(u32, u32, Vec<u32>),
    FileHovered,
    FileHoverLeft,
    FileDropped(std::path::PathBuf),
}

/// Recharge l'issue courante depuis l'API — les sections et leurs articles
/// sont embarqués dans la réponse, un seul appel suffit.
fn refresh_issue(state: &mut State, issue_id: u32) {
    let token = match &state.current_user {
        Some(session) => session.token.clone(),
        None => return,
    };

    if let Ok(crate::data::responses::Response::Success(new_issue)) =
        crate::data::issues::get_issue(issue_id, &token)
    {
        state.issue.item = Some(new_issue);
    }
}

fn all_categories() -> Vec<crate::data::categories::Category> {
    match crate::data::categories::get_categories() {
        Ok(res) => res.data,
        Err(e) => {
            eprintln!("Error: {}", e);
            vec![]
        }
    }
}

/// Toast, rechargement de l'issue et fermeture de la modale après une
/// mutation du contenu.
fn handle_content_result<T>(
    state: &mut State,
    issue_id: u32,
    result: Result<T, String>,
    success: &str,
) {
    match result {
        Ok(_) => {
            refresh_issue(state, issue_id);
            state.modal = ModalKind::None;
            push_toast(
                state,
                "Success".to_string(),
                success.to_string(),
                Status::Success,
            );
        }
        Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
    }
}

/// Un éditeur markdown n'est visible que dans les modales de section texte et
/// d'article : ailleurs, un fichier déposé n'a nulle part où aller.
fn markdown_editor_on_screen(state: &State) -> bool {
    match state.modal {
        ModalKind::NewArticle(_, _) | ModalKind::EditArticle(_, _, _) => true,
        ModalKind::NewIssueSection(_) | ModalKind::EditIssueSection(_, _) => {
            state.section_form.is_text()
        }
        _ => false,
    }
}

/// Envoie les images locales du markdown et le réécrit avec leurs urls
/// définitives. `false` si un envoi a échoué : la sauvegarde doit alors être
/// abandonnée, jamais menée à moitié.
fn sync_images_before_save(
    state: &mut State,
    token: &str,
    markdown: String,
    apply: fn(&mut State, &str),
) -> bool {
    match crate::utils::markdown_media::upload_local_images(&markdown, token) {
        Ok(converted) => {
            if converted != markdown {
                apply(state, &converted);
            }
            true
        }
        Err(e) => {
            push_toast(state, "Error".to_string(), e, Status::Danger);
            false
        }
    }
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
        Message::Categories(message) => match state.categories.update(message) {
            categories::Action::None => Task::none(),
            categories::Action::OpenCategory(id, item) => {
                state.category_form = CategoryForm::new(item.clone());
                state.modal = ModalKind::EditCategory(id);
                Task::none()
            }
            categories::Action::NewCategory => {
                state.category_form = CategoryForm::new(Category::default());
                state.modal = ModalKind::NewCategory;
                Task::none()
            }
            categories::Action::DeleteCategory(id) => {
                state.modal = ModalKind::ConfirmDeleteCategory(id);
                Task::none()
            }
        },
        Message::CategoryForm(message) => {
            state.category_form.update(message);
            Task::none()
        }
        Message::Feeds(message) => match state.feeds.update(message) {
            feeds::Action::None => Task::none(),
            feeds::Action::OpenFeed(id, item) => {
                state.feed_form = FeedForm::new(item.clone());
                state.modal = ModalKind::EditFeed(id);
                Task::none()
            }
            feeds::Action::NewFeed => {
                state.feed_form = FeedForm::new(Feed::default());
                state.modal = ModalKind::NewFeed;
                Task::none()
            }
            feeds::Action::DeleteFeed(id) => {
                state.modal = ModalKind::ConfirmDeleteFeed(id);
                Task::none()
            }
        },
        Message::FeedForm(message) => {
            state.feed_form.update(message);
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
                if let Some(session) = &state.current_user {
                    state.issues = Issues::new(session);
                }
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
            issue::Action::Preview(id) => {
                let (token, slug, published) = match (&state.current_user, &state.issue.item) {
                    (Some(session), Some(item)) => (
                        session.token.clone(),
                        item.slug.clone(),
                        item.status == crate::data::issues::IssueStatus::Published,
                    ),
                    _ => return Task::none(),
                };

                let config = g_config();

                // Une issue publiée est lisible par tout le monde : pas besoin
                // d'un jeton, on ouvre l'URL publique telle quelle.
                let url = if published {
                    format!("{}/issue/{}", config.front_url, slug)
                } else {
                    match crate::data::issues::get_preview_token(id, token) {
                        Ok(preview) => format!(
                            "{}/issue/{}?preview={}",
                            config.front_url, slug, preview.token
                        ),
                        Err(e) => {
                            push_toast(state, "Error".to_string(), e, Status::Danger);
                            return Task::none();
                        }
                    }
                };

                // `open` délègue à xdg-open : le navigateur déjà ouvert
                // récupère l'onglet, sinon il en démarre un.
                if let Err(e) = open::that(&url) {
                    push_toast(
                        state,
                        "Error".to_string(),
                        format!("Could not open the browser: {}", e),
                        Status::Danger,
                    );
                }

                Task::none()
            }
            issue::Action::NewSection => {
                let issue_id = state.issue.id;
                state.section_form = SectionForm::new(issue_id, all_categories());
                state.modal = ModalKind::NewIssueSection(issue_id);
                Task::none()
            }
            issue::Action::EditSection(section_id) => {
                let section = state.issue.item.as_ref().and_then(|item| {
                    item.sections
                        .iter()
                        .find(|section| section.id == section_id)
                        .cloned()
                });

                if let Some(section) = section {
                    let issue_id = state.issue.id;
                    state.section_form =
                        SectionForm::edit(issue_id, &section, all_categories());
                    state.modal = ModalKind::EditIssueSection(issue_id, section_id);
                }
                Task::none()
            }
            issue::Action::ConfirmDeleteSection(section_id) => {
                state.modal = ModalKind::ConfirmDeleteIssueSection(state.issue.id, section_id);
                Task::none()
            }
            issue::Action::ReorderSections(order) => {
                let issue_id = state.issue.id;
                Task::done(Message::ReorderSections(issue_id, order))
            }
            issue::Action::NewArticle(section_id) => {
                let issue_id = state.issue.id;
                state.article_form = ArticleForm::new(issue_id, section_id);
                state.modal = ModalKind::NewArticle(issue_id, section_id);
                Task::none()
            }
            issue::Action::EditArticle(section_id, article_id) => {
                let article = state.issue.item.as_ref().and_then(|item| {
                    item.sections
                        .iter()
                        .find(|section| section.id == section_id)
                        .and_then(|section| {
                            section
                                .articles
                                .iter()
                                .find(|article| article.id == article_id)
                                .cloned()
                        })
                });

                if let Some(article) = article {
                    let issue_id = state.issue.id;
                    state.article_form = ArticleForm::edit(issue_id, section_id, &article);
                    state.modal = ModalKind::EditArticle(issue_id, section_id, article_id);
                }
                Task::none()
            }
            issue::Action::ConfirmDeleteArticle(section_id, article_id) => {
                state.modal =
                    ModalKind::ConfirmDeleteArticle(state.issue.id, section_id, article_id);
                Task::none()
            }
            issue::Action::ReorderArticles(section_id, order) => {
                let issue_id = state.issue.id;
                Task::done(Message::ReorderArticles(issue_id, section_id, order))
            }
            issue::Action::EditTags(id) => {
                if let Some(item) = &state.issue.item {
                    let all_tags = match crate::data::tags::get_tags() {
                        Ok(res) => res.data,
                        Err(e) => {
                            eprintln!("Error: {}", e);
                            vec![]
                        }
                    };
                    state.issue_tags_form = components::forms::issue_tags::IssueTagsForm::new(
                        id,
                        item.tags.clone(),
                        all_tags,
                    );
                    state.modal = ModalKind::EditIssueTags(id);
                }
                Task::none()
            }
            issue::Action::None => Task::none(),
        },
        Message::NewIssue(message) => match state.new_issue.update(message) {
            new_issue::Action::BackToList => {
                if let Some(session) = &state.current_user {
                    state.issues = Issues::new(session);
                }
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
        Message::Profile(message) => match state.profile.update(message) {
            profile::Action::Toast(title, content, status) => {
                push_toast(state, title, content, status);
                Task::none()
            }
            profile::Action::Updated(user) => {
                if let Some(session) = &mut state.current_user {
                    session.user = user;
                }

                push_toast(
                    state,
                    "Success".to_string(),
                    "Profile updated".to_string(),
                    Status::Success,
                );
                Task::none()
            }
            profile::Action::None => Task::none(),
        },
        Message::Nav(message) => {
            if let Some(session) = &state.current_user {
                match state.nav.update(message) {
                    nav::Action::GoToScreen(screen) => {
                        match screen {
                            Screen::Issues => state.issues = Issues::new(session),
                            Screen::Sponsors => state.sponsors = Sponsors::new(session.clone()),
                            Screen::Tags => state.tags = Tags::new(),
                            Screen::Listing => state.listing = Listing::new(),
                            Screen::Categories => state.categories = Categories::new(),
                            Screen::Feeds => state.feeds = Feeds::new(),
                            Screen::Profile => state.profile = Profile::new(session.clone()),
                            _ => (),
                        }
                        state.current_screen = screen;
                    }
                    nav::Action::LogOut => {
                        clear_session();
                        state.current_user = None;
                        state.current_screen = Screen::Login;
                    }
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
        Message::ConfirmDeleteCategory(id) => {
            if let Some(session) = &state.current_user {
                match crate::data::categories::delete_category(id, session.token.clone()) {
                    Ok(_) => {
                        push_toast(
                            state,
                            "Success".to_string(),
                            "The category has been successfully deleted.".to_string(),
                            Status::Success,
                        );

                        state.categories.reload_data();
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmEditCategory(id) => {
            if let Some(session) = &state.current_user {
                match crate::data::categories::update_category(
                    &id,
                    state.category_form.get_category(),
                    session.token.clone(),
                ) {
                    Ok(_) => {
                        push_toast(
                            state,
                            "Success".to_string(),
                            "The category has been successfully updated".to_string(),
                            Status::Success,
                        );

                        state.categories.reload_data();
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmNewCategory => {
            if let Some(session) = &state.current_user {
                match crate::data::categories::create_category(
                    state.category_form.get_category(),
                    session.token.clone(),
                ) {
                    Ok(_) => {
                        push_toast(
                            state,
                            "Success".to_string(),
                            "The category has been successfully created".to_string(),
                            Status::Success,
                        );

                        state.categories.reload_data();
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmDeleteFeed(id) => {
            if let Some(session) = &state.current_user {
                match crate::data::feeds::delete_feed(id, session.token.clone()) {
                    Ok(_) => {
                        push_toast(
                            state,
                            "Success".to_string(),
                            "The feed has been successfully deleted.".to_string(),
                            Status::Success,
                        );

                        state.feeds.reload_data();
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmEditFeed(id) => {
            if let Some(session) = &state.current_user {
                match crate::data::feeds::update_feed(
                    &id,
                    state.feed_form.get_feed(),
                    session.token.clone(),
                ) {
                    Ok(_) => {
                        push_toast(
                            state,
                            "Success".to_string(),
                            "The feed has been successfully updated".to_string(),
                            Status::Success,
                        );

                        state.feeds.reload_data();
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            state.modal = ModalKind::None;
            Task::none()
        }
        Message::ConfirmNewFeed => {
            if let Some(session) = &state.current_user {
                match crate::data::feeds::create_feed(
                    state.feed_form.get_feed(),
                    session.token.clone(),
                ) {
                    Ok(_) => {
                        push_toast(
                            state,
                            "Success".to_string(),
                            "The feed has been successfully created".to_string(),
                            Status::Success,
                        );

                        state.feeds.reload_data();
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
        Message::IssueTagsForm(message) => {
            match state.issue_tags_form.update(message) {
                components::forms::issue_tags::Action::None => {}
                components::forms::issue_tags::Action::Add(name) => {
                    return iced::Task::done(Message::AddIssueTag(name));
                }
                components::forms::issue_tags::Action::Delete(name) => {
                    return iced::Task::done(Message::RemoveIssueTag(name));
                }
            }
            Task::none()
        }
        Message::AddIssueTag(name) => {
            if let Some(session) = &state.current_user {
                match crate::data::issue_tags::add_issue_tag(
                    state.issue_tags_form.issue_id,
                    name,
                    session.token.clone(),
                ) {
                    Ok(_) => {
                        if let Ok(crate::data::responses::Response::Success(new_issue)) =
                            crate::data::issues::get_issue(
                                state.issue_tags_form.issue_id,
                                &session.token,
                            )
                        {
                            state.issue.item = Some(new_issue.clone());
                            state.issue_tags_form.set_tags(new_issue.tags);
                        }
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            Task::none()
        }
        Message::FileHovered => {
            state.file_hovering = markdown_editor_on_screen(state);
            Task::none()
        }
        Message::FileHoverLeft => {
            state.file_hovering = false;
            Task::none()
        }
        Message::FileDropped(path) => {
            state.file_hovering = false;

            if !markdown_editor_on_screen(state) {
                return Task::none();
            }

            if !crate::utils::images::has_image_extension(&path) {
                push_toast(
                    state,
                    "Error".to_string(),
                    "Only image files can be dropped here.".to_string(),
                    Status::Danger,
                );
                return Task::none();
            }

            match state.modal {
                ModalKind::NewArticle(_, _) | ModalKind::EditArticle(_, _, _) => {
                    state.article_form.insert_image(&path)
                }
                _ => state.section_form.insert_image(&path),
            }

            Task::none()
        }
        Message::SectionForm(message) => state.section_form.update(message).map(Message::SectionForm),
        Message::ArticleForm(message) => state.article_form.update(message).map(Message::ArticleForm),
        Message::SaveSection => {
            let token = match &state.current_user {
                Some(session) => session.token.clone(),
                None => return Task::none(),
            };
            let issue_id = state.section_form.issue_id;

            // Les images choisies pendant l'édition partent maintenant, pas
            // avant : abandonner la modale ne laisse rien sur le serveur.
            let before = state.section_form.original_markdown().to_string();
            if state.section_form.is_text() {
                let markdown = state.section_form.markdown();
                if !sync_images_before_save(state, &token, markdown, |state, text| {
                    state.section_form.set_markdown(text)
                }) {
                    return Task::none();
                }
            }
            let after = state.section_form.markdown();

            match state.section_form.section_id {
                Some(section_id) => match state.section_form.update_payload() {
                    Some(payload) => {
                        let result = crate::data::issue_sections::update_section(
                            issue_id,
                            section_id,
                            payload,
                            token.clone(),
                        );
                        let saved = result.is_ok();
                        handle_content_result(
                            state,
                            issue_id,
                            result,
                            "The section has been saved.",
                        );
                        // Après l'enregistrement seulement : le garde-fou de
                        // l'API lit le contenu en base pour savoir si une
                        // image sert encore ailleurs.
                        if saved {
                            crate::utils::markdown_media::delete_removed_images(
                                &before, &after, &token,
                            );
                        }
                    }
                    None => push_toast(
                        state,
                        "Error".to_string(),
                        "Select a category first.".to_string(),
                        Status::Danger,
                    ),
                },
                None => match state.section_form.new_payload() {
                    Some(payload) => {
                        let result =
                            crate::data::issue_sections::create_section(issue_id, payload, token);
                        handle_content_result(
                            state,
                            issue_id,
                            result,
                            "The section has been created.",
                        );
                    }
                    None => push_toast(
                        state,
                        "Error".to_string(),
                        "Select a category first.".to_string(),
                        Status::Danger,
                    ),
                },
            }
            Task::none()
        }
        Message::DeleteSection(issue_id, section_id) => {
            let token = match &state.current_user {
                Some(session) => session.token.clone(),
                None => return Task::none(),
            };
            let result =
                crate::data::issue_sections::delete_section(issue_id, section_id, token);
            handle_content_result(state, issue_id, result, "The section has been deleted.");
            Task::none()
        }
        Message::ReorderSections(issue_id, order) => {
            let token = match &state.current_user {
                Some(session) => session.token.clone(),
                None => return Task::none(),
            };
            let result = crate::data::issue_sections::reorder_sections(issue_id, order, token);
            handle_content_result(state, issue_id, result, "The sections have been reordered.");
            Task::none()
        }
        Message::SaveArticle => {
            let token = match &state.current_user {
                Some(session) => session.token.clone(),
                None => return Task::none(),
            };
            let issue_id = state.article_form.issue_id;
            let section_id = state.article_form.section_id;

            let before = state.article_form.original_markdown().to_string();
            let markdown = state.article_form.markdown();
            if !sync_images_before_save(state, &token, markdown, |state, text| {
                state.article_form.set_markdown(text)
            }) {
                return Task::none();
            }
            let after = state.article_form.markdown();

            let payload = state.article_form.payload();

            let result = match state.article_form.article_id {
                Some(article_id) => crate::data::articles::update_article(
                    issue_id,
                    section_id,
                    article_id,
                    payload,
                    token.clone(),
                )
                .map(|_| ()),
                None => crate::data::articles::create_article(
                    issue_id,
                    section_id,
                    payload,
                    token.clone(),
                )
                .map(|_| ()),
            };

            let saved = result.is_ok();
            handle_content_result(state, issue_id, result, "The article has been saved.");
            if saved {
                crate::utils::markdown_media::delete_removed_images(&before, &after, &token);
            }
            Task::none()
        }
        Message::DeleteArticle(issue_id, section_id, article_id) => {
            let token = match &state.current_user {
                Some(session) => session.token.clone(),
                None => return Task::none(),
            };
            let result =
                crate::data::articles::delete_article(issue_id, section_id, article_id, token);
            handle_content_result(state, issue_id, result, "The article has been deleted.");
            Task::none()
        }
        Message::ReorderArticles(issue_id, section_id, order) => {
            let token = match &state.current_user {
                Some(session) => session.token.clone(),
                None => return Task::none(),
            };
            let result =
                crate::data::articles::reorder_articles(issue_id, section_id, order, token);
            handle_content_result(state, issue_id, result, "The articles have been reordered.");
            Task::none()
        }
        Message::RemoveIssueTag(name) => {
            if let Some(session) = &state.current_user {
                match crate::data::issue_tags::delete_issue_tag(
                    state.issue_tags_form.issue_id,
                    name,
                    session.token.clone(),
                ) {
                    Ok(_) => {
                        if let Ok(crate::data::responses::Response::Success(new_issue)) =
                            crate::data::issues::get_issue(
                                state.issue_tags_form.issue_id,
                                &session.token,
                            )
                        {
                            state.issue.item = Some(new_issue.clone());
                            state.issue_tags_form.set_tags(new_issue.tags);
                        }
                    }
                    Err(e) => push_toast(state, "Error".to_string(), e, Status::Danger),
                }
            }
            Task::none()
        }
    }
}

fn view(state: &State) -> Element<'_, Message> {
    match &state.current_user {
        Some(session) => {
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
                Screen::Categories => state.categories.view().map(Message::Categories),
                Screen::Feeds => state.feeds.view().map(Message::Feeds),
                Screen::Profile => state.profile.view().map(Message::Profile),
            };
            // Hauteur bornée à la fenêtre : sans elle, le `scrollable` d'un
            // écran n'aurait aucune limite à respecter et ne défilerait pas.
            let main_container = container(main_content)
                .width(Length::FillPortion(5))
                .height(Length::Fill);

            // Return composed layout
            let content = row![
                state
                    .nav
                    .view(&state.current_screen, session)
                    .map(Message::Nav),
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
                        None,
                        Message::CloseModal,
                        Some(Message::CloseModal),
                        Some(Message::ConfirmDeleteTag(id.clone())),
                    )
                }
                ModalKind::EditTag(id) => components::modal::modal(
                    content,
                    Some(format!("Tag {}", id)),
                    state.tag_form.view().map(Message::TagForm),
                    None,
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::ConfirmEditTag(id.clone())),
                ),
                ModalKind::NewTag => components::modal::modal(
                    content,
                    Some("New tag".to_string()),
                    state.tag_form.view().map(Message::TagForm),
                    None,
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::ConfirmNewTag),
                ),
                ModalKind::ConfirmDeleteCategory(id) => {
                    let modal_content: Element<'_, Message> =
                        column![text(format!("Delete category « {} » ?", id.clone())),].into();

                    components::modal::modal(
                        content,
                        Some("Confirmation".to_string()),
                        modal_content,
                        None,
                        Message::CloseModal,
                        Some(Message::CloseModal),
                        Some(Message::ConfirmDeleteCategory(id.clone())),
                    )
                }
                ModalKind::EditCategory(id) => components::modal::modal(
                    content,
                    Some(format!("Category {}", id)),
                    state.category_form.view().map(Message::CategoryForm),
                    None,
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::ConfirmEditCategory(id.clone())),
                ),
                ModalKind::NewCategory => components::modal::modal(
                    content,
                    Some("New category".to_string()),
                    state.category_form.view().map(Message::CategoryForm),
                    None,
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::ConfirmNewCategory),
                ),
                ModalKind::ConfirmDeleteFeed(id) => {
                    let modal_content: Element<'_, Message> =
                        column![text(format!("Delete feed « {} » ?", id.clone())),].into();

                    components::modal::modal(
                        content,
                        Some("Confirmation".to_string()),
                        modal_content,
                        None,
                        Message::CloseModal,
                        Some(Message::CloseModal),
                        Some(Message::ConfirmDeleteFeed(id.clone())),
                    )
                }
                ModalKind::EditFeed(id) => components::modal::modal(
                    content,
                    Some(format!("Feed {}", id)),
                    state.feed_form.view().map(Message::FeedForm),
                    None,
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::ConfirmEditFeed(id.clone())),
                ),
                ModalKind::NewFeed => components::modal::modal(
                    content,
                    Some("New feed".to_string()),
                    state.feed_form.view().map(Message::FeedForm),
                    None,
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::ConfirmNewFeed),
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
                        None,
                        Message::CloseModal,
                        Some(Message::CloseModal),
                        Some(Message::ConfirmPublishIssue(*id)),
                    )
                }
                ModalKind::ConfirmArchiveIssue(id) => components::modal::modal(
                    content,
                    Some("Confirmation".to_string()),
                    text(format!("Archive issue #{}?", id)).into(),
                    None,
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
                        None,
                        Message::CloseModal,
                        Some(Message::CloseModal),
                        Some(Message::ConfirmDeleteUser(id.clone())),
                    )
                }
                ModalKind::NewIssueSection(_) => components::modal::modal(
                    content,
                    Some("New section".to_string()),
                    state.section_form.view().map(Message::SectionForm),
                    Some(Length::Fixed(800.0)),
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::SaveSection),
                ),
                ModalKind::EditIssueSection(_, section_id) => components::modal::modal(
                    content,
                    Some(format!("Section #{}", section_id)),
                    state.section_form.view().map(Message::SectionForm),
                    Some(Length::Fixed(800.0)),
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::SaveSection),
                ),
                ModalKind::ConfirmDeleteIssueSection(issue_id, section_id) => {
                    let modal_content: Element<'_, Message> = column![text(format!(
                        "Delete section #{} and everything it contains?",
                        section_id
                    ))]
                    .into();

                    components::modal::modal(
                        content,
                        Some("Confirmation".to_string()),
                        modal_content,
                        None,
                        Message::CloseModal,
                        Some(Message::CloseModal),
                        Some(Message::DeleteSection(*issue_id, *section_id)),
                    )
                }
                ModalKind::NewArticle(_, _) => components::modal::modal(
                    content,
                    Some("New article".to_string()),
                    state.article_form.view().map(Message::ArticleForm),
                    Some(Length::Fixed(800.0)),
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::SaveArticle),
                ),
                ModalKind::EditArticle(_, _, article_id) => components::modal::modal(
                    content,
                    Some(format!("Article #{}", article_id)),
                    state.article_form.view().map(Message::ArticleForm),
                    Some(Length::Fixed(800.0)),
                    Message::CloseModal,
                    Some(Message::CloseModal),
                    Some(Message::SaveArticle),
                ),
                ModalKind::ConfirmDeleteArticle(issue_id, section_id, article_id) => {
                    let modal_content: Element<'_, Message> =
                        column![text(format!("Delete article #{}?", article_id))].into();

                    components::modal::modal(
                        content,
                        Some("Confirmation".to_string()),
                        modal_content,
                        None,
                        Message::CloseModal,
                        Some(Message::CloseModal),
                        Some(Message::DeleteArticle(*issue_id, *section_id, *article_id)),
                    )
                }
                ModalKind::EditIssueTags(_) => components::modal::modal(
                    content,
                    Some("Tags".to_string()),
                    state.issue_tags_form.view().map(Message::IssueTagsForm),
                    None,
                    Message::CloseModal,
                    None,
                    None,
                ),
            };

            // Voile sur toute la fenêtre pendant qu'un fichier la survole —
            // iced ne dit pas quelle zone est visée, donc l'indication est
            // globale et n'apparaît que si un éditeur peut recevoir l'image.
            let content: Element<'_, Message> = if state.file_hovering {
                stack![
                    content,
                    opaque(
                        center(components::typography::typography(
                            String::from("Drop the image here"),
                            components::typography::TypographyStyle::Title,
                        ))
                        .style(|_theme| {
                            container::Style::default()
                                .background(Color::from_rgba(0.0, 0.0, 0.0, 0.6))
                        })
                    )
                ]
                .into()
            } else {
                content.into()
            };

            toast::Manager::new(content, &state.toasts, Message::DismissToast)
                .timeout(toast::DEFAULT_TIMEOUT)
                .into()
        }
        None => state.login.view().map(Message::Login),
    }
}

/// iced ne signale le survol que globalement : c'est `update` qui décide si le
/// fichier a une destination.
fn subscription(_state: &State) -> iced::Subscription<Message> {
    iced::event::listen_with(|event, _status, _window| match event {
        iced::Event::Window(iced::window::Event::FileHovered(_)) => Some(Message::FileHovered),
        iced::Event::Window(iced::window::Event::FilesHoveredLeft) => Some(Message::FileHoverLeft),
        iced::Event::Window(iced::window::Event::FileDropped(path)) => {
            Some(Message::FileDropped(path))
        }
        _ => None,
    })
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
        .subscription(subscription)
        .window(iced::window::Settings {
            size: iced::Size::new(1440.0, 900.0),
            ..iced::window::Settings::default()
        })
        .font(include_bytes!("../assets/fonts/GeneralSans-Variable.ttf").as_slice())
        .theme(theme)
        .settings(settings)
        .run()
}
