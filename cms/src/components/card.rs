use iced::widget::container;
use iced::{Color, Element, Shadow, border};

pub struct Card<'a, M> {
    pub body: Element<'a, M>,
}

impl<'a, M: 'a> Card<'a, M> {
    pub fn update(&mut self, message: M) {}

    pub fn view(self) -> Element<'a, M> {
        container(self.body)
            .padding(20)
            .style(|theme| {
                let palette = theme.extended_palette();

                container::Style::default()
                    .background(palette.background.weak.color.scale_alpha(0.25))
                    .border(
                        border::rounded(10)
                            .width(2)
                            .color(palette.primary.base.color),
                    )
                    .shadow(Shadow {
                        color: Color::from_rgba(0.0, 0.0, 0.0, 0.25),
                        offset: [-1.0, 1.0].into(),
                        blur_radius: 9.0,
                    })
            })
            .into()
    }
}
