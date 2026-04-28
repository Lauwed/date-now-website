use iced::font::Weight;
use iced::widget::{button, column, text, text_input};
use iced::{Element, Font};
use reqwest;

#[derive(Debug, Clone)]
pub enum Message {
    InputChanged(String),
    SendConfirmationToken,
}

#[derive(Default)]
pub struct Login {
    email: String,
    confirmation_response: String,
}

impl Login {
    pub fn update(&mut self, message: Message) {
        match message {
            Message::InputChanged(email) => {
                self.email = email;
            }
            Message::SendConfirmationToken => {
                let url = "http://localhost:8000/api/auth/login";

                let client = reqwest::blocking::Client::new();
                let res = client
                    .post(url)
                    .body(format!(r#"{{ "email": "{}" }}"#, self.email))
                    .send();

                let confirmation_text = "Please verify your Email box".to_string();

                self.confirmation_response = match res {
                    Ok(r) => match r.error_for_status() {
                        Ok(content) => match content.text() {
                            Ok(_) => confirmation_text,
                            Err(parsing_err) => {
                                format!("Error while parsing response body: {}", parsing_err)
                            }
                        },
                        Err(login_err) => match login_err.status() {
                            Some(status) => match status {
                                reqwest::StatusCode::NOT_FOUND => confirmation_text,
                                default => format!("login failed: {}", default),
                            },
                            None => format!("Error while retrieve status code"),
                        },
                    },
                    Err(req_err) => format!("request failed: {}", req_err),
                }
            }
        }
    }
    pub fn view(&self) -> Element<'_, Message> {
        let title = text("Login")
            .font(Font {
                weight: Weight::Black,
                ..Default::default()
            })
            .size(40);

        let email_input = column![
            text("Email"),
            text_input("Give me your Email uwu", &self.email).on_input(Message::InputChanged)
        ]
        .spacing(10);

        let send_button = button("Send confirmation").on_press(Message::SendConfirmationToken);
        let confirmation_alert = text(self.confirmation_response.to_string());

        column![title, email_input, send_button, confirmation_alert]
            .padding(40)
            .spacing(20)
            .into()
    }
}
