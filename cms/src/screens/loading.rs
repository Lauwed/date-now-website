use iced::Element;
use iced::widget::text;

#[derive(Debug, Clone)]
pub enum Message {}

#[derive(Default)]
pub struct Loading {}

impl Loading {
    pub fn update(&mut self, message: Message) {}
    pub fn view(&self) -> Element<'_, Message> {
        text("loading").into()
    }
}
