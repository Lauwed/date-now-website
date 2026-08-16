use iced::Alignment::Center;
use iced::widget::canvas::{self, Canvas, Frame, Geometry, Path, Stroke};
use iced::{Color, Element, Length, Point, Rectangle, Renderer, Size, Theme};

pub struct BarChart {
    pub values: Vec<(String, f32)>,
    pub max_bars: usize,
}

impl<Message> canvas::Program<Message> for BarChart {
    type State = ();

    fn draw(
        &self,
        _state: &Self::State,
        renderer: &Renderer,
        theme: &Theme,
        bounds: Rectangle,
        _cursor: iced::mouse::Cursor,
    ) -> Vec<Geometry> {
        let mut frame = Frame::new(renderer, bounds.size());
        let palette = theme.extended_palette();

        let values: Vec<_> = self.values.iter().rev().take(self.max_bars).collect();
        if values.is_empty() {
            return vec![frame.into_geometry()];
        }

        let max_value = values
            .iter()
            .map(|(_, v)| *v)
            .fold(0.0_f32, f32::max)
            .max(1.0);
        let bar_width = bounds.width / values.len() as f32;
        let padding = bar_width * 0.2;
        let chart_height = bounds.height - 24.0; // laisse de la place au label sous chaque barre

        for (i, (label, value)) in values.iter().enumerate() {
            let height = (value / max_value) * chart_height;
            let x = i as f32 * bar_width + padding / 2.0;
            let y = chart_height - height;

            let bar = Path::rectangle(Point::new(x, y), Size::new(bar_width - padding, height));
            frame.fill(&bar, palette.primary.base.color);

            frame.fill_text(canvas::Text {
                content: label.clone(),
                position: Point::new(x + (bar_width - padding) / 2.0, chart_height + 4.0),
                color: palette.background.base.text,
                size: iced::Pixels(11.0),
                ..Default::default()
            });
        }

        vec![frame.into_geometry()]
    }
}

pub fn bar_chart<'a, Message: 'a>(
    values: Vec<(String, f32)>,
    max_bars: usize,
) -> Element<'a, Message> {
    Canvas::new(BarChart { values, max_bars })
        .width(Length::Fill)
        .height(Length::Fixed(220.0))
        .into()
}
