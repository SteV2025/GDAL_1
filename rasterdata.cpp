#include "rasterdata.h"
#include "geoutils.h"
#include <gdalwarper.h>
#include <cpl_conv.h>
#include <QDebug>
#include <cmath>
#include <limits>
#include <algorithm>

static bool g_gdalInitialized = false;

// 可选：固定温度范围（°C）
static constexpr double FIXED_TEMP_MIN = -30.0;
static constexpr double FIXED_TEMP_MAX = 50.0;

RasterData::RasterData() {
    if (!g_gdalInitialized) {
        GDALAllRegister();
        g_gdalInitialized = true;
    }
}

RasterData::~RasterData() {}

/**
 * @brief 加载 HDF4/GDAL 栅格文件（自动重投影到 WGS84）
 * 支持 MODIS LST 数据，生成伪彩色温度图。
 */
bool RasterData::loadHDF4(const QString &filePath)
{
    GDALDataset *dataset = static_cast<GDALDataset *>(
        GDALOpen(filePath.toUtf8().constData(), GA_ReadOnly));
    if (!dataset) {
        qWarning() << "RasterData: cannot open dataset:" << CPLGetLastErrorMsg();
        return false;
    }

    const char *projRef = dataset->GetProjectionRef();
    GDALDataset *warpDataset = dataset;

    // === 自动重投影到 WGS84 ===
    if (projRef && strlen(projRef) > 0 && !strstr(projRef, "4326")) {
        GDALDataset *tmp = (GDALDataset *)GDALAutoCreateWarpedVRT(
            dataset, projRef, "EPSG:4326", GRA_NearestNeighbour, 0.0, nullptr);
        if (tmp) warpDataset = tmp;
        else qWarning() << "RasterData: warp failed, fallback to original dataset";
    }

    // === 读取首波段 ===
    GDALRasterBand *band = warpDataset->GetRasterBand(1);
    if (!band) {
        qWarning() << "RasterData: cannot get raster band.";
        if (warpDataset != dataset) GDALClose(warpDataset);
        GDALClose(dataset);
        return false;
    }

    m_width = warpDataset->GetRasterXSize();
    m_height = warpDataset->GetRasterYSize();
    m_data.resize(m_width * m_height);

    if (band->RasterIO(GF_Read, 0, 0, m_width, m_height,
                       m_data.data(), m_width, m_height, GDT_Float32, 0, 0) != CE_None) {
        qWarning() << "RasterData: RasterIO read failed";
        if (warpDataset != dataset) GDALClose(warpDataset);
        GDALClose(dataset);
        return false;
    }

    // === GeoTransform ===
    double gt[6];
    if (warpDataset->GetGeoTransform(gt) == CE_None) {
        std::copy(std::begin(gt), std::end(gt), m_geoTransform);
        m_hasGeoTransform = true;

        double left   = gt[0];
        double right  = gt[0] + gt[1] * m_width;
        double top    = gt[3];
        double bottom = gt[3] + gt[5] * m_height;
        m_geoExtent   = QRectF(QPointF(left, bottom), QPointF(right, top));
    }

    // === 数据转换与范围计算 ===
    m_minVal = std::numeric_limits<double>::max();
    m_maxVal = std::numeric_limits<double>::lowest();

    const bool isLST = filePath.contains("LST_Day_1km") || filePath.contains("LST_Night_1km");
    int validCount = 0;

    for (float &v : m_data) {
        if (isLST) {
            if (v == 0 || v > 65535) { v = NAN; continue; }
            double vv = v * 0.02 - 273.15;
            if (vv < -80 || vv > 80) { v = NAN; continue; }
            v = vv;
        } else {
            if (!std::isfinite(v) || std::abs(v) > 1e6) { v = NAN; continue; }
        }

        m_minVal = std::min(m_minVal, (double)v);
        m_maxVal = std::max(m_maxVal, (double)v);
        validCount++;
    }

    qDebug() << "[RasterData] Valid pixels:" << validCount
             << "Raw range:" << m_minVal << "→" << m_maxVal;

    // === 固定温度范围（提升不同图层一致性）===
    if (isLST) {
        m_minVal = FIXED_TEMP_MIN;
        m_maxVal = FIXED_TEMP_MAX;
        qDebug() << "[RasterData] Fixed LST range:" << m_minVal << "→" << m_maxVal;
    }

    buildImageFromData();

    if (warpDataset != dataset) GDALClose(warpDataset);
    GDALClose(dataset);
    return true;
}

/**
 * @brief 根据浮点数据生成伪彩图像
 */
void RasterData::buildImageFromData()
{
    if (m_width <= 0 || m_height <= 0) {
        m_image = QImage();
        return;
    }

    m_image = QImage(m_width, m_height, QImage::Format_ARGB32);
    for (int y = 0; y < m_height; ++y) {
        QRgb *scan = reinterpret_cast<QRgb *>(m_image.scanLine(m_height - 1 - y));
        const float *src = m_data.constData() + y * m_width;
        for (int x = 0; x < m_width; ++x) {
            float v = src[x];
            scan[x] = std::isnan(v)
                ? qRgba(0, 0, 0, 0)
                : ColorMap::mapValue(v, m_minVal, m_maxVal, ColorMap::Heat);
        }
    }
}

/**
 * @brief 获取像素值
 */
double RasterData::valueAt(int x, int y) const
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return std::numeric_limits<double>::quiet_NaN();

    float v = m_data[y * m_width + x];
    return std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
}

/**
 * @brief 按经纬度采样（最近邻）
 */
double RasterData::sampleAtGeo(double lon, double lat) const
{
    if (m_data.isEmpty() || !m_geoExtent.isValid())
        return std::numeric_limits<double>::quiet_NaN();

    int px = -1, py = -1;
    if (m_hasGeoTransform && std::abs(m_geoTransform[2]) < 1e-10 && std::abs(m_geoTransform[4]) < 1e-10) {
        px = (lon - m_geoTransform[0]) / m_geoTransform[1];
        py = (lat - m_geoTransform[3]) / m_geoTransform[5];
    } else {
        double fx = (lon - m_geoExtent.left()) / m_geoExtent.width();
        double fy = (m_geoExtent.top() - lat) / m_geoExtent.height();
        px = static_cast<int>(fx * (m_width - 1));
        py = static_cast<int>(fy * (m_height - 1));
    }

    if (px < 0 || px >= m_width || py < 0 || py >= m_height)
        return std::numeric_limits<double>::quiet_NaN();

    float v = m_data[py * m_width + px];
    return std::isfinite(v) ? v : std::numeric_limits<double>::quiet_NaN();
}
