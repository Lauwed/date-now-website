use std::sync::Arc;

use iced::{
    Element, Point,
    widget::{button, container, row, text_editor},
};
use lucide_icons::Icon;

use crate::components::markdown_editor::{self, MarkdownEditor};

use super::{EditorMode, Message, ToolbarAction};

pub enum Action {
    Move(text_editor::Motion),
    Select(text_editor::Motion),
    SelectWord,
    SelectLine,
    SelectAll,
    Edit(Edit),
    Click(Point),
    Drag(Point),
    Scroll { lines: i32 },
}

pub enum Edit {
    Insert(char),
    Paste(Arc<String>),
    Enter,
    Indent,
    Unindent,
    Backspace,
    Delete,
}

fn icon_button(icon: Icon, on_press: Option<Message>) -> Element<'static, Message> {
    button(iced::widget::Text::from(icon))
        .on_press_maybe(on_press)
        .into()
}

pub fn view(editor: &MarkdownEditor) -> Element<'_, Message> {
    let formatting_enabled = !matches!(editor.mode, EditorMode::Preview);

    let action = |a: ToolbarAction| formatting_enabled.then_some(Message::ToolbarAction(a));

    row![
        icon_button(Icon::Bold, action(ToolbarAction::Bold)),
        icon_button(Icon::Italic, action(ToolbarAction::Italic)),
        icon_button(Icon::Heading1, action(ToolbarAction::Heading(1))),
        icon_button(Icon::Heading2, action(ToolbarAction::Heading(2))),
        icon_button(Icon::Heading3, action(ToolbarAction::Heading(3))),
        icon_button(Icon::List, action(ToolbarAction::BulletList)),
        icon_button(Icon::ListOrdered, action(ToolbarAction::NumberedList)),
        icon_button(Icon::Quote, action(ToolbarAction::Quote)),
        icon_button(Icon::Code, action(ToolbarAction::Code)),
        icon_button(Icon::FileBracesCorner, action(ToolbarAction::CodeBlock)),
        icon_button(Icon::Link, action(ToolbarAction::Link)),
        iced::widget::space::horizontal(),
        icon_button(Icon::Pencil, Some(Message::ModeChanged(EditorMode::Edit))),
        icon_button(Icon::Eye, Some(Message::ModeChanged(EditorMode::Preview))),
        icon_button(Icon::Columns, Some(Message::ModeChanged(EditorMode::Split))),
    ]
    .spacing(4)
    .into()
}

pub fn apply(content: &mut iced::widget::text_editor::Content, action: ToolbarAction) {
    let paste = |content: &mut iced::widget::text_editor::Content, text: String| {
        content.perform(text_editor::Action::Edit(text_editor::Edit::Paste(
            Arc::new(text),
        )));
    };

    let wrap = |content: &mut iced::widget::text_editor::Content, before: &str, after: &str| {
        let wrapped = match content.selection() {
            Some(selected) => format!("{before}{selected}{after}"),
            None => format!("{before}{after}"),
        };
        paste(content, wrapped);
    };

    let prefix_lines = |content: &mut iced::widget::text_editor::Content,
                        make_prefix: &dyn Fn(usize) -> String| {
        if content.selection().is_none() {
            content.perform(text_editor::Action::SelectLine);
        }
        let Some(selected) = content.selection() else {
            return;
        };

        let transformed = selected
            .lines()
            .enumerate()
            .map(|(i, line)| {
                format!(
                    "{}{}",
                    make_prefix(i),
                    line.trim_start_matches(['#', '-', '>']).trim_start()
                )
            })
            .collect::<Vec<_>>()
            .join("\n");

        paste(content, transformed);
    };

    match action {
        ToolbarAction::Bold => wrap(content, "**", "**"),
        ToolbarAction::Italic => wrap(content, "*", "*"),
        ToolbarAction::Code => wrap(content, "`", "`"),
        ToolbarAction::CodeBlock => wrap(content, "```\n", "\n```"),
        ToolbarAction::Link => wrap(content, "[", "](url)"),
        ToolbarAction::Heading(level) => {
            let prefix = "#".repeat(level as usize) + " ";
            prefix_lines(content, &|_| prefix.clone());
        }
        ToolbarAction::BulletList => prefix_lines(content, &|_| "- ".to_string()),
        ToolbarAction::NumberedList => prefix_lines(content, &|i| format!("{}. ", i + 1)),
        ToolbarAction::Quote => prefix_lines(content, &|_| "> ".to_string()),
    }
}
