#include "viewwidget.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDebug>
#include <cmath>

ViewWidget::ViewWidget(QWidget *parent)
    : QWidget(parent),
      m_showGrid(true)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_controller.setViewportSize(size());
}

/**
 * @brief 清除所有图层
 */
void ViewWidget::clearAll()
{
    m_rasters.clear();
    m_visible.clear();
    update();
}

/**
 * @brief 添加一个新的影像图层（主线程调用）
 */
void ViewWidget::appendRaster(const RasterData &raster)
{
    m_rasters.append(raster);
    m_visible.append(true);
    update();
}

/**
 * @brief 设置图层可见性
 */
void ViewWidget::setLayerVisible(int index, bool visible)
{
    if (index < 0 || index >= m_visible.size()) return;
    m_visible[index] = visible;
    update();
}

/**
 * @brief 视图重置（回到标准地理范围）
 */
void ViewWidget::resetView()
{
    m_controller.fitWorldToViewport();
    update();
}

/**
 * @brief 显示或隐藏经纬网格
 */
void ViewWidget::setShowGrid(bool enabled)
{
    m_showGrid = enabled;
    update();
}

/**
 * @brief 绘制所有图层与网格
 */
void ViewWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    for (int i = 0; i < m_rasters.size(); ++i) {
        if (i < m_visible.size() && !m_visible[i])
            continue;
        m_renderer.renderAll(&p, m_controller, m_rasters[i], false);
    }

    if (m_showGrid) {
        QRectF frame(0, 0, width(), height());
        m_renderer.drawLatLonGrid(&p, m_controller, frame);
    }
}

/* ====================== 鼠标与缩放 ====================== */

void ViewWidget::wheelEvent(QWheelEvent *event)
{
    m_controller.handleZoom(event);
    update();
}

void ViewWidget::mousePressEvent(QMouseEvent *event)
{
    m_controller.handleMousePress(event);
}

void ViewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_controller.handleMouseMove(event)) {
        update();
        return;
    }

    // 屏幕 → 地理
    QTransform screenToWorld = m_controller.screenToWorld();
    QPointF worldPt = screenToWorld.map(QPointF(event->pos()));

    // 采样像素值（仅取第一个命中的影像）
    double value = std::numeric_limits<double>::quiet_NaN();
    for (int i = 0; i < m_rasters.size(); ++i) {
        if (i < m_visible.size() && !m_visible[i]) continue;
        const RasterData &r = m_rasters[i];
        if (r.geoExtent().contains(worldPt)) {
            value = r.sampleAtGeo(worldPt.x(), worldPt.y());
            if (std::isfinite(value)) break;
        }
    }

    emit mouseGeoPositionChanged(worldPt.x(), worldPt.y(), value);
}

void ViewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_controller.handleMouseRelease(event);
}

void ViewWidget::resizeEvent(QResizeEvent *event)
{
    m_controller.setViewportSize(event->size());
    QWidget::resizeEvent(event);
}
