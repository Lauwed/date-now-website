use iced::{Color, Element, Font, font::Weight, widget::text};
use std::str::FromStr;

pub enum TypographyStyle {
    Title,
    SubTitle,
    H1,
    H2,
    H3,
    Body,
    Small,
    Label,
    DangerSmall,
}

pub fn typography<'a, M>(value: String, style: TypographyStyle) -> Element<'a, M> {
    let s = match style {
        TypographyStyle::Title => Font {
            family: Font::with_name("General Sans Variable").family,
            weight: Weight::Black,
            ..Default::default()
        },
        TypographyStyle::SubTitle => Font {
            family: Font::with_name("General Sans Variable").family,
            ..Default::default()
        },
        TypographyStyle::H1 => Font {
            family: Font::with_name("General Sans Variable").family,
            weight: Weight::Black,
            ..Default::default()
        },
        TypographyStyle::H2 => Font {
            family: Font::with_name("General Sans Variable").family,
            weight: Weight::Black,
            ..Default::default()
        },
        TypographyStyle::H3 => Font {
            family: Font::with_name("General Sans Variable").family,
            weight: Weight::Black,
            ..Default::default()
        },
        TypographyStyle::Body => Font {
            family: Font::with_name("General Sans Variable").family,
            ..Default::default()
        },
        TypographyStyle::Small => Font {
            family: Font::with_name("General Sans Variable").family,
            ..Default::default()
        },
        TypographyStyle::Label => Font {
            family: Font::with_name("General Sans Variable").family,
            ..Default::default()
        },
        TypographyStyle::DangerSmall => Font {
            family: Font::with_name("General Sans Variable").family,
            ..Default::default()
        },
    };

    let size = match style {
        TypographyStyle::Title => 40,
        TypographyStyle::SubTitle => 14,
        TypographyStyle::H1 => 32,
        TypographyStyle::H2 => 28,
        TypographyStyle::H3 => 24,
        TypographyStyle::Body => 14,
        TypographyStyle::Small => 12,
        TypographyStyle::Label => 14,
        TypographyStyle::DangerSmall => 12,
    };

    let v = match style {
        TypographyStyle::SubTitle => value.to_uppercase(),
        _ => String::from(value),
    };

    text(v)
        .color(match style {
            TypographyStyle::DangerSmall => Color::from_str("#D9281C").unwrap(),
            _ => Color::BLACK,
        })
        .font(s)
        .size(size)
        .into()
}
