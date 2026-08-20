use std::path::PathBuf;

use iced::{
    Alignment::Center,
    Element, Length,
    widget::{button, column, container, image as iced_image, row, scrollable, space::horizontal},
    widget::image::Handle,
};
use lucide_icons::Icon;
use rslug::slugify;

use crate::{
    components::{
        badge::badge,
        card::Card,
        form_control::{form_control, form_control_switch},
        toast::Status,
        typography::{TypographyStyle, typography},
    },
    data::{
        articles::Article,
        issue_sections::{IssueSection, SectionType},
        issues::{Issue as IssueType, IssueStatus, get_issue, update_issue},
        responses::Response,
        sessions::Session,
        traits::Table,
    },
    utils::images::{fetch_handle, load_handle},
};

/// Déplace `id` de `offset` rang dans `ids` et renvoie la liste complète —
/// les endpoints /reorder exigent l'ensemble exact des ids du parent.
/// `None` quand le mouvement sortirait des bornes.
fn moved(mut ids: Vec<u32>, id: u32, offset: i8) -> Option<Vec<u32>> {
    let index = ids.iter().position(|current| *current == id)?;
    let target = index as i32 + offset as i32;

    if target < 0 || target >= ids.len() as i32 {
        return None;
    }

    ids.swap(index, target as usize);
    Some(ids)
}

/// Aperçu d'un corps markdown sur une ligne.
fn preview(body: &str, max: usize) -> String {
    let single_line = body.split_whitespace().collect::<Vec<&str>>().join(" ");

    match single_line.char_indices().nth(max) {
        Some((index, _)) => format!("{}…", &single_line[..index]),
        None => single_line,
    }
}

#[derive(Default)]
pub struct Issue {
    pub id: u32,
    pub item: Option<IssueType>,
    pub session: Session,
    pub auto_slug: bool,
    /// Image choisie mais pas encore envoyée : l'upload n'a lieu qu'à la
    /// sauvegarde, pour ne rien laisser sur le serveur si on abandonne.
    pending_cover: Option<PathBuf>,
    pending_cover_handle: Option<Handle>,
    cover_handle: Option<Handle>,
}

#[derive(Debug, Clone)]
pub enum Message {
    BackToList,
    Submit,
    TitleChanged(String),
    SlugChanged(String),
    IssueNumberChanged(String),
    SubtitleChanged(String),
    ExcerptChanged(String),
    VodUrlChanged(String),
    IsSponsoredChanged(bool),
    ResetSlug,
    RequestPublish,
    RequestArchive,
    RequestPreview,
    PickCover,
    RemoveCover,
    RequestEditTags,
    RequestNewSection,
    RequestEditSection(u32),
    RequestDeleteSection(u32),
    MoveSection(u32, i8),
    RequestNewArticle(u32),
    RequestEditArticle(u32, u32),
    RequestDeleteArticle(u32, u32),
    MoveArticle(u32, u32, i8),
}

pub enum Action {
    None,
    BackToList,
    Toast(String, String, Status),
    ConfirmPublish(u32),
    ConfirmArchive(u32),
    Preview(u32),
    EditTags(u32),
    NewSection,
    EditSection(u32),
    ConfirmDeleteSection(u32),
    /// Liste complète des ids de sections dans le nouvel ordre.
    ReorderSections(Vec<u32>),
    NewArticle(u32),
    EditArticle(u32, u32),
    ConfirmDeleteArticle(u32, u32),
    /// Id de section, puis liste complète des ids d'articles réordonnés.
    ReorderArticles(u32, Vec<u32>),
}

impl Issue {
    pub fn new(id: u32, session: Session) -> Self {
        let item = match get_issue(id, &session.token) {
            Ok(Response::Success(i)) => Some(i),
            Ok(Response::Error(e)) => {
                eprintln!("Error: {}", e);
                None
            }
            Err(e) => {
                eprintln!("Error: {}", e);
                None
            }
        };

        let cover_handle = item
            .as_ref()
            .and_then(|data| data.cover.as_ref())
            .and_then(|cover| cover.url.as_ref())
            .and_then(|url| fetch_handle(url, false));

        Self {
            id,
            item: item.clone(),
            session,
            auto_slug: match &item {
                Some(data) => slugify!(&data.title) == data.slug,
                None => true,
            },
            pending_cover: None,
            pending_cover_handle: None,
            cover_handle,
        }
    }

    /// Sections de l'issue triées par position.
    fn sorted_sections(&self) -> Vec<&IssueSection> {
        match &self.item {
            Some(item) => {
                let mut sections: Vec<&IssueSection> = item.sections.iter().collect();
                sections.sort_by_key(|section| section.position);
                sections
            }
            None => vec![],
        }
    }

    /// Articles d'une section triés par position.
    fn sorted_articles(section: &IssueSection) -> Vec<&Article> {
        let mut articles: Vec<&Article> = section.articles.iter().collect();
        articles.sort_by_key(|article| article.position);
        articles
    }

    fn sorted_section_ids_moved(&self, section_id: u32, offset: i8) -> Option<Vec<u32>> {
        let ids = self
            .sorted_sections()
            .iter()
            .map(|section| section.id)
            .collect();

        moved(ids, section_id, offset)
    }

    fn sorted_article_ids_moved(
        &self,
        section_id: u32,
        article_id: u32,
        offset: i8,
    ) -> Option<Vec<u32>> {
        let sections = self.sorted_sections();
        let section = sections
            .iter()
            .find(|section| section.id == section_id)?;
        let ids = Self::sorted_articles(section)
            .iter()
            .map(|article| article.id)
            .collect();

        moved(ids, article_id, offset)
    }

    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::BackToList => Action::BackToList,
            Message::Submit => {
                // L'image en attente part maintenant : c'est la sauvegarde qui
                // décide, pas la sélection du fichier.
                if let Some(path) = self.pending_cover.clone() {
                    let alt = format!("Cover of issue {}", self.id);
                    match crate::data::medias::upload_media_blocking(
                        &alt,
                        &path,
                        &self.session.token,
                    ) {
                        Ok(media) => {
                            if let Some(item) = &self.item {
                                self.item = Some(IssueType {
                                    cover_id: Some(media.id),
                                    ..item.clone()
                                });
                            }
                            self.cover_handle = self.pending_cover_handle.take();
                            self.pending_cover = None;
                        }
                        Err(e) => {
                            return Action::Toast("Error".to_string(), e, Status::Danger);
                        }
                    }
                }

                if let Some(issue) = &mut self.item {
                    // Save in API
                    match update_issue(self.id, issue.clone(), self.session.token.clone()) {
                        Ok(Response::Success(new_issue)) => {
                            // L'ancienne cover a été supprimée côté API ; on
                            // repart sans coverId pour ne pas la réémettre.
                            self.item = Some(IssueType {
                                cover_id: None,
                                ..new_issue
                            });

                            Action::Toast(
                                "Success".to_string(),
                                "The issue has been successfully saved.".to_string(),
                                Status::Success,
                            )
                        }
                        Ok(Response::Error(parse_err)) => Action::Toast(
                            "Error".to_string(),
                            format!("Parsing error: {}", parse_err),
                            Status::Danger,
                        ),
                        Err(err) => Action::Toast(
                            "Error".to_string(),
                            format!("Fetch error: {}", err),
                            Status::Danger,
                        ),
                    }
                } else {
                    Action::Toast(
                        "Failed".to_string(),
                        "No issues".to_string(),
                        Status::Danger,
                    )
                }
            }
            Message::TitleChanged(value) => {
                if let Some(item) = &self.item {
                    let mut slug = item.slug.clone();

                    if self.auto_slug {
                        slug = slugify!(&value);
                    }

                    self.item = Some(IssueType {
                        title: value,
                        slug,
                        ..item.clone()
                    });
                }
                Action::None
            }
            Message::SlugChanged(value) => {
                if let Some(item) = &self.item {
                    self.item = Some(IssueType {
                        slug: value,
                        ..item.clone()
                    });
                }
                self.auto_slug = false;

                Action::None
            }
            Message::ResetSlug => {
                if let Some(item) = &self.item {
                    let slug = slugify!(&item.title);
                    self.item = Some(IssueType {
                        slug,
                        ..item.clone()
                    });
                }
                self.auto_slug = true;
                Action::None
            }
            Message::SubtitleChanged(value) => {
                if let Some(item) = &self.item {
                    self.item = Some(IssueType {
                        subtitle: value,
                        ..item.clone()
                    });
                }
                Action::None
            }
            Message::IssueNumberChanged(value) => {
                let nb_result = value.parse::<u32>();

                if let (Some(item), Ok(nb)) = (&self.item, nb_result) {
                    self.item = Some(IssueType {
                        issue_number: nb,
                        ..item.clone()
                    });
                }
                Action::None
            }
            Message::ExcerptChanged(value) => {
                if let Some(item) = &self.item {
                    self.item = Some(IssueType {
                        excerpt: value,
                        ..item.clone()
                    });
                }
                Action::None
            }
            Message::VodUrlChanged(value) => {
                if let Some(item) = &self.item {
                    self.item = Some(IssueType {
                        vod_url: value,
                        ..item.clone()
                    });
                }
                Action::None
            }
            Message::IsSponsoredChanged(value) => {
                if let Some(item) = &self.item {
                    self.item = Some(IssueType {
                        is_sponsored: value,
                        ..item.clone()
                    });
                }
                Action::None
            }
            Message::RequestPublish => Action::ConfirmPublish(self.id),
            Message::RequestArchive => Action::ConfirmArchive(self.id),
            Message::RequestPreview => Action::Preview(self.id),
            Message::PickCover => {
                if let Some(path) = rfd::FileDialog::new()
                    .add_filter("Image", &crate::utils::images::IMAGE_EXTENSIONS)
                    .pick_file()
                {
                    self.pending_cover_handle = load_handle(&path, false);
                    self.pending_cover = Some(path);
                }
                Action::None
            }
            Message::RemoveCover => {
                self.pending_cover = None;
                self.pending_cover_handle = None;
                Action::None
            }
            Message::RequestEditTags => Action::EditTags(self.id),
            Message::RequestNewSection => Action::NewSection,
            Message::RequestEditSection(section_id) => Action::EditSection(section_id),
            Message::RequestDeleteSection(section_id) => {
                Action::ConfirmDeleteSection(section_id)
            }
            Message::MoveSection(section_id, offset) => {
                match self.sorted_section_ids_moved(section_id, offset) {
                    Some(order) => Action::ReorderSections(order),
                    None => Action::None,
                }
            }
            Message::RequestNewArticle(section_id) => Action::NewArticle(section_id),
            Message::RequestEditArticle(section_id, article_id) => {
                Action::EditArticle(section_id, article_id)
            }
            Message::RequestDeleteArticle(section_id, article_id) => {
                Action::ConfirmDeleteArticle(section_id, article_id)
            }
            Message::MoveArticle(section_id, article_id, offset) => {
                match self.sorted_article_ids_moved(section_id, article_id, offset) {
                    Some(order) => Action::ReorderArticles(section_id, order),
                    None => Action::None,
                }
            }
        }
    }

    fn article_view<'a>(
        section_id: u32,
        article: &'a Article,
        is_first: bool,
        is_last: bool,
    ) -> Element<'a, Message> {
        let move_up = button(iced::widget::Text::from(Icon::ArrowUp)).on_press_maybe(
            (!is_first).then_some(Message::MoveArticle(section_id, article.id, -1)),
        );
        let move_down = button(iced::widget::Text::from(Icon::ArrowDown)).on_press_maybe(
            (!is_last).then_some(Message::MoveArticle(section_id, article.id, 1)),
        );

        let heading = column![
            typography(article.title.clone(), TypographyStyle::Body),
            typography(
                format!("{} — {}", article.source_name, article.source_url),
                TypographyStyle::Small,
            ),
        ]
        .spacing(2);

        container(
            row![
                heading,
                horizontal(),
                move_up,
                move_down,
                button(iced::widget::Text::from(Icon::Pencil))
                    .on_press(Message::RequestEditArticle(section_id, article.id)),
                button(iced::widget::Text::from(Icon::Trash2))
                    .on_press(Message::RequestDeleteArticle(section_id, article.id))
                    .style(button::danger),
            ]
            .align_y(Center)
            .spacing(6),
        )
        .padding([4, 10])
        .into()
    }

    fn section_view<'a>(
        section: &'a IssueSection,
        is_first: bool,
        is_last: bool,
    ) -> Element<'a, Message> {
        let label = match section.kind {
            SectionType::Category => badge(
                section
                    .category_name
                    .clone()
                    .unwrap_or_else(|| String::from("Category")),
                crate::components::badge::BadgeStyle::Primary,
            ),
            SectionType::Text => badge(
                String::from("Text"),
                crate::components::badge::BadgeStyle::Ghost,
            ),
        };

        let move_up = button(iced::widget::Text::from(Icon::ArrowUp))
            .on_press_maybe((!is_first).then_some(Message::MoveSection(section.id, -1)));
        let move_down = button(iced::widget::Text::from(Icon::ArrowDown))
            .on_press_maybe((!is_last).then_some(Message::MoveSection(section.id, 1)));

        let header = row![
            label,
            horizontal(),
            button("Add article")
                            .on_press(Message::RequestNewArticle(section.id)),
            move_up,
            move_down,
            button(iced::widget::Text::from(Icon::Pencil))
                .on_press(Message::RequestEditSection(section.id)),
            button(iced::widget::Text::from(Icon::Trash2))
                .on_press(Message::RequestDeleteSection(section.id))
                .style(button::danger),
        ]
        .align_y(Center)
        .spacing(6);

        let body: Element<'_, Message> = match section.kind {
            SectionType::Category => {
                let articles = Self::sorted_articles(section);
                let last_index = articles.len().saturating_sub(1);

                let rows: Vec<Element<'_, Message>> = articles
                    .iter()
                    .enumerate()
                    .map(|(index, article)| {
                        Self::article_view(
                            section.id,
                            article,
                            index == 0,
                            index == last_index,
                        )
                    })
                    .collect();

                let list: Element<'_, Message> = if rows.is_empty() {
                    typography(
                        String::from("No article in this category yet."),
                        TypographyStyle::Small,
                    )
                } else {
                    column(rows).spacing(4).into()
                };

                column![
                    row![
                        horizontal(),
                    ],
                    list,
                ]
                .spacing(6)
                .into()
            }
            SectionType::Text => typography(
                preview(section.text_body.as_deref().unwrap_or(""), 160),
                TypographyStyle::Small,
            ),
        };

        Card {
            body: column![header, body].spacing(8).into(),
        }
        .view()
    }

    fn content_view(&self) -> Element<'_, Message> {
        let sections = self.sorted_sections();
        let last_index = sections.len().saturating_sub(1);

        let header = row![
            typography(String::from("Content"), TypographyStyle::H3),
            horizontal(),
            button("Add section").on_press(Message::RequestNewSection),
        ]
        .align_y(Center)
        .spacing(10);

        let list: Element<'_, Message> = if sections.is_empty() {
            typography(
                String::from("No content yet. Add a category or a text block."),
                TypographyStyle::Small,
            )
        } else {
            let rows: Vec<Element<'_, Message>> = sections
                .iter()
                .enumerate()
                .map(|(index, section)| {
                    Self::section_view(section, index == 0, index == last_index)
                })
                .collect();

            column(rows).spacing(8).into()
        };

        column![header, list].spacing(10).into()
    }

    pub fn view(&self) -> Element<'_, Message> {
        if let Some(item) = &self.item {
            let back_button = button("← Back to issues").on_press(Message::BackToList);
            let mut header = row![back_button, horizontal(),].align_y(Center).spacing(10);

            if item.status != IssueStatus::Archive {
                header = header.push(button("Archive").on_press(Message::RequestArchive));
            }
            if item.status != IssueStatus::Published {
                header = header.push(button("Publish").on_press(Message::RequestPublish));
            }

            header = header.push(button("Preview").on_press(Message::RequestPreview));

            let submit = button("Save").on_press(Message::Submit);
            header = header.push(submit);

            let title = typography(
                format!("Issue Number {}", item.issue_number),
                crate::components::typography::TypographyStyle::Title,
            );
            let status = row![
                typography(
                    String::from("Status: "),
                    crate::components::typography::TypographyStyle::Small
                ),
                item.render("status")
            ]
            .align_y(Center);
            let id = row![
                typography(
                    String::from("ID: "),
                    crate::components::typography::TypographyStyle::Small,
                ),
                badge(
                    item.id.to_string(),
                    crate::components::badge::BadgeStyle::Ghost
                )
            ]
            .align_y(Center);
            let created_at = typography(
                format!("Created at: {}", &item.value_from_key("created_at")),
                crate::components::typography::TypographyStyle::Small,
            );
            let updated_at = typography(
                format!("Update at: {}", &item.value_from_key("updated_at")),
                crate::components::typography::TypographyStyle::Small,
            );
            let metadata = row![status, id, created_at, updated_at]
                .align_y(Center)
                .spacing(10);

            let views = typography(
                format!("Views: {}", item.views),
                crate::components::typography::TypographyStyle::Small,
            );
            let opened_mail_count = typography(
                format!("Opened mail: {}", item.opened_mail_count.to_string()),
                crate::components::typography::TypographyStyle::Small,
            );
            let stats = row![views, opened_mail_count].align_y(Center).spacing(10);

            let issue_number_input = form_control(
                "Issue Number",
                "issue number",
                &item.issue_number.to_string(),
                Some(Message::IssueNumberChanged),
                Length::Fixed(150.0),
                None,
                None,
            );

            let slug_action = if !self.auto_slug {
                Some((String::from("Reset to Title"), Message::ResetSlug))
            } else {
                None
            };
            let slug_input = form_control(
                "Slug",
                "slug",
                &item.slug,
                Some(Message::SlugChanged),
                Length::Fill,
                slug_action,
                None,
            );
            let slug_row = row![issue_number_input, slug_input]
                .align_y(Center)
                .spacing(10);

            let title_input = form_control(
                "Title",
                "title",
                &item.title,
                Some(Message::TitleChanged),
                Length::Fill,
                None,
                None,
            );
            let subtitle_input = form_control(
                "Subtitle",
                "subtitle",
                &item.subtitle,
                Some(Message::SubtitleChanged),
                Length::Fill,
                None,
                None,
            );
            let title_row = row![title_input, subtitle_input]
                .align_y(Center)
                .spacing(10);

            let is_sponsored_input = form_control_switch(
                "Is sponsored?",
                item.is_sponsored,
                Some(Message::IsSponsoredChanged),
            );
            let tags_section = row![
                typography(
                    String::from("Tags: "),
                    crate::components::typography::TypographyStyle::Small
                ),
                item.render("tags"),
                horizontal(),
                button("Edit").on_press(Message::RequestEditTags),
            ]
            .align_y(Center)
            .spacing(10);
            let excerpt_input = form_control(
                "Excerpt",
                "excerpt",
                &item.excerpt,
                Some(Message::ExcerptChanged),
                Length::Fill,
                None,
                None,
            );
            let vod_url_input = form_control(
                "VOD URL",
                "vod url",
                &item.vod_url,
                Some(Message::VodUrlChanged),
                Length::Fill,
                None,
                None,
            );

            let cover_preview: Element<'_, Message> = match self
                .pending_cover_handle
                .as_ref()
                .or(self.cover_handle.as_ref())
            {
                Some(handle) => iced_image(handle.clone()).width(200).height(112).into(),
                None => container(typography(
                    String::from("No cover"),
                    TypographyStyle::Small,
                ))
                .width(200)
                .height(112)
                .center(Length::Fill)
                .into(),
            };

            let mut cover_buttons = column![button("Choose an image").on_press(Message::PickCover)]
                .spacing(6);
            if self.pending_cover.is_some() {
                cover_buttons = cover_buttons.push(
                    button("Cancel")
                        .on_press(Message::RemoveCover)
                        .style(button::secondary),
                );
                cover_buttons = cover_buttons.push(typography(
                    String::from("Uploaded when you save."),
                    TypographyStyle::Small,
                ));
            }

            let cover_section = row![
                typography(String::from("Cover"), TypographyStyle::Label),
                horizontal(),
                cover_preview,
                cover_buttons,
            ]
            .align_y(Center)
            .spacing(10);

            let form = column![
                slug_row,
                title_row,
                is_sponsored_input,
                tags_section,
                cover_section,
                excerpt_input,
                vod_url_input,
            ]
            .spacing(10);

            // Un seul défilement, pour l'écran entier : deux zones
            // imbriquées rendaient la molette imprévisible selon le survol.
            scrollable(
                container(
                    column![header, title, metadata, stats, form, self.content_view()].spacing(6),
                )
                .padding(20),
            )
            .height(Length::Fill)
            .width(Length::Fill)
            .into()
        } else {
            // TODO Redirection
            return container("Error").into();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{moved, preview};

    #[test]
    fn moved_returns_the_full_list_in_the_new_order() {
        assert_eq!(moved(vec![1, 2, 3], 3, -1), Some(vec![1, 3, 2]));
        assert_eq!(moved(vec![1, 2, 3], 1, 1), Some(vec![2, 1, 3]));
    }

    #[test]
    fn moved_refuses_to_go_out_of_bounds() {
        assert_eq!(moved(vec![1, 2, 3], 1, -1), None);
        assert_eq!(moved(vec![1, 2, 3], 3, 1), None);
    }

    #[test]
    fn moved_ignores_an_unknown_id() {
        assert_eq!(moved(vec![1, 2, 3], 9, 1), None);
    }

    #[test]
    fn preview_flattens_and_truncates() {
        assert_eq!(preview("# Titre\n\nUn corps", 40), "# Titre Un corps");
        assert_eq!(preview("abcdefghij", 5), "abcde…");
    }
}
