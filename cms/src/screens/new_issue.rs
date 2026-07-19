use iced::{
    Alignment::Center,
    Element, Length,
    widget::{button, column, container, row, space::horizontal, text_editor},
};
use rslug::slugify;

use crate::{
    components::{
        form_control::{form_control, form_control_switch},
        toast::Status,
        typography::typography,
    },
    data::{
        issues::{Issue as IssueType, create_issue},
        responses::Response,
        sessions::Session,
    },
};

#[derive(Default)]
pub struct NewIssue {
    pub item: IssueType,
    pub session: Session,
    pub content: text_editor::Content,
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
    ContentChanged(text_editor::Action),
    IsSponsoredChanged(bool),
    ResetSlug,
}

pub enum Action {
    None,
    BackToList,
    Toast(String, String, Status),
}

impl NewIssue {
    pub fn new(session: Session) -> Self {
        Self {
            item: IssueType::default(),
            session,
            content: text_editor::Content::new(),
            auto_slug: true,
        }
    }

    pub fn update(&mut self, message: Message) -> Action {
        match message {
            Message::BackToList => Action::BackToList,
            Message::Submit => {
                self.item.content = self.content.text();

                // Save in API
                match create_issue(self.item.clone(), self.session.token.clone()) {
                    Ok(Response::Success(new_issue)) => {
                        self.item = new_issue;

                        Action::BackToList
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
            }
            Message::TitleChanged(value) => {
                let mut slug = self.item.slug.clone();

                if self.auto_slug {
                    slug = slugify!(&value);
                }

                self.item = IssueType {
                    title: value,
                    slug,
                    ..self.item.clone()
                };
                Action::None
            }
            Message::SlugChanged(value) => {
                self.item = IssueType {
                    slug: value,
                    ..self.item.clone()
                };
                self.auto_slug = false;

                Action::None
            }
            Message::ResetSlug => {
                let slug = slugify!(&self.item.title);
                self.item = IssueType {
                    slug,
                    ..self.item.clone()
                };
                self.auto_slug = true;
                Action::None
            }
            Message::SubtitleChanged(value) => {
                self.item = IssueType {
                    subtitle: value,
                    ..self.item.clone()
                };

                Action::None
            }
            Message::IssueNumberChanged(value) => {
                let nb_result = value.parse::<u32>();

                if let Ok(nb) = nb_result {
                    self.item = IssueType {
                        issue_number: nb,
                        ..self.item.clone()
                    };
                }
                Action::None
            }
            Message::ExcerptChanged(value) => {
                self.item = IssueType {
                    excerpt: value,
                    ..self.item.clone()
                };

                Action::None
            }
            Message::ContentChanged(action) => {
                self.content.perform(action);
                Action::None
            }
            Message::IsSponsoredChanged(value) => {
                self.item = IssueType {
                    is_sponsored: value,
                    ..self.item.clone()
                };

                Action::None
            }
        }
    }

    pub fn view(&self) -> Element<'_, Message> {
        let back_button = button("← Back to issues").on_press(Message::BackToList);
        let submit = button("Save").on_press(Message::Submit);
        let header = row![back_button, horizontal(), submit].align_y(Center);

        let title = typography(
            String::from("New Issue"),
            crate::components::typography::TypographyStyle::Title,
        );

        let issue_number_input = form_control(
            "Issue Number",
            "issue number",
            &self.item.issue_number.to_string(),
            Some(Message::IssueNumberChanged),
            Length::Fixed(150.0),
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
            &self.item.slug,
            Some(Message::SlugChanged),
            Length::Fill,
            slug_action,
        );
        let slug_row = row![issue_number_input, slug_input]
            .align_y(Center)
            .spacing(10);

        let title_input = form_control(
            "Title",
            "title",
            &self.item.title,
            Some(Message::TitleChanged),
            Length::Fill,
            None,
        );
        let subtitle_input = form_control(
            "Subtitle",
            "subtitle",
            &self.item.subtitle,
            Some(Message::SubtitleChanged),
            Length::Fill,
            None,
        );
        let title_row = row![title_input, subtitle_input]
            .align_y(Center)
            .spacing(10);

        let is_sponsored_input = form_control_switch(
            "Is sponsored?",
            self.item.is_sponsored,
            Some(Message::IsSponsoredChanged),
        );
        let excerpt_input = form_control(
            "Excerpt",
            "excerpt",
            &self.item.excerpt,
            Some(Message::ExcerptChanged),
            Length::Fill,
            None,
        );

        let content_label = typography(
            String::from("Content"),
            crate::components::typography::TypographyStyle::Label,
        );
        let content_input = text_editor(&self.content)
            .on_action(Message::ContentChanged)
            .height(Length::Fixed(100.0));
        let content_form = column![content_label, content_input].spacing(4);

        let form = column![
            slug_row,
            title_row,
            is_sponsored_input,
            excerpt_input,
            content_form,
        ]
        .spacing(10);

        return container(column![header, title, form].spacing(6))
            .padding(20)
            .into();
    }
}
