#ifndef VIEWWIDGET_H
#define VIEWWIDGET_H

#include <QWidget>
#include "viewcontroller.h"
#include "viewrenderer.h"
#include "rasterdata.h"

/**
 * @brief 地图视图控件：支持多栅格加载、缩放、平移与取样显示
 */
class ViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ViewWidget(QWidget *parent = nullptr);

    void clearAll(); // 清空所有图层


    // 同步加载（单文件）
    bool loadHDF4(const QString &path);

    // 异步加载完毕后添加到图层（主线程调用）
    void appendRaster(const RasterData &raster);

    // 显示或隐藏经纬网格
    void setShowGrid(bool enabled);

    // 控制某图层可见性
    void setLayerVisible(int index, bool visible);

    // 视图控制
    void resetView();

signals:
    void mouseGeoPositionChanged(double lon, double lat, double value);

protected:
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    ViewController m_controller;
    ViewRenderer   m_renderer;

    QVector<RasterData> m_rasters;  // 所有加载的影像
    QVector<bool>       m_visible;  // 对应图层可见性

    bool m_showGrid = true;

    // 鼠标状态
    QPointF m_lastMouseGeo;
    QPoint  m_lastPixel;
    double  m_lastValue = std::numeric_limits<double>::quiet_NaN();
};

#endif // VIEWWIDGET_H
