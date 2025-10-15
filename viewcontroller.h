#ifndef VIEWCONTROLLER_H
#define VIEWCONTROLLER_H

#include <QTransform>
#include <QPoint>
#include <QRectF>
#include <QSize>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDebug>

class ViewController
{
public:
    ViewController();

    // === 基本设置 ===
    void setViewportSize(const QSize &size);
    QSize viewportSize() const { return m_viewportSize; }

    void reset(const QRectF &geoBounds, int viewWidth, int viewHeight);
    void fitWorldToViewport();
    void resetToRaster(const QRectF &geoExtent, const QSize &viewport, double marginFraction);

    // === 坐标变换 ===
    QTransform worldToScreen() const;       // 世界→屏幕矩阵
    QTransform screenToWorld() const;       // 屏幕→世界矩阵

    // ✅ 内联：单点转换（注意：必须写 inline）
    inline QPointF worldToScreen(const QPointF &world) const {
        return worldToScreen().map(world);
    }

    inline QPointF screenToWorld(const QPointF &screen) const {
        return screenToWorld().map(screen);
    }

    // === 鼠标操作 ===
    void handleZoom(QWheelEvent *event);
    void handleMousePress(QMouseEvent *event);
    bool handleMouseMove(QMouseEvent *event);
    void handleMouseRelease(QMouseEvent *event);

    // === 状态访问 ===
    double zoomFactor() const { return m_zoomFactor; }
    QPoint panOffset() const { return m_panOffset; }
    QRectF worldExtent() const { return m_worldExtent; }

private:
    QSize   m_viewportSize;
    QRectF  m_worldExtent;
    QPoint  m_panOffset;
    double  m_zoomFactor;
    double  m_minZoom, m_maxZoom;
    bool    m_isDragging;
    QPoint  m_lastMousePos;
};

#endif // VIEWCONTROLLER_H
