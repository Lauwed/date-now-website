use iced::font::Weight;
use iced::widget::{button, column, container, image, stack, svg, text, text_input};
use iced::{Alignment, Border, Color, ContentFit, Element, Font, Length, Shadow, border};
use reqwest;

use crate::components::alert::{AlertStyle, alert};
use crate::components::form_control::form_control_submit;
use crate::components::typography::{TypographyStyle, typography};
use crate::data::auth;

#[derive(Debug, Clone)]
pub enum Message {
    InputChanged(String),
    SendConfirmationToken,
}

#[derive(Default)]
pub struct Login {
    email: String,
    email_err: Option<String>,
    confirmation_response: Option<(String, AlertStyle)>,
}

impl Login {
    pub fn update(&mut self, message: Message) {
        match message {
            Message::InputChanged(email) => {
                self.email_err = None;
                self.email = email;
            }
            Message::SendConfirmationToken => {
                if self.email.is_empty() {
                    self.email_err = Some("Email is required.".to_string());

                    return;
                }

                let success = (
                    "Please verify your Email box".to_string(),
                    AlertStyle::Success,
                );

                let confirmation_text = match auth::send_login_confirmation_mail(&self.email) {
                    Ok(r) => match r.error_for_status() {
                        Ok(content) => match content.text() {
                            Ok(_) => success,
                            Err(parsing_err) => (
                                format!("Error while parsing response body: {}", parsing_err),
                                AlertStyle::Danger,
                            ),
                        },
                        Err(login_err) => match login_err.status() {
                            Some(status) => match status {
                                reqwest::StatusCode::NOT_FOUND => success,
                                default => {
                                    (format!("login failed: {}", default), AlertStyle::Danger)
                                }
                            },
                            None => (
                                format!("Error while retrieve status code"),
                                AlertStyle::Danger,
                            ),
                        },
                    },
                    Err(req_err) => (format!("request failed: {}", req_err), AlertStyle::Danger),
                };

                self.confirmation_response = Some(confirmation_text);
            }
        }
    }
    pub fn view(&self) -> Element<'_, Message> {
        let logo = svg(svg::Handle::from_path("assets/images/Logo.svg")).width(Length::Fill);
        let background = image(image::Handle::from_path("assets/images/bg.png"))
            .width(Length::Fill)
            .height(Length::Fill)
            .content_fit(ContentFit::Cover);

        let title = typography("Welcome back".to_string(), TypographyStyle::H1);
        let subtitle = typography(
            "Sign in to access the CMS".to_string(),
            TypographyStyle::Body,
        );
        let title_container = column![title, subtitle].spacing(6.0);

        let email_input = form_control_submit(
            "Email",
            "kakou@kakou.com",
            &self.email,
            Some(Message::InputChanged),
            Length::Fill,
            None,
            self.email_err.clone(),
            Some(Message::SendConfirmationToken),
        );

        let send_button = button("Send confirmation").on_press(Message::SendConfirmationToken);

        let mut content = column![logo, title_container, email_input, send_button,]
            .padding(40)
            .spacing(20);

        if let Some((text, style)) = &self.confirmation_response {
            content = content.push(alert(text.to_string(), style.clone()));
        }

        let content_container = container(
            container(content)
                .style(|_theme| {
                    container::Style::default()
                        .background(Color::WHITE)
                        .border(border::rounded(10.0))
                        .shadow(Shadow {
                            color: Color::from_rgba(0.0, 0.0, 0.0, 0.25),
                            offset: [-1.0, 1.0].into(),
                            blur_radius: 9.0,
                        })
                })
                .width(Length::Fixed(500.0)),
        )
        .width(Length::Fill)
        .height(Length::Fill)
        .center(Length::Fill);

        stack![background, content_container].into()
    }
}
