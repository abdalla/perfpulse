#include "app/icon_factory.hpp"

#include "app/gdiplus_include.hpp"

namespace perfpulse::app {

namespace {

using Gdiplus::Color;

void roundedRectPath(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float radius) {
    const float d = radius * 2.0f;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
}

}  // namespace

HICON createTrayIcon(TrayState state, int size) {
    Gdiplus::Bitmap bitmap(size, size, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&bitmap);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.Clear(Color(0, 0, 0, 0));

        const float s = static_cast<float>(size) / 32.0f;
        Color from(255, 124, 92, 255);
        Color to(255, 34, 211, 238);
        if (state == TrayState::Paused || state == TrayState::Warning) {
            from = Color(255, 120, 120, 132);
            to = Color(255, 90, 90, 100);
        } else if (state == TrayState::Detecting) {
            from = Color(255, 245, 158, 11);
            to = Color(255, 234, 88, 12);
        }
        Gdiplus::GraphicsPath background;
        roundedRectPath(background, 0.5f, 0.5f, size - 1.0f, size - 1.0f, 7.0f * s);
        Gdiplus::LinearGradientBrush fill(Gdiplus::PointF(0, 0), Gdiplus::PointF(static_cast<float>(size),
                                                                                  static_cast<float>(size)),
                                          from, to);
        g.FillPath(&fill, &background);

        const Gdiplus::PointF pulse[] = {{4 * s, 17 * s},  {10 * s, 17 * s}, {13 * s, 8 * s}, {17 * s, 25 * s},
                                         {20 * s, 14 * s}, {22 * s, 17 * s}, {28 * s, 17 * s}};
        Gdiplus::Pen pen(Color(255, 255, 255, 255), std::max(1.5f, 2.8f * s));
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLines(&pen, pulse, static_cast<INT>(std::size(pulse)));

        if (state == TrayState::Recording || state == TrayState::Warning) {
            const Color dot = state == TrayState::Recording ? Color(255, 239, 68, 68) : Color(255, 245, 158, 11);
            Gdiplus::SolidBrush dotBrush(dot);
            Gdiplus::Pen ring(Color(255, 255, 255, 255), 1.5f * s);
            const float r = 11.0f * s;
            g.FillEllipse(&dotBrush, size - r - 0.5f, 0.5f, r, r);
            g.DrawEllipse(&ring, size - r - 0.5f, 0.5f, r, r);
        }
    }
    HICON icon = nullptr;
    bitmap.GetHICON(&icon);
    return icon;
}

}  // namespace perfpulse::app
