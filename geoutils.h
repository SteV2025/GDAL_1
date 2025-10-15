#pragma once
#include <QColor>
#include <QVector>
#include <QRgb>

/**
 * @brief ColorMap 工具类 - 提供伪彩色映射功能（冷→热）
 * 支持连续插值和 LUT 加速两种模式。
 */
class ColorMap
{
public:
    enum Preset {
        Heat,      ///< 蓝→青→黄→橙→红（默认）
        Terrain,   ///< 绿→棕→灰→白
        Gray       ///< 灰度
    };

    static QRgb mapValue(double value, double vmin, double vmax,
                         Preset type = Heat);

private:
    static QRgb interpolate(const QColor &a, const QColor &b, double t);
    static const QVector<QColor> &heatLUT();
    static const QVector<QColor> &terrainLUT();
    static const QVector<QColor> &grayLUT();
};
