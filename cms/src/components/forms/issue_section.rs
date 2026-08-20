use iced::{
    Alignment::Center,
    Element, Length, Task,
    widget::{column, combo_box, container, pick_list, row},
};

use crate::{
    components::{
        markdown_editor::{self, MarkdownEditor},
        typography::{TypographyStyle, typography},
    },
    data::{
        categories::Category,
        issue_sections::{IssueSection, NewSection, SectionType, UpdateSection},
    },
};

pub struct SectionForm {
    pub issue_id: u32,
    /// `None` en création, `Some(id)` en édition.
    pub section_id: Option<u32>,
    kind: SectionType,
    categories: combo_box::State<Category>,
    selected_category: Option<Category>,
    editor: MarkdownEditor,
    original_markdown: String,
}

impl Default for SectionForm {
    fn default() -> Self {
        Self {
            issue_id: 0,
            section_id: None,
            kind: SectionType::Category,
            categories: combo_box::State::new(vec![]),
            selected_category: None,
            editor: MarkdownEditor::default(),
            original_markdown: String::new(),
        }
    }
}

#[derive(Debug, Clone)]
pub enum Message {
    KindSelected(SectionType),
    CategorySelected(Category),
    Editor(markdown_editor::Message),
}

impl SectionForm {
    pub fn new(issue_id: u32, categories: Vec<Category>) -> Self {
        Self {
            issue_id,
            section_id: None,
            kind: SectionType::Category,
            categories: combo_box::State::new(categories),
            selected_category: None,
            editor: MarkdownEditor::new(""),
            original_markdown: String::new(),
        }
    }

    /// Pré-remplit le formulaire depuis une section existante. Le type d'une
    /// section est immuable côté API : il n'est plus modifiable ici.
    pub fn edit(issue_id: u32, section: &IssueSection, categories: Vec<Category>) -> Self {
        let selected_category = section.category_name.as_ref().and_then(|name| {
            categories
                .iter()
                .find(|category| &category.name == name)
                .cloned()
        });

        Self {
            issue_id,
            section_id: Some(section.id),
            kind: section.kind.clone(),
            categories: combo_box::State::new(categories),
            selected_category,
            editor: MarkdownEditor::new(section.text_body.as_deref().unwrap_or("")),
            original_markdown: section.text_body.clone().unwrap_or_default(),
        }
    }

    /// Corps du POST, ou `None` si aucune catégorie n'est sélectionnée.
    pub fn new_payload(&self) -> Option<NewSection> {
        match self.kind {
            SectionType::Category => self
                .selected_category
                .as_ref()
                .map(|category| NewSection::category(category.name.clone())),
            SectionType::Text => Some(NewSection::text(self.editor.text())),
        }
    }

    /// Corps du PUT, ou `None` si aucune catégorie n'est sélectionnée.
    pub fn update_payload(&self) -> Option<UpdateSection> {
        match self.kind {
            SectionType::Category => self
                .selected_category
                .as_ref()
                .map(|category| UpdateSection::category(category.name.clone())),
            SectionType::Text => Some(UpdateSection::text(self.editor.text())),
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

    /// Seules les sections texte portent un éditeur markdown.
    pub fn is_text(&self) -> bool {
        self.kind == SectionType::Text
    }

    /// Insère une image locale dans l'éditeur, si celui-ci est visible.
    pub fn insert_image(&mut self, path: &std::path::Path) {
        if self.is_text() {
            self.editor.insert_image(path);
        }
    }

    pub fn update(&mut self, message: Message) -> Task<Message> {
        match message {
            Message::KindSelected(kind) => {
                // Interdit en édition : le type est figé à la création.
                if self.section_id.is_none() {
                    self.kind = kind;
                }
            }
            Message::CategorySelected(category) => {
                self.selected_category = Some(category);
            }
            Message::Editor(message) => return self.editor.update(message).map(Message::Editor),
        }

        Task::none()
    }

    pub fn view(&self) -> Element<'_, Message> {
        let mut content = column![].spacing(10);

        if self.section_id.is_none() {
            content = content.push(
                row![
                    typography(String::from("Type"), TypographyStyle::Label),
                    pick_list(
                        vec![SectionType::Category, SectionType::Text],
                        Some(self.kind.clone()),
                        Message::KindSelected,
                    ),
                ]
                .align_y(Center)
                .spacing(10),
            );
        }

        content = match self.kind {
            SectionType::Category => content.push(
                column![
                    typography(String::from("Category"), TypographyStyle::Label),
                    combo_box(
                        &self.categories,
                        "Search a category...",
                        self.selected_category.as_ref(),
                        Message::CategorySelected,
                    )
                    .width(Length::Fill),
                ]
                .spacing(4),
            ),
            SectionType::Text => content.push(
                container(self.editor.view().map(Message::Editor))
                    .height(Length::Fixed(420.0))
                    .width(Length::Fill),
            ),
        };

        container(content)
            .width(Length::Fixed(match self.kind {
                SectionType::Category => 400.0,
                SectionType::Text => 900.0,
            }))
            .into()
    }
}
