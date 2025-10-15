#include "interactiveimagewidget.h"
#include "geoutils.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDebug>

InteractiveImageWidget::InteractiveImageWidget(QWidget *parent)
    : QWidget(parent),
      m_zoomFactor(1.0),
      m_isDragging(false),
      m_worldExtent(-180.0, -90.0, 360.0, 180.0)
{
    setMouseTracking(true);
}

bool InteractiveImageWidget::loadHDF4(const QString &filePath)
{
    if (!m_raster.loadHDF4(filePath)) {
        qWarning() << "InteractiveImageWidget: loadHDF4 failed";
        return false;
    }

    // 不再重置视图，由上层（ViewWidget/MainWindow）统一处理
    update();
    return true;
}


void InteractiveImageWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 白背景（你要求白色或透明底可在此修改）
    p.fillRect(rect(), Qt::white);

    if (m_raster.image().isNull())
    {
        p.setPen(Qt::gray);
        p.drawText(rect(), Qt::AlignCenter, tr("请加载 HDF4 数据"));
        return;
    }

    p.save();

    // world baseline (global) -> used to compute base scale
    QRectF worldRect = m_worldExtent;

    // 视图原点到中心，加上交互平移
    QPointF viewCenter(width() / 2.0, height() / 2.0);
    p.translate(viewCenter + QPointF(m_panOffset));

    // 缩放（m_zoomFactor），并 Y 翻转使地理 Y 越大越向上
    p.scale(m_zoomFactor, -m_zoomFactor);

    // 把屏幕单位变换到 世界单位（使用一个 baseScale 保证经纬比）
    double baseScale = std::min(width() / worldRect.width(), height() / worldRect.height());
    p.scale(baseScale, baseScale);

    // 将原点移到世界中心以便用经纬直接绘制
    p.translate(-worldRect.center().x(), -worldRect.center().y());

    // 绘制 raster（使用其 geoExtent）
    QRectF imgGeo = m_raster.geoExtent(); // QRectF(minX,minY,maxX-minX,maxY-minY)
    if (imgGeo.isValid()) {
        // 输入的 geoExtent 用 (left=minX, top=minY?) 我们需要绘制为 QRectF(left, top, width, height) 但 y方向在绘制中是向上
        // 在这里使用 (left, bottom) -> 转换为 QRectF(left, topY) with topY = imgGeo.top()? to be safe, construct with left,minY,width,height and rely on the global transform
        QRectF imgRect(imgGeo.left(), imgGeo.top(), imgGeo.width(), imgGeo.height());
        p.drawImage(imgRect, m_raster.image());
    } else {
        // fallback: center draw
        QImage img = m_raster.image();
        QRectF target(-img.width()/2.0, -img.height()/2.0, img.width(), img.height());
        p.drawImage(target, img);
    }

    // 动态经纬网（线条随当前变换绘制在经纬坐标上）
    // 计算合适步长（度）
    double degreesPerPixel = 1.0 / (m_zoomFactor * baseScale);
    double approxStep = std::pow(10.0, std::floor(std::log10(degreesPerPixel * 120.0)));
    if (degreesPerPixel * 300.0 < approxStep) approxStep /= 2.0;
    if (degreesPerPixel * 30.0  > approxStep) approxStep *= 2.0;
    double step = approxStep;
    if (step <= 0) step = 0.1;

    p.setPen(QPen(QColor(220,220,220), 0));
    for (double lon = -180.0; lon <= 180.0; lon += step)
        p.drawLine(QPointF(lon, -90), QPointF(lon, 90));
    for (double lat = -90.0; lat <= 90.0; lat += step)
        p.drawLine(QPointF(-180, lat), QPointF(180, lat));

    // 小坐标标注 (在经纬网格内绘制)
    p.setPen(QPen(Qt::darkGray, 0));
    QFont f = font();
    f.setPointSizeF(std::max(7.0, 9.0 / std::max(1.0, m_zoomFactor)));
    p.setFont(f);
    for (double lon = -180.0; lon <= 180.0; lon += step)
        p.drawText(QPointF(lon + step*0.1, -89.0 + step*0.05), QString::number(lon, 'f', (step < 1.0) ? 1 : 0) + QChar(0xB0));
    for (double lat = -90.0; lat <= 90.0; lat += step)
        p.drawText(QPointF(-179.5 + step*0.05, lat + step*0.05), QString::number(lat, 'f', (step < 1.0) ? 1 : 0) + QChar(0xB0));

    p.restore();

    // 边缘坐标轴（固定在窗口边缘，始终可见）
    p.setPen(QPen(Qt::black, 1));
    QFont labFont = font();
    labFont.setPointSize(9);
    p.setFont(labFont);

    // compute current geo bounds from canvas
    QPointF topLeftGeo = mapCanvasToGeo(QPoint(0,0));
    QPointF bottomRightGeo = mapCanvasToGeo(QPoint(width(), height()));
    double lonMin = std::min(topLeftGeo.x(), bottomRightGeo.x());
    double lonMax = std::max(topLeftGeo.x(), bottomRightGeo.x());
    double latMin = std::min(bottomRightGeo.y(), topLeftGeo.y());
    double latMax = std::max(bottomRightGeo.y(), topLeftGeo.y());

    // 动态刻度：分成 ~5 段
    double lonRange = std::max(1e-6, lonMax - lonMin);
    double latRange = std::max(1e-6, latMax - latMin);
    double lonStepEdge = std::pow(10.0, std::floor(std::log10(lonRange / 5.0)));
    double latStepEdge = std::pow(10.0, std::floor(std::log10(latRange / 5.0)));
    if ((lonRange / lonStepEdge) < 3) lonStepEdge /= 2.0;
    if ((latRange / latStepEdge) < 3) latStepEdge /= 2.0;

    const int tickLen = 6;
    const int labelOffset = 2;

    // bottom ticks (longitude)
    for (double lon = std::ceil(lonMin / lonStepEdge) * lonStepEdge; lon <= lonMax + 1e-9; lon += lonStepEdge) {
        double x = (lon - lonMin) / (lonMax - lonMin) * width();
        p.drawLine(QPointF(x, height()), QPointF(x, height() - tickLen));
        QString label = QString::number(lon, 'f', (lonStepEdge < 1.0) ? 1 : 0) + QChar(0xB0);
        p.drawText(QRectF(x - 30, height() - tickLen - 15, 60, 15), Qt::AlignCenter | Qt::AlignTop, label);
    }

    // left ticks (latitude)
    for (double lat = std::ceil(latMin / latStepEdge) * latStepEdge; lat <= latMax + 1e-9; lat += latStepEdge) {
        double y = height() - (lat - latMin) / (latMax - latMin) * height();
        p.drawLine(QPointF(0, y), QPointF(tickLen, y));
        QString label = QString::number(lat, 'f', (latStepEdge < 1.0) ? 1 : 0) + QChar(0xB0);
        p.drawText(QRectF(tickLen + labelOffset, y - 8, 60, 16), Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    // 固定外框
    p.setPen(QPen(Qt::gray, 1));
    p.drawRect(rect().adjusted(0,0,-1,-1));

    // 比例尺
    drawScaleBar(&p);
}

void InteractiveImageWidget::drawScaleBar(QPainter *p)
{
    if (width() < 50 || height() < 50) return;

    // 使用当前视图经度跨度计算每像素对应度，再换算成 km（取中心纬度）
    QPointF topLeftGeo = mapCanvasToGeo(QPoint(0,0));
    QPointF bottomRightGeo = mapCanvasToGeo(QPoint(width(), height()));
    double lonSpan = std::abs(bottomRightGeo.x() - topLeftGeo.x());
    double centerLat = (topLeftGeo.y() + bottomRightGeo.y()) / 2.0;

    double degPerPixel = lonSpan / width();
    double kmPerPixel = degPerPixel * GeoUtils::kmPerDegreeLonAtLat(centerLat);

    // 目标显示长度：屏幕 1/5
    double targetPx = width() / 5.0;
    double rawKm = kmPerPixel * targetPx;
    if (rawKm <= 0) return;

    // 选取舒适刻度（最小单位 1 km）
    double chosenKm = GeoUtils::niceKmStep(rawKm);
    if (chosenKm < 1.0) chosenKm = 1.0;

    int barWidthPx = static_cast<int>(chosenKm / kmPerPixel);
    if (barWidthPx <= 0) return;

    // 单位选择 km 或 1000km (Mm)
    QString unit = "km";
    double displayVal = chosenKm;
    if (chosenKm >= 1000.0) { displayVal = chosenKm / 1000.0; unit = "1000km"; }

    QString text = QString("%1 %2").arg(displayVal, 0, 'f', (displayVal < 10.0) ? 1 : 0).arg(unit);

    int margin = 20;
    int barH = 8;
    int x = width() - barWidthPx - margin;
    int y = height() - margin;

    // 黑白相间
    p->setPen(Qt::black);
    p->setBrush(Qt::black);
    p->drawRect(x, y - barH, barWidthPx / 2, barH);
    p->setBrush(Qt::white);
    p->drawRect(x + barWidthPx/2, y - barH, barWidthPx - barWidthPx/2, barH);

    // 边框
    p->setBrush(Qt::NoBrush);
    p->drawRect(x, y - barH, barWidthPx, barH);

    // 文本
    QFont f = p->font();
    f.setPointSize(9);
    p->setFont(f);
    p->drawText(QRectF(x - 10, y - barH - 20, barWidthPx + 20, 18), Qt::AlignCenter, text);
}

void InteractiveImageWidget::wheelEvent(QWheelEvent *event)
{
    if (m_raster.image().isNull()) { event->ignore(); return; }

    const double zoomStep = 1.2;
    double oldZoom = m_zoomFactor;
    double factor = (event->angleDelta().y() > 0) ? zoomStep : (1.0 / zoomStep);
    double newZoom = std::clamp(oldZoom * factor, 0.05, 120.0);

    if (qFuzzyCompare(oldZoom, newZoom)) {
        event->accept();
        return;
    }

    QPointF mousePos = event->position();
    QPointF center(width() / 2.0, height() / 2.0);
    QPointF panF = QPointF(m_panOffset);
    QPointF beforeImage = (mousePos - center - panF) / oldZoom;
    QPointF newPanF = mousePos - center - beforeImage * newZoom;

    m_zoomFactor = newZoom;
    m_panOffset = newPanF.toPoint();

    update();
    event->accept();
}

void InteractiveImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_lastMousePos = event->pos();
    }
}

void InteractiveImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_panOffset += delta;
        m_lastMousePos = event->pos();
        update();
    } else {
        // 发出鼠标经纬与值
        QPointF geo = mapCanvasToGeo(event->pos());
        QRectF imgGeo = m_raster.geoExtent();
        if (m_raster.image().isNull() == false && imgGeo.isValid()) {
            // 计算像素索引（基于 raster 的 geoExtent）
            double minX = imgGeo.left();
            double minY = imgGeo.top(); // depending on how geoExtent stored; here we use left/minY
            double w = imgGeo.width();
            double h = imgGeo.height();

            // convert lon/lat -> pixel
            double px_d = (geo.x() - imgGeo.left()) / imgGeo.width() * m_raster.width();
            double py_d = (imgGeo.bottom() - geo.y()) / imgGeo.height() * m_raster.height(); // bottom minus lat
            int px = static_cast<int>(std::floor(px_d));
            int py = static_cast<int>(std::floor(py_d));
            double val = std::numeric_limits<double>::quiet_NaN();
            if (px >= 0 && px < m_raster.width() && py >=0 && py < m_raster.height())
                val = m_raster.valueAt(px, py);
            emit mouseGeoPositionChanged(geo.x(), geo.y(), val);
        } else {
            emit mouseGeoPositionChanged(geo.x(), geo.y(), std::numeric_limits<double>::quiet_NaN());
        }
    }
}

void InteractiveImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) m_isDragging = false;
}

void InteractiveImageWidget::resetView()
{
    if (m_raster.image().isNull()) return;

    // 以世界范围为参考，基于 raster extent 将初始缩放设为局部放大（使影像在窗口居中并能放大查看）
    QRectF worldRect = m_worldExtent;
    QRectF imgGeo = m_raster.geoExtent();
    if (!imgGeo.isValid()) {
        m_zoomFactor = 1.0;
        m_panOffset = QPoint(0,0);
        update();
        return;
    }

    // baseScale: 屏幕像素 / 度
    double baseScale = std::min(width() / worldRect.width(), height() / worldRect.height());

    // 初始缩放：使影像在窗口中偏大显示（允许局部查看）
    // 参考：当影像宽度占屏幕 1/4 时，放大至使其宽度占屏幕 0.6
    double imgDegW = imgGeo.width();
    double desiredScreenFraction = 0.6;
    double scaleToFit = (width() * desiredScreenFraction) / (imgDegW * baseScale);
    if (scaleToFit <= 0) scaleToFit = 1.0;
    m_zoomFactor = scaleToFit;

    // 平移：把影像中心移动到屏幕中心
    QPointF worldCenter = worldRect.center();
    QPointF imgCenter = imgGeo.center();
    // in world units multiplied by baseScale and m_zoomFactor -> pixel offset
    double dx = (imgCenter.x() - worldCenter.x()) * baseScale * m_zoomFactor;
    double dy = -(imgCenter.y() - worldCenter.y()) * baseScale * m_zoomFactor;
    m_panOffset = QPoint(static_cast<int>(dx), static_cast<int>(dy));

    update();
}

void InteractiveImageWidget::resizeEvent(QResizeEvent *)
{
    // 当窗口大小改变，重新计算 resetView 会改变初始视图；但是保留当前交互状态更友好。
    // 这里选择不自动 resetView（避免每次调整窗口都回到初始位置）
    // 如果想在 resize 时自动调整，请启用下面一行：
    // resetView();
    update();
}

QPointF InteractiveImageWidget::mapCanvasToGeo(const QPoint &p) const
{
    // 将窗口坐标 -> 以 worldExtent 为坐标系的经纬度
    QRectF world = m_worldExtent;
    // 将屏幕点转到 屏幕中心坐标系，并撤销 pan 与 zoom 与 baseScale
    QPointF center(width()/2.0, height()/2.0);
    QPointF pt = QPointF(p) - center - QPointF(m_panOffset);
    // 先除以 zoomFactor（屏幕缩放），再除以 baseScale（世界度->屏幕像素）
    double baseScale = std::min(width() / world.width(), height() / world.height());
    QPointF worldPt = pt / m_zoomFactor / baseScale;
    // world origin currently at world center due to earlier translate(-world.center())
    QPointF lonlat = world.center() + worldPt;
    // lonlat is (lon, lat)
    return lonlat;
}
