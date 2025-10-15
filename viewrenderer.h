#ifndef VIEWRENDERER_H
#define VIEWRENDERER_H

#include <QPainter>
#include "viewcontroller.h"
#include "rasterdata.h"

/**
 * @brief ViewRenderer —— 负责绘制地图、格网、刻度、色标等图层
 */
class ViewRenderer
{
public:
    ViewRenderer() = default;

    /**
     * @brief 渲染整幅地图（影像 + 网格 + 坐标轴 + 刻度）
     */
    void renderAll(QPainter *p,
                   const ViewController &controller,
                   const RasterData &raster,
                   bool showGrid);

    /**
     * @brief 绘制经纬网格（井字格 + 外框 + 刻度）
     */
    void drawLatLonGrid(QPainter *p,
                                 const ViewController &controller,
                                 const QRectF &frame);

    /**
     * @brief 绘制左下角鼠标信息（可选）
     */
    void drawMouseInfo(QPainter *p,
                       const QPointF &geo,
                       const QPoint &pixel,
                       double value);
    /**
     * @brief （可选）刻度尺
     */
    void drawScaleBar(QPainter *p, const ViewController &controller, const QRectF &frame);

private:
    /**
     * @brief （可选）绘制色标条（影像值范围）
     */
    void drawColorBar(QPainter *p,
                      const QRectF &frame,
                      const RasterData &raster);
};



#endif // VIEWRENDERER_H
