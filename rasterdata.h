#ifndef RASTERDATA_H
#define RASTERDATA_H

#include <QString>
#include <QImage>
#include <QRectF>
#include <QPointF>
#include <QVector>
#include <QColor>
#include <gdal_priv.h>

/**
 * @class RasterData
 * @brief 封装单波段 GDAL 栅格（如 HDF4、GeoTIFF 等）
 *
 * 主要职责：
 *  1. 从文件加载（带自动重投影到 EPSG:4326）
 *  2. 保存地理范围、像素值数组、QImage 伪彩渲染图
 *  3. 提供 valueAt() 与 sampleAtGeo() 等采样接口
 */
class RasterData
{
public:
    RasterData();
    ~RasterData();

    /** 加载栅格文件（支持 HDF4/GDAL），返回是否成功 */
    bool loadHDF4(const QString &filePath);

    /** 获取地理范围（经纬度矩形） */
    const QRectF &geoExtent() const { return m_geoExtent; }

    /** 获取伪彩影像（供绘制使用） */
    const QImage &image() const { return m_image; }

    /** 获取最小/最大值（用于色带或图例） */
    double minValue() const { return m_minVal; }
    double maxValue() const { return m_maxVal; }

    /** 获取像素值（索引坐标） */
    double valueAt(int x, int y) const;

    /** 地理坐标采样（经纬度） */
    double sampleAtGeo(double lon, double lat) const;

    /** 获取尺寸 */
    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    /** 构建伪彩图 */
    void buildImageFromData();

private:
    QVector<float> m_data;      ///< 实际数值数组
    QImage m_image;             ///< 渲染图像
    QRectF m_geoExtent;         ///< 经纬度范围（左下角→右上角）

    int m_width = 0;
    int m_height = 0;
    double m_minVal = 0;
    double m_maxVal = 0;

    // === GeoTransform 与标志 ===
    double m_geoTransform[6] = {0, 1, 0, 0, 0, -1};
    bool   m_hasGeoTransform = false;
};

#endif // RASTERDATA_H
