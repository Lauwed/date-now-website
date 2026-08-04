use iced::{
    Alignment::Center,
    Element, Length,
    widget::{button, column, container, row, space::horizontal},
};
use rslug::slugify;

use crate::{
    components::{
        badge::badge,
        form_control::{form_control, form_control_switch},
        markdown_editor::{self, MarkdownEditor},
        toast::Status,
        typography::typography,
    },
    data::{
        issues::{Issue as IssueType, IssueStatus, get_issue, update_issue},
        responses::Response,
        sessions::Session,
        traits::Table,
    },
};

#[derive(Default)]
pub struct Issue {
    pub id: u32,
    pub item: Option<IssueType>,
    pub session: Session,
    pub content: MarkdownEditor,
    pub auto_slug: bool,
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
    ContentEditor(markdown_editor::Message),
    IsSponsoredChanged(bool),
    ResetSlug,
    RequestPublish,
    RequestSend,
    RequestArchive,
}

pub enum Action {
    None,
    BackToList,
    Toast(String, String, Status),
    ConfirmPublish(u32),
    ConfirmSend(u32),
    ConfirmArchive(u32),
}

impl Issue {
    pub fn new(id: u32, session: Session) -> Self {
        let item = match get_issue(id) {
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

        Self {
            id,
            item: item.clone(),
            session,
            content: match &item {
                Some(data) => MarkdownEditor::new(&data.content),
                None => MarkdownEditor::new(""),
            },
            auto_slug: match &item {
                Some(data) => slugify!(&data.title) == data.slug,
                None => true,
            },
        }
    }

    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::BackToList => Action::BackToList,
            Message::Submit => {
                if let Some(issue) = &mut self.item {
                    issue.content = self.content.text();

                    // Save in API
                    match update_issue(self.id, issue.clone(), self.session.token.clone()) {
                        Ok(Response::Success(new_issue)) => {
                            self.item = Some(new_issue);

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
            Message::ContentEditor(action) => {
                self.content.update(action);
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
            Message::RequestSend => Action::ConfirmSend(self.id),
            Message::RequestArchive => Action::ConfirmArchive(self.id),
        }
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
            let excerpt_input = form_control(
                "Excerpt",
                "excerpt",
                &item.excerpt,
                Some(Message::ExcerptChanged),
                Length::Fill,
                None,
                None,
            );

            let content_label = typography(
                String::from("Content"),
                crate::components::typography::TypographyStyle::Label,
            );
            let content_input = self.content.view().map(Message::ContentEditor);
            let content_form = column![content_label, content_input].spacing(4);

            let form = column![
                slug_row,
                title_row,
                is_sponsored_input,
                excerpt_input,
                content_form,
            ]
            .spacing(10);

            return container(column![header, title, metadata, stats, form].spacing(6))
                .padding(20)
                .into();
        } else {
            // TODO Redirection
            return container("Error").into();
        }
    }
}
