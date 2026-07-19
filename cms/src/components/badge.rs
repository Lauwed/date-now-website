use iced::{
    Border, Color, Element,
    theme::palette::{darken, lighten, mix},
    widget::container,
};

use crate::components::typography::{TypographyStyle, typography};

pub enum BadgeStyle {
    Primary,
    Secondary,
    Ghost,
    Success,
    Danger,
    Warning,
    FromColor(Color),
}

pub fn badge<'a, M: 'a>(value: String, style: BadgeStyle) -> Element<'a, M> {
    container(typography(value, TypographyStyle::Small))
        .style(move |theme| {
            let palette = theme.extended_palette();

            let (strong, weak) = match style {
                BadgeStyle::Primary => (palette.primary.strong.color, palette.primary.weak.color),
                BadgeStyle::Success => (palette.success.strong.color, palette.success.weak.color),
                BadgeStyle::Ghost => (
                    palette.background.strongest.color,
                    palette.background.weakest.color,
                ),
                BadgeStyle::FromColor(c) => (c, mix(c, Color::WHITE, 0.15)),
                _ => (palette.secondary.strong.color, palette.secondary.weak.color),
            };

            let color = darken(strong, 0.5);
            let bg = lighten(weak, 0.5);

            container::Style::default()
                .background(bg)
                .color(color)
                .border(Border::default().width(1).color(color).rounded(10))
        })
        .padding([4, 8])
        .into()
}
