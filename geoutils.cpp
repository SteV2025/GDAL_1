#include "geoutils.h"
#include <algorithm>
#include <cmath>

QRgb ColorMap::mapValue(double value, double vmin, double vmax, Preset type)
{
    // 无效或空数据
    if (!std::isfinite(value))
        return qRgba(0, 0, 0, 0);

    // 防止零区间除零
    if (vmax <= vmin + 1e-12)
        return qRgb(128, 128, 128);

    // 归一化到 [0, 1]
    double ratio = (value - vmin) / (vmax - vmin);
    ratio = std::clamp(ratio, 0.0, 1.0);

    const QVector<QColor>* lut = nullptr;
    switch (type) {
    case Terrain: lut = &terrainLUT(); break;
    case Gray:    lut = &grayLUT(); break;
    case Heat:
    default:      lut = &heatLUT(); break;
    }

    const auto &colors = *lut;
    int n = colors.size() - 1;
    if (n <= 0)
        return colors.isEmpty() ? qRgb(128,128,128) : colors.first().rgb();

    double scaled = ratio * n;
    int i = static_cast<int>(scaled);
    double t = scaled - i;

    const QColor &c1 = colors[i];
    const QColor &c2 = colors[std::min(i + 1, n)];
    return interpolate(c1, c2, t);
}

QRgb ColorMap::interpolate(const QColor &a, const QColor &b, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    int r = static_cast<int>(std::lround(a.red()   * (1 - t) + b.red()   * t));
    int g = static_cast<int>(std::lround(a.green() * (1 - t) + b.green() * t));
    int b_ = static_cast<int>(std::lround(a.blue()  * (1 - t) + b.blue()  * t));
    return qRgb(r, g, b_);
}

// 蓝→青→黄→橙→红
const QVector<QColor> &ColorMap::heatLUT()
{
    static const QVector<QColor> lut = {
        QColor(0,   0,   255),
        QColor(0,   150, 255),
        QColor(0,   255, 150),
        QColor(255, 255, 0),
        QColor(255, 120, 0),
        QColor(180, 0,   0)
    };
    return lut;
}

// 绿→棕→灰→白
const QVector<QColor> &ColorMap::terrainLUT()
{
    static const QVector<QColor> lut = {
        QColor(0,   120, 0),
        QColor(160, 120, 40),
        QColor(180, 180, 180),
        QColor(255, 255, 255)
    };
    return lut;
}

// 黑→白
const QVector<QColor> &ColorMap::grayLUT()
{
    static const QVector<QColor> lut = {
        QColor(0, 0, 0),
        QColor(255, 255, 255)
    };
    return lut;
}
