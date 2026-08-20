use iced::Alignment::Center;
use iced::widget::{column, container, row, text};
use iced::{Element, Length};

use crate::components::card::Card;
use crate::components::charts::bar_chart;
use crate::components::typography::{TypographyStyle, typography};
use crate::data::issues::Issue;
use crate::data::sessions::Session;
use crate::data::stats;

#[derive(Debug, Clone)]
pub enum Message {
    Reload,
}

pub struct Dashboard {
    subscriber_count: i64,
    author_count: i64,
    published_count: i64,
    draft_count: i64,
    archived_count: i64,
    sponsor_count: u32,
    total_views: u32,
    recent_issues: Vec<Issue>,
}

impl Default for Dashboard {
    fn default() -> Self {
        Self {
            subscriber_count: 0,
            author_count: 0,
            published_count: 0,
            draft_count: 0,
            archived_count: 0,
            sponsor_count: 0,
            total_views: 0,
            recent_issues: vec![],
        }
    }
}

impl Dashboard {
    pub fn new(session: &Session) -> Self {
        let token = &session.token;
        Self {
            subscriber_count: stats::get_subscriber_count(token).unwrap_or_default(),
            author_count: stats::get_author_count(token).unwrap_or_default(),
            published_count: stats::get_issue_count(Some("PUBLISHED"), token).unwrap_or_default(),
            draft_count: stats::get_issue_count(Some("DRAFT"), token).unwrap_or_default(),
            archived_count: stats::get_issue_count(Some("ARCHIVE"), token).unwrap_or_default(),
            sponsor_count: stats::get_sponsor_count().unwrap_or_default(),
            total_views: stats::get_total_view_count(token).unwrap_or_default(),
            recent_issues: stats::get_recent_issues_for_dashboard(10, token),
        }
    }

    pub fn update(&mut self, _message: Message) {}

    pub fn view(&self) -> Element<'_, Message> {
        let kpi = |label: &str, value: String| {
            Card {
                body: column![
                    typography(label.to_string(), TypographyStyle::SubTitle),
                    text(value).size(36),
                ]
                .align_x(Center)
                .spacing(6)
                .into(),
            }
            .view()
        };

        let kpi_row = row![
            kpi("Subscribers", self.subscriber_count.to_string()),
            kpi("Authors", self.author_count.to_string()),
            kpi("Sponsors", self.sponsor_count.to_string()),
            kpi("Total views", self.total_views.to_string()),
        ]
        .spacing(20)
        .width(Length::Fill);

        let status_row = row![
            kpi("Published", self.published_count.to_string()),
            kpi("Draft", self.draft_count.to_string()),
            kpi("Archived", self.archived_count.to_string()),
        ]
        .spacing(20)
        .width(Length::Fill);

        let views_data: Vec<(String, f32)> = self
            .recent_issues
            .iter()
            .map(|i| (format!("#{}", i.issue_number), i.views as f32))
            .collect();

        let views_chart = Card {
            body: column![
                typography(String::from("Views per issue"), TypographyStyle::SubTitle),
                bar_chart(views_data, 10),
            ]
            .spacing(10)
            .into(),
        };

        let mut top_issues = self.recent_issues.clone();
        top_issues.sort_by(|a, b| b.views.cmp(&a.views));
        top_issues.truncate(5);

        let top_list = Card {
            body: column(
                std::iter::once(typography(
                    String::from("Top 5 most viewed"),
                    TypographyStyle::SubTitle,
                ))
                .chain(top_issues.iter().map(|i| {
                    row![
                        text(format!("#{} — {}", i.issue_number, i.title)),
                        iced::widget::space::horizontal(),
                        text(format!("{} views", i.views)),
                    ]
                    .spacing(10)
                    .into()
                }))
                .collect::<Vec<_>>(),
            )
            .spacing(8)
            .into(),
        };

        column![
            kpi_row,
            status_row,
            row![views_chart.view(), top_list.view()].spacing(20)
        ]
        .spacing(20)
        .padding(20)
        .into()
    }
}
