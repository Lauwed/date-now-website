use iced::Element;

pub trait Table {
    fn value_from_key(&self, key: &str) -> String;
    fn render<'a, M: 'a>(&self, key: &str) -> Element<'a, M>;
}
