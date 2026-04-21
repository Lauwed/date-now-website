use iced::Element;
use iced::widget::text;

#[derive(Debug, Clone, Copy)]
pub enum Message {
    Increment,
    Decrement,
}

#[derive(Default)]
pub struct Issues {
    value: i64,
}

impl Issues {
    pub fn update(&mut self, message: Message) {}
    pub fn view(&self) -> Element<'_, Message> {
        text("kuku").into()
    }
}
