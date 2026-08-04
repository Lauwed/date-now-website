use iced::{
    Alignment, Border, Color, Element, Length,
    border::{self, Radius},
    widget::{Column, button, center, column, container, mouse_area, opaque, row, space, stack},
};

use crate::components::typography::{TypographyStyle, typography};

pub fn modal<'a, Message>(
    base: impl Into<Element<'a, Message>>,
    title: Option<String>,
    content: Element<'a, Message>,
    on_blur: Message,
    on_cancel: Option<Message>,
    on_confirm: Option<Message>,
) -> Element<'a, Message>
where
    Message: Clone + 'a,
{
    let mut content_wrapper: Column<'a, Message> = column![content].spacing(10.0);
    let mut actions_row = row![space().width(Length::Fill)].spacing(10.0);
    let mut display_actions = false;

    if let Some(c) = on_cancel {
        actions_row = actions_row.push(button("Cancel").on_press(c));
        display_actions = true;
    }
    if let Some(c) = on_confirm {
        actions_row = actions_row.push(button("Confirm").on_press(c));
        display_actions = true;
    }

    if display_actions {
        content_wrapper = content_wrapper.push(
            actions_row
                .width(Length::Fill)
                .wrap()
                .align_x(Alignment::End),
        );
    }

    let content_container: Element<'a, Message> =
        container(content_wrapper).padding([12, 10]).into();

    let inner: Element<'a, Message> = container(match title {
        Some(title) => column![
            container(typography(title, TypographyStyle::H3))
                .style(|theme| {
                    let palette = theme.palette();
                    container::Style::default()
                        .background(palette.background)
                        .border(Border {
                            radius: border::top(10.0),
                            ..Border::default()
                        })
                })
                .padding([0, 10])
                .align_y(Alignment::Center)
                .width(Length::Fill)
                .height(Length::Fixed(40.0)),
            container("")
                .width(Length::Fill)
                .height(Length::Fixed(2.0))
                .style(|_theme| {
                    container::Style {
                        background: Some(
                            Color {
                                a: 0.25,
                                ..Color::BLACK
                            }
                            .into(),
                        ),
                        ..container::Style::default()
                    }
                }),
            content_container,
        ]
        .width(Length::Fixed(400.0))
        .into(),
        None => content_container,
    })
    .style(|_theme| {
        container::Style::default()
            .background(Color::WHITE)
            .border(border::rounded(10.0))
    })
    .into();

    stack![
        base.into(),
        opaque(
            mouse_area(center(opaque(inner)).style(|_theme| {
                container::Style {
                    background: Some(
                        Color {
                            a: 0.5,
                            ..Color::BLACK
                        }
                        .into(),
                    ),
                    ..container::Style::default()
                }
            }))
            .on_press(on_blur)
        )
    ]
    .into()
}
