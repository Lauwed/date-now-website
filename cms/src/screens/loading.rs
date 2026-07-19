use iced::widget::text;
use iced::Element;

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
