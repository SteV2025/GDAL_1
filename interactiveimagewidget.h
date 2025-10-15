#ifndef INTERACTIVEIMAGEWIDGET_H
#define INTERACTIVEIMAGEWIDGET_H

#include <QWidget>
#include <QImage>
#include <QRectF>
#include <QPoint>
#include <QPointF>
#include "rasterdata.h"

class InteractiveImageWidget : public QWidget
{
    Q_OBJECT
public:
    explicit InteractiveImageWidget(QWidget *parent = nullptr);
    ~InteractiveImageWidget() override {}

    // API 保持不变：加载子数据集路径
    bool loadHDF4(const QString &filePath);

signals:
    void mouseGeoPositionChanged(double lon, double lat, double value);

public slots:
    void resetView();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // 将窗口坐标 -> 地理坐标（lon, lat）
    QPointF mapCanvasToGeo(const QPoint &p) const;

    // 比例尺绘制
    void drawScaleBar(QPainter *p);

private:
    RasterData m_raster;

    // 交互状态
    double m_zoomFactor;
    QPoint m_panOffset;
    bool m_isDragging;
    QPoint m_lastMousePos;

    // 全局参考世界范围（用于保持经纬参考）
    QRectF m_worldExtent;
};

#endif // INTERACTIVEIMAGEWIDGET_H
