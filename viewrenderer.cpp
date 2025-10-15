#include "viewrenderer.h"
#include <QtMath>
#include <QLinearGradient>
#include <QFontMetrics>
#include <QDebug>

/**
 * @brief 绘制整个视图内容（包括影像、网格、边框、刻度等）
 */
void ViewRenderer::renderAll(QPainter *p, const ViewController &controller,
                             const RasterData &raster, bool showGrid)
{
    if (!p || controller.viewportSize().isEmpty()) return;

    const QSizeF viewport = controller.viewportSize();
    QRectF frame(0, 0, viewport.width(), viewport.height());
    QTransform worldToScreen = controller.worldToScreen();

    // === 世界坐标层 ===
    p->save();
    p->setTransform(worldToScreen);

    const QRectF geoRect = raster.geoExtent();
    const QImage &img = raster.image();

    if (geoRect.isValid() && !img.isNull()) {
        p->drawImage(geoRect, img);
    }
    p->restore();

    // === 屏幕层 ===
    if (showGrid)
        drawLatLonGrid(p, controller, frame);
}

/**
 * @brief 绘制“井字格”经纬度网格 + 外框 + 刻度 + 比例尺
 */
void ViewRenderer::drawLatLonGrid(QPainter *p, const ViewController &controller, const QRectF &frame)
{
    if (!p) return;
    p->save();

    QTransform worldToScreen = controller.worldToScreen();
    QTransform screenToWorld = controller.screenToWorld();
    double zoom = controller.zoomFactor();

    // === 动态步长（最小0.25°）===
    double stepLon = 30.0;
    if (zoom >= 2.0) stepLon = 15.0;
    if (zoom >= 5.0) stepLon = 10.0;
    if (zoom >= 10.0) stepLon = 5.0;
    if (zoom >= 20.0) stepLon = 2.0;
    if (zoom >= 60.0) stepLon = 1.0;
    if (zoom >= 120.0) stepLon = 0.5;
    if (zoom >= 240.0) stepLon = 0.25;   // ✅ 限制最小间距
    double stepLat = stepLon / 2.0;

    // === 当前地理可视范围 ===
    QPointF topLeftGeo = screenToWorld.map(QPointF(0, 0));
    QPointF bottomRightGeo = screenToWorld.map(QPointF(frame.width(), frame.height()));
    double lonMin = std::floor(std::min(topLeftGeo.x(), bottomRightGeo.x()) / stepLon) * stepLon;
    double lonMax = std::ceil(std::max(topLeftGeo.x(), bottomRightGeo.x()) / stepLon) * stepLon;
    double latMin = std::floor(std::min(topLeftGeo.y(), bottomRightGeo.y()) / stepLat) * stepLat;
    double latMax = std::ceil(std::max(topLeftGeo.y(), bottomRightGeo.y()) / stepLat) * stepLat;

    // === 防止缩得太小导致上千条线 ===
    int maxLines = 500;
    if ((lonMax - lonMin) / stepLon > maxLines ||
        (latMax - latMin) / stepLat > maxLines) {
        p->restore();
        return;
    }

    // === 绘制井字格 ===
    p->setPen(QPen(QColor(210, 210, 210, 150), 0));
    for (double lon = lonMin; lon <= lonMax; lon += stepLon) {
        for (double lat = latMin; lat <= latMax; lat += stepLat) {
            QPointF p00 = worldToScreen.map(QPointF(lon, lat));
            QPointF p10 = worldToScreen.map(QPointF(lon + stepLon, lat));
            QPointF p01 = worldToScreen.map(QPointF(lon, lat + stepLat));
            QPointF p11 = worldToScreen.map(QPointF(lon + stepLon, lat + stepLat));
            QPolygonF cell;
            cell << p00 << p10 << p11 << p01 << p00;
            p->drawPolyline(cell);
        }
    }

    // === 外框 ===
    p->setPen(QPen(Qt::gray, 1.2));
    p->drawRect(frame.adjusted(1, 1, -1, -1));

    // === 刻度 ===
    QFont font = p->font();
    font.setPointSizeF(9);
    p->setFont(font);
    QFontMetrics fm(font);
    p->setPen(QPen(QColor(90, 90, 90), 1));

    auto formatDeg = [](double v) -> QString {
        QString s = QString::number(v, 'f', (std::fabs(v) < 1.0) ? 2 : ((std::fmod(v, 1.0) == 0) ? 0 : 1));
        if (!s.contains('.')) s += ".0"; // 美化 30 → 30.0°
        return s + "°";
    };

    // 经度标签
    for (double lon = lonMin; lon <= lonMax + 1e-9; lon += stepLon) {
        QPointF topPt = worldToScreen.map(QPointF(lon, latMax));
        QPointF bottomPt = worldToScreen.map(QPointF(lon, latMin));
        if (topPt.x() < frame.left() || topPt.x() > frame.right()) continue;
        QString label = formatDeg(lon);
        p->drawText(QPointF(topPt.x() - fm.horizontalAdvance(label)/2, frame.top() + 15), label);
        p->drawText(QPointF(bottomPt.x() - fm.horizontalAdvance(label)/2, frame.bottom() - 7), label);
    }

    // 纬度标签
    for (double lat = latMin; lat <= latMax + 1e-9; lat += stepLat) {
        QPointF leftPt = worldToScreen.map(QPointF(lonMin, lat));
        QPointF rightPt = worldToScreen.map(QPointF(lonMax, lat));
        if (leftPt.y() < frame.top() || leftPt.y() > frame.bottom()) continue;
        QString label = formatDeg(lat);
        p->drawText(QPointF(frame.left() + 8, leftPt.y() + fm.ascent()/2), label);
        p->drawText(QPointF(frame.right() - fm.horizontalAdvance(label) - 8, rightPt.y() + fm.ascent()/2), label);
    }

    // ✅ 绘制比例尺
    drawScaleBar(p, controller, frame);

    p->restore();
}

/**
 * @brief 绘制比例尺（右下角，动态长度/单位）
 */
void ViewRenderer::drawScaleBar(QPainter *p, const ViewController &controller, const QRectF &frame)
{
    if (!p) return;
    p->save();

    const int margin = 20;
    const int barHeight = 8;

    QPointF screenRB(frame.right() - margin, frame.bottom() - margin);
    QPointF screenLB(frame.right() - 160, frame.bottom() - margin);  // 初始假设150px宽
    QPointF geoR = controller.screenToWorld().map(screenRB);
    QPointF geoL = controller.screenToWorld().map(screenLB);

    double degDist = std::fabs(geoR.x() - geoL.x());
    double km = degDist * 111.0; // 简化：1° ≈ 111 km

    // === 动态取整比例尺长度 ===
    double niceKm[] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
    double targetKm = niceKm[0];
    for (double v : niceKm)
        if (v >= km / 2.0) { targetKm = v; break; }

    double degLen = targetKm / 111.0;
    QPointF geoStart = geoR;
    QPointF geoEnd(geoR.x() - degLen, geoR.y());
    QPointF screenStart = controller.worldToScreen().map(geoStart);
    QPointF screenEnd = controller.worldToScreen().map(geoEnd);

    QRectF barRect(screenEnd.x(), screenStart.y() - barHeight,
                   screenStart.x() - screenEnd.x(), barHeight);

    p->setPen(Qt::black);
    p->setBrush(QColor(60, 60, 60));
    p->drawRect(barRect);

    QFont font = p->font();
    font.setPointSizeF(9);
    p->setFont(font);
    QFontMetrics fm(font);
    QString label = (targetKm >= 1000)
        ? QString::number(targetKm / 1000.0, 'f', 1) + " Mm"
        : QString::number(targetKm, 'f', 0) + " km";

    p->drawText(barRect.adjusted(0, -fm.height() - 2, 0, 0), Qt::AlignCenter, label);

    p->restore();
}
