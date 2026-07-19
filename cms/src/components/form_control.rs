use iced::{
    Alignment::Center,
    Element, Length,
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

    let input = text_input(placeholder, value).on_input_maybe(msg);

    container(column![label_row, input].spacing(4))
        .width(width)
        .into()
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
