use iced::{
    Border, Color, Element, Length,
    theme::palette::{darken, lighten, mix},
    widget::container,
};

use crate::components::typography::{TypographyStyle, typography};

#[derive(Clone)]
pub enum AlertStyle {
    Primary,
    Secondary,
    Ghost,
    Success,
    Danger,
    Warning,
    FromColor(Color),
}

pub fn alert<'a, M: 'a>(value: String, style: AlertStyle) -> Element<'a, M> {
    container(typography(value, TypographyStyle::Body))
        .style(move |theme| {
            let palette = theme.extended_palette();

            let (strong, weak) = match style {
                AlertStyle::Primary => (palette.primary.strong.color, palette.primary.weak.color),
                AlertStyle::Success => (palette.success.strong.color, palette.success.weak.color),
                AlertStyle::Danger => (palette.danger.strong.color, palette.danger.weak.color),
                AlertStyle::Ghost => (
                    palette.background.strongest.color,
                    palette.background.weakest.color,
                ),
                AlertStyle::FromColor(c) => (c, mix(c, Color::WHITE, 0.15)),
                _ => (palette.secondary.strong.color, palette.secondary.weak.color),
            };

            let color = darken(strong, 0.5);
            let bg = lighten(weak, 0.5);

            container::Style::default()
                .background(bg)
                .color(color)
                .border(Border::default().width(1).color(strong).rounded(2))
        })
        .width(Length::Fill)
        .padding(10)
        .into()
}
