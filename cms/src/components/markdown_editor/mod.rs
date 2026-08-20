use std::collections::HashMap;
use std::path::Path;
use std::sync::Arc;
use std::time::Duration;

use frostmark::{MarkState, MarkWidget, UpdateMsg};
use iced::{
    Border, Color, Element, Length, Padding, Task, Theme,
    border::Radius,
    keyboard::{self, key::Named},
    widget::image::Handle,
    widget::{column, container, image as iced_image, row, scrollable, text, text_editor},
};

use crate::utils::images::plain_png_bytes;

pub mod embeds;
pub mod link_paste;
pub mod toolbar;

/// Préfixe des images pas encore envoyées. Le markdown sert lui-même de
/// registre : aucun état parallèle à resynchroniser quand on annule, refait
/// ou retape une ligne.
pub const LOCAL_SCHEME: &str = "file://";

/// Lien markdown vers un fichier local, tel qu'inséré par le bouton ou par un
/// glisser-déposer.
pub fn image_markdown(path: &Path) -> String {
    let alt = path
        .file_stem()
        .and_then(|stem| stem.to_str())
        .unwrap_or("image");

    format!("![{}]({}{})", alt, LOCAL_SCHEME, path.display())
}

/// Chemin disque d'un lien local, `None` si l'url est distante.
pub fn local_path(url: &str) -> Option<&Path> {
    url.strip_prefix(LOCAL_SCHEME).map(Path::new)
}

/// Télécharge une image de la prévisualisation. Le délai court évite de figer
/// la frappe sur une url incomplète en cours de saisie.
fn fetch_remote_image(url: &str) -> Option<Handle> {
    if !url.starts_with("http") {
        return None;
    }

    let client = reqwest::blocking::Client::builder()
        .timeout(Duration::from_secs(3))
        .build()
        .ok()?;
    let bytes = client.get(url).send().ok()?.bytes().ok()?;
    plain_png_bytes(&bytes).map(Handle::from_bytes)
}

#[derive(Clone, Debug)]
pub enum EditorMode {
    Edit,
    Preview,
    Split,
}

#[derive(Clone, Debug)]
pub enum ToolbarAction {
    Bold,
    Italic,
    Heading(u8),
    BulletList,
    NumberedList,
    Quote,
    Code,
    CodeBlock,
    Link,
    Image,
    Collapse,
    Fullscreen,
}

#[derive(Clone, Debug)]
pub enum Message {
    ContentChanged(text_editor::Action),
    ModeChanged(EditorMode),
    ToolbarAction(ToolbarAction),
    LinkClicked(String),
    UpdatePreview(UpdateMsg),
    /// Titre récupéré pour une URL collée, `None` si la page est injoignable.
    TitleFetched { url: String, title: Option<String> },
}

/// URL collée telle quelle, en attente de son titre.
struct PendingLink {
    url: String,
    line: usize,
    start: usize,
    end: usize,
}

pub struct MarkdownEditor {
    pub content: text_editor::Content,
    pub mode: EditorMode,
    pub preview: MarkState,
    pub fullscreen: bool,
    pending_link: Option<PendingLink>,
    /// Images de la prévisualisation. `None` mémorise un échec, pour ne pas
    /// retenter le chargement à chaque frappe.
    images: HashMap<String, Option<Handle>>,
}

impl Default for MarkdownEditor {
    fn default() -> Self {
        Self {
            content: text_editor::Content::default(),
            mode: EditorMode::Split,
            preview: MarkState::with_html_and_markdown(""),
            fullscreen: false,
            pending_link: None,
            images: HashMap::new(),
        }
    }
}

impl MarkdownEditor {
    pub fn new(initial_text: &str) -> Self {
        let mut editor = Self {
            content: text_editor::Content::with_text(initial_text),
            mode: EditorMode::Split,
            preview: MarkState::with_html_and_markdown(initial_text),
            fullscreen: false,
            pending_link: None,
            images: HashMap::new(),
        };
        editor.refresh_images();
        editor
    }

    pub fn text(&self) -> String {
        self.content.text()
    }

    /// Remplace tout le contenu — utilisé après l'envoi des images, quand les
    /// liens locaux deviennent des urls définitives.
    pub fn set_text(&mut self, text: &str) {
        self.content = text_editor::Content::with_text(text);
        self.refresh_preview();
    }

    /// Insère un lien vers un fichier local au curseur.
    pub fn insert_image(&mut self, path: &Path) {
        self.content
            .perform(text_editor::Action::Edit(text_editor::Edit::Paste(
                Arc::new(image_markdown(path)),
            )));
        self.refresh_preview();
    }

    fn refresh_preview(&mut self) {
        self.preview = MarkState::with_html_and_markdown(&self.content.text());
        self.refresh_images();
    }

    /// Charge les images citées par le document qui ne sont pas en cache.
    /// Toujours appelé depuis `update`, jamais depuis `view`.
    fn refresh_images(&mut self) {
        for url in self.preview.find_image_links() {
            if self.images.contains_key(&url) {
                continue;
            }

            let handle = match local_path(&url) {
                Some(path) => std::fs::read(path)
                    .ok()
                    .and_then(|bytes| plain_png_bytes(&bytes))
                    .map(Handle::from_bytes),
                None => fetch_remote_image(&url),
            };

            self.images.insert(url, handle);
        }
    }

    pub fn update(&mut self, message: Message) -> Task<Message> {
        match message {
            Message::ContentChanged(action) => {
                let is_edit = action.is_edit();

                let task = match self.paste_link(&action) {
                    Some(task) => task,
                    None => {
                        self.content.perform(action);
                        Task::none()
                    }
                };

                if is_edit {
                    self.refresh_preview();
                }

                return task;
            }
            Message::ModeChanged(mode) => {
                self.mode = mode;
            }
            Message::ToolbarAction(action) => {
                match action {
                    ToolbarAction::Fullscreen => {
                        self.fullscreen = !self.fullscreen;
                    }
                    _ => toolbar::apply(&mut self.content, action),
                }

                self.refresh_preview();
            }
            Message::UpdatePreview(msg) => {
                self.preview.update(msg);
            }
            Message::LinkClicked(url) => {
                println!("Opening link: {url}");
                _ = open::that(&url);
            }
            Message::TitleFetched { url, title } => {
                if self.replace_pending_link(&url, title) {
                    self.preview = MarkState::with_html_and_markdown(&self.content.text());
                }
            }
        }

        Task::none()
    }

    /// Intercepte le collage d'une URL nue.
    ///
    /// Avec une sélection, elle devient le libellé du lien tout de suite ; sinon
    /// l'URL est collée telle quelle et son titre viendra la remplacer.
    /// Renvoie `None` quand le collage doit suivre le chemin normal.
    fn paste_link(&mut self, action: &text_editor::Action) -> Option<Task<Message>> {
        let text_editor::Action::Edit(text_editor::Edit::Paste(pasted)) = action else {
            return None;
        };

        let url = link_paste::as_link(pasted)?;

        if let Some(selection) = self.content.selection() {
            let label = link_paste::escape_label(&selection);
            self.paste(format!("[{label}]({url})"));

            return Some(Task::none());
        }

        // Déjà entre les parenthèses d'un lien : on ne double pas la syntaxe.
        let cursor = self.content.cursor().position;
        let line = self.content.line(cursor.line)?;

        if line.text.get(..cursor.column)?.ends_with(['(', '<']) {
            return None;
        }

        // Une vidéo ou un post se rend en embed : la directive se suffit à
        // elle-même, inutile d'aller chercher un titre de page.
        if let Some(directive) = embeds::directive_for(&url) {
            self.paste(directive);
            return Some(Task::none());
        }

        self.paste(url.clone());

        let cursor = self.content.cursor().position;
        self.pending_link = cursor
            .column
            .checked_sub(url.len())
            .map(|start| PendingLink {
                url: url.clone(),
                line: cursor.line,
                start,
                end: cursor.column,
            });

        Some(Task::perform(link_paste::fetch_title(url.clone()), move |title| {
            Message::TitleFetched {
                url: url.clone(),
                title,
            }
        }))
    }

    /// Remplace l'URL collée par `[titre](url)`, si elle est toujours en place.
    fn replace_pending_link(&mut self, url: &str, title: Option<String>) -> bool {
        // Une autre URL a pu être collée entre-temps : sa réponse est la seule
        // qui doit encore la remplacer.
        let Some(pending) = self.pending_link.take_if(|pending| pending.url == url) else {
            return false;
        };

        let Some(title) = title else {
            return false;
        };

        // L'utilisateur a pu éditer le texte pendant la requête.
        let Some(line) = self.content.line(pending.line) else {
            return false;
        };

        if line.text.get(pending.start..pending.end) != Some(url) {
            return false;
        }

        self.content.move_to(text_editor::Cursor {
            position: text_editor::Position {
                line: pending.line,
                column: pending.end,
            },
            selection: Some(text_editor::Position {
                line: pending.line,
                column: pending.start,
            }),
        });

        let label = link_paste::escape_label(&title);
        self.paste(format!("[{label}]({url})"));

        true
    }

    fn paste(&mut self, text: String) {
        self.content
            .perform(text_editor::Action::Edit(text_editor::Edit::Paste(
                Arc::new(text),
            )));
    }

    pub fn view(&self) -> Element<'_, Message> {
        let toolbar = toolbar::view(&self);

        let editor: Element<'_, Message> = container(
            text_editor(&self.content)
                .key_binding(|key_press: text_editor::KeyPress| {
                    shortcut_binding(&key_press)
                        .map(text_editor::Binding::Custom)
                        .or_else(|| text_editor::Binding::from_key_press(key_press))
                })
                .wrapping(iced::widget::text::Wrapping::WordOrGlyph)
                .on_action(Message::ContentChanged),
        )
        .height(Length::Fill)
        .width(Length::Fill)
        .into();

        let preview: Element<'_, Message> = container(
            scrollable(
                container(
                    MarkWidget::new(&self.preview)
                        .paragraph_spacing(20.0)
                        .on_updating_state(|msg| Message::UpdatePreview(msg))
                        .on_clicking_link(|url| Message::LinkClicked(url))
                        .on_drawing_image(|info| match self.images.get(info.url) {
                            Some(Some(handle)) => iced_image(handle.clone())
                                .width(info.width)
                                .height(info.height)
                                .into(),
                            _ => text("[image]").into(),
                        }),
                )
                .padding(Padding {
                    top: 10.0,
                    left: 10.0,
                    bottom: 10.0,
                    right: 18.0,
                }),
            )
            .height(Length::Fill)
            .direction(scrollable::Direction::Vertical(
                scrollable::Scrollbar::default(),
            )),
        )
        .height(Length::Fill)
        .width(Length::Fill)
        .style(|theme: &Theme| {
            let palette = theme.palette();
            container::Style {
                border: Border {
                    width: 1.0,
                    radius: Radius::default(),
                    color: palette.text,
                },
                ..Default::default()
            }
        })
        .into();

        let content_area = match self.mode {
            EditorMode::Edit => editor,
            EditorMode::Preview => preview,
            EditorMode::Split => row![editor, preview].spacing(12).into(),
        };

        container(
            column![toolbar, content_area]
                .spacing(6)
                .height(Length::Fill)
                .width(Length::Fill),
        )
        .into()
    }
}

fn shortcut_binding(key_press: &text_editor::KeyPress) -> Option<Message> {
    if !key_press.modifiers.command() {
        return None;
    }

    let action = match key_press.key.as_ref() {
        keyboard::Key::Character("b") => ToolbarAction::Bold,
        keyboard::Key::Character("i") => ToolbarAction::Italic,
        keyboard::Key::Character("k") => ToolbarAction::Link,
        keyboard::Key::Character("e") => ToolbarAction::Code,
        keyboard::Key::Named(Named::Enter) if key_press.modifiers.shift() => {
            ToolbarAction::CodeBlock
        }
        keyboard::Key::Character("1") if key_press.modifiers.shift() => ToolbarAction::Heading(1),
        keyboard::Key::Character("2") if key_press.modifiers.shift() => ToolbarAction::Heading(2),
        keyboard::Key::Character("3") if key_press.modifiers.shift() => ToolbarAction::Heading(3),
        keyboard::Key::Character(".") => ToolbarAction::Quote, // Cmd/Ctrl + .
        keyboard::Key::Character("8") if key_press.modifiers.shift() => ToolbarAction::BulletList, // Ctrl+Shift+8 = *
        keyboard::Key::Character("7") if key_press.modifiers.shift() => ToolbarAction::NumberedList,
        _ => return None,
    };

    Some(Message::ToolbarAction(action))
}
