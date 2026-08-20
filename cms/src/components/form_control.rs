use iced::{
    Alignment::Center,
    Element, Length, Theme,
    widget::{button, column, container, row, space::horizontal, text_input, toggler},
};

use crate::components::typography::typography;

pub fn form_control<'a, M: 'a>(
    label: &str,
    placeholder: &str,
    value: &str,
    msg: Option<fn(String) -> M>,
    width: Length,
    action: Option<(String, M)>,
    error: Option<String>,
) -> Element<'a, M>
where
    M: Clone,
{
    form_control_submit(label, placeholder, value, msg, width, action, error, None)
}

/// Comme `form_control`, avec en plus un message émis quand l'utilisateur
/// valide le champ au clavier (Entrée).
pub fn form_control_submit<'a, M: 'a>(
    label: &str,
    placeholder: &str,
    value: &str,
    msg: Option<fn(String) -> M>,
    width: Length,
    action: Option<(String, M)>,
    error: Option<String>,
    on_submit: Option<M>,
) -> Element<'a, M>
where
    M: Clone,
{
    let label = typography(label.to_string(), super::typography::TypographyStyle::Label);

    let mut label_row = row![label].align_y(Center);
    if let Some((action_label, a)) = action {
        let action_btn = button(typography(
            action_label,
            crate::components::typography::TypographyStyle::Small,
        ))
        .on_press(a);

        label_row = label_row.push(horizontal());
        label_row = label_row.push(action_btn);
    }

    let has_error = error.is_some();

    let input = text_input(placeholder, value)
        .style(move |theme: &Theme, status| {
            let palette = theme.extended_palette();
            let mut style = text_input::default(theme, status); // style de base selon Active/Hovered/etc.

            if has_error {
                style.border.color = palette.danger.base.color;
                style.border.width = 2.0;
            }

            style
        })
        .on_input_maybe(msg)
        .on_submit_maybe(on_submit);

    let mut content = column![label_row, input].spacing(4);

    if let Some(err) = error {
        content = content.push(typography(
            err.to_string(),
            crate::components::typography::TypographyStyle::DangerSmall,
        ))
    }

    container(content).width(width).into()
}

pub fn form_control_switch<'a, M: 'a>(
    label: &str,
    value: bool,
    msg: Option<fn(bool) -> M>,
) -> Element<'a, M>
where
    M: Clone,
{
    let input = toggler(value).label(label.to_string()).on_toggle_maybe(msg);

    container(input).into()
}
