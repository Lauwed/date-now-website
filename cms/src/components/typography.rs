use iced::{Element, Font, font::Weight, widget::text};

pub enum TypographyStyle {
    Title,
    SubTitle,
    H1,
    H2,
    H3,
    Body,
    Small,
    Label,
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
    };

    let v = match style {
        TypographyStyle::SubTitle => value.to_uppercase(),
        default => String::from(value),
    };

    text(v).font(s).size(size).into()
}
