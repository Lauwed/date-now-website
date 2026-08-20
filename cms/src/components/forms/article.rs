use iced::{
    Alignment::Center,
    Element, Length, Task,
    widget::{column, container, row},
};

use crate::{
    components::{
        form_control::form_control,
        markdown_editor::{self, MarkdownEditor},
    },
    data::articles::{Article, ArticlePayload},
};

pub struct ArticleForm {
    pub issue_id: u32,
    pub section_id: u32,
    /// `None` en création, `Some(id)` en édition.
    pub article_id: Option<u32>,
    title: String,
    source_name: String,
    source_url: String,
    editor: MarkdownEditor,
    original_markdown: String,
}

impl Default for ArticleForm {
    fn default() -> Self {
        Self {
            issue_id: 0,
            section_id: 0,
            article_id: None,
            title: String::new(),
            source_name: String::new(),
            source_url: String::new(),
            editor: MarkdownEditor::default(),
            original_markdown: String::new(),
        }
    }
}

#[derive(Debug, Clone)]
pub enum Message {
    TitleChanged(String),
    SourceNameChanged(String),
    SourceUrlChanged(String),
    Editor(markdown_editor::Message),
}

impl ArticleForm {
    pub fn new(issue_id: u32, section_id: u32) -> Self {
        Self {
            issue_id,
            section_id,
            article_id: None,
            title: String::new(),
            source_name: String::new(),
            source_url: String::new(),
            editor: MarkdownEditor::new(""),
            original_markdown: String::new(),
        }
    }

    pub fn edit(issue_id: u32, section_id: u32, article: &Article) -> Self {
        Self {
            issue_id,
            section_id,
            article_id: Some(article.id),
            title: article.title.clone(),
            source_name: article.source_name.clone(),
            source_url: article.source_url.clone(),
            editor: MarkdownEditor::new(&article.summary),
            original_markdown: article.summary.clone(),
        }
    }

    /// L'API exige les quatre champs, en création comme en édition.
    pub fn payload(&self) -> ArticlePayload {
        ArticlePayload {
            title: self.title.clone(),
            source_name: self.source_name.clone(),
            source_url: self.source_url.clone(),
            summary: self.editor.text(),
        }
    }

    /// Markdown tel qu'il était à l'ouverture, pour repérer à la sauvegarde
    /// les images qui ont été retirées.
    pub fn original_markdown(&self) -> &str {
        &self.original_markdown
    }

    pub fn markdown(&self) -> String {
        self.editor.text()
    }

    pub fn set_markdown(&mut self, text: &str) {
        self.editor.set_text(text);
    }

    /// Insère une image locale dans l'éditeur de résumé.
    pub fn insert_image(&mut self, path: &std::path::Path) {
        self.editor.insert_image(path);
    }

    pub fn update(&mut self, message: Message) -> Task<Message> {
        match message {
            Message::TitleChanged(value) => self.title = value,
            Message::SourceNameChanged(value) => self.source_name = value,
            Message::SourceUrlChanged(value) => self.source_url = value,
            Message::Editor(message) => return self.editor.update(message).map(Message::Editor),
        }

        Task::none()
    }

    pub fn view(&self) -> Element<'_, Message> {
        let title_input = form_control(
            "Title",
            "title",
            &self.title,
            Some(Message::TitleChanged),
            Length::Fill,
            None,
            None,
        );
        let source_name_input = form_control(
            "Source name",
            "source name",
            &self.source_name,
            Some(Message::SourceNameChanged),
            Length::Fill,
            None,
            None,
        );
        let source_url_input = form_control(
            "Source URL",
            "source url",
            &self.source_url,
            Some(Message::SourceUrlChanged),
            Length::Fill,
            None,
            None,
        );

        let source_row = row![source_name_input, source_url_input]
            .align_y(Center)
            .spacing(10);

        container(
            column![
                title_input,
                source_row,
                container(self.editor.view().map(Message::Editor))
                    .height(Length::Fixed(420.0))
                    .width(Length::Fill),
            ]
            .spacing(10),
        )
        .width(Length::Fixed(900.0))
        .into()
    }
}
