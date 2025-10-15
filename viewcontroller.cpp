#include "viewcontroller.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDebug>
#include <algorithm>
#include <cmath>

ViewController::ViewController()
    : m_worldExtent(-180, -90, 360, 180),
      m_zoomFactor(1.0),
      m_minZoom(0.25),
      m_maxZoom(200.0),
      m_isDragging(false)
{
}

void ViewController::setViewportSize(const QSize &size)
{
    m_viewportSize = size;
}

/**
 * @brief 初始化世界视图（仅在首次加载或重置时）
 */
void ViewController::fitWorldToViewport()
{
    if (m_viewportSize.isEmpty()) return;
    m_worldExtent = QRectF(-180, -90, 360, 180);
    m_zoomFactor = 1.0;
    m_panOffset = QPoint(0, 0);
}

/* ====================== 坐标变换 ====================== */

QTransform ViewController::worldToScreen() const
{
    if (m_viewportSize.isEmpty()) return QTransform();

    QPointF viewCenter(m_viewportSize.width() / 2.0, m_viewportSize.height() / 2.0);
    double baseScale = std::min(m_viewportSize.width() / m_worldExtent.width(),
                                m_viewportSize.height() / m_worldExtent.height());

    QTransform t;
    t.translate(viewCenter.x() + m_panOffset.x(),
                viewCenter.y() + m_panOffset.y());
    t.scale(m_zoomFactor * baseScale, -m_zoomFactor * baseScale);
    t.translate(-m_worldExtent.center().x(), -m_worldExtent.center().y());
    return t;
}

QTransform ViewController::screenToWorld() const
{
    bool ok = false;
    QTransform inv = worldToScreen().inverted(&ok);
    if (!ok)
        qWarning() << "[ViewController] transform inversion failed!";
    return inv;
}

/* ====================== 鼠标交互 ====================== */

void ViewController::handleZoom(QWheelEvent *event)
{
    const double step = 1.2;
    double factor = (event->angleDelta().y() > 0) ? step : 1.0 / step;
    double newZoom = std::clamp(m_zoomFactor * factor, m_minZoom, m_maxZoom);

    QPointF mousePos = event->position();
    QPointF beforeGeo = screenToWorld(mousePos);
    m_zoomFactor = newZoom;
    QPointF afterScreen = worldToScreen(beforeGeo);
    m_panOffset += (mousePos - afterScreen).toPoint();
}

void ViewController::handleMousePress(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_lastMousePos = event->pos();
    }
}

bool ViewController::handleMouseMove(QMouseEvent *event)
{
    if (m_isDragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_panOffset += delta;
        m_lastMousePos = event->pos();
        return true; // 需要刷新
    }
    return false;
}

void ViewController::handleMouseRelease(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_isDragging = false;
}
