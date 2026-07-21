use frostmark::{MarkState, MarkWidget, UpdateMsg};
use iced::{
    Border, Color, Element, Length, Padding, Theme,
    border::Radius,
    widget::{column, container, row, scrollable, text_editor},
};

pub mod toolbar;

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
}

#[derive(Clone, Debug)]
pub enum Message {
    ContentChanged(text_editor::Action),
    ModeChanged(EditorMode),
    ToolbarAction(ToolbarAction),
    LinkClicked(String),
    UpdatePreview(UpdateMsg),
}

pub struct MarkdownEditor {
    pub content: text_editor::Content,
    pub mode: EditorMode,
    pub preview: MarkState,
}

impl Default for MarkdownEditor {
    fn default() -> Self {
        Self {
            content: text_editor::Content::default(),
            mode: EditorMode::Split,
            preview: MarkState::with_html_and_markdown(""),
        }
    }
}

impl MarkdownEditor {
    pub fn new(initial_text: &str) -> Self {
        Self {
            content: text_editor::Content::with_text(initial_text),
            mode: EditorMode::Split,
            preview: MarkState::with_html_and_markdown(initial_text),
        }
    }

    pub fn text(&self) -> String {
        self.content.text()
    }

    pub fn update(&mut self, message: Message) {
        match message {
            Message::ContentChanged(action) => {
                let is_edit = action.is_edit();
                self.content.perform(action);

                if is_edit {
                    self.preview = MarkState::with_html_and_markdown(&self.content.text());
                }
            }
            Message::ModeChanged(mode) => {
                self.mode = mode;
            }
            Message::ToolbarAction(action) => {
                toolbar::apply(&mut self.content, action);

                self.preview = MarkState::with_html_and_markdown(&self.content.text());
            }
            Message::UpdatePreview(msg) => {
                self.preview.update(msg);
            }
            Message::LinkClicked(url) => {
                println!("Opening link: {url}");
                _ = open::that(&url);
            }
        }
    }

    pub fn view(&self) -> Element<'_, Message> {
        let toolbar = toolbar::view(&self);

        let editor: Element<'_, Message> = container(
            text_editor(&self.content)
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
                        .on_clicking_link(|url| Message::LinkClicked(url)),
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
