#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QtConcurrent>
#include <gdal_priv.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_mapWidget = ui->viewWidget;

    // === 信号连接 ===
    connect(ui->resetView, &QPushButton::clicked,
            this, &MainWindow::on_resetView_clicked);

    connect(ui->actionShowGrid, &QCheckBox::toggled,
            this, &MainWindow::onGridToggled);
    connect(ui->comboDatasets, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDatasetChanged);
    connect(ui->fileList, &QListWidget::itemChanged,
            this, &MainWindow::onFileItemChanged);

    // ✅ 避免重复刷新同一个列表项
    connect(ui->fileList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current, QListWidgetItem *previous) {
        if (!current || current == previous)
            return;
        updateDatasetCombo(ui->fileList->row(current));
    });

    connect(m_mapWidget, &ViewWidget::mouseGeoPositionChanged,
            this, &MainWindow::onMouseInfo);
}

MainWindow::~MainWindow()
{
    // 安全清理后台任务
    for (auto *w : m_watcherToItem.keys()) {
        if (w->isRunning()) w->cancel();
        w->deleteLater();
    }
    delete ui;
}

/* ======================================================
 *  打开并加载多个 HDF 文件或文件夹（异步加载）
 * ====================================================== */
void MainWindow::on_btnReload_clicked()
{
    QString path = QFileDialog::getExistingDirectory(
        this, tr("选择 HDF 文件夹（或取消以单独选文件）"), "");

    QStringList files;
    if (!path.isEmpty()) {
        // === 文件夹模式 ===
        QDir dir(path);
        QStringList filters = {"*.hdf", "*.hdf4"};
        QFileInfoList list = dir.entryInfoList(filters, QDir::Files);
        for (const QFileInfo &fi : list)
            files << fi.absoluteFilePath();
    } else {
        // === 多选文件模式 ===
        files = QFileDialog::getOpenFileNames(
            this, tr("打开 HDF 文件"), "", tr("HDF 文件 (*.hdf *.hdf4)"));
    }

    if (files.isEmpty()) {
        QMessageBox::information(this, "提示", "未选择任何 HDF 文件。");
        return;
    }

    int addedCount = 0;
    bool firstLoad = m_rasters.isEmpty();

    for (const QString &filePath : files) {
        if (m_loadingFiles.contains(filePath))
            continue;
        if (std::any_of(m_rasters.begin(), m_rasters.end(),
                        [&](const RasterEntry &r){ return r.filePath == filePath; }))
            continue;

        // === 添加到文件列表 ===
        auto *item = new QListWidgetItem(QFileInfo(filePath).fileName() + " (加载中...)");
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        item->setCheckState(Qt::Unchecked);
        item->setToolTip(filePath);
        ui->fileList->addItem(item);

        startAsyncLoad(filePath, item);
        addedCount++;
    }

    // ✅ 仅在首次加载时清空画面
    if (firstLoad && addedCount > 0)
        m_mapWidget->clearAll();

    // 自动选中第一个新文件
    if (addedCount > 0 && ui->fileList->selectedItems().isEmpty()) {
        ui->fileList->setCurrentRow(ui->fileList->count() - addedCount);
        updateDatasetCombo(ui->fileList->currentRow());
    }

    statusBar()->showMessage(QString("已添加 %1 个文件").arg(addedCount), 3000);
}

/* ======================================================
 *  异步加载线程启动
 * ====================================================== */
void MainWindow::startAsyncLoad(const QString &filePath, QListWidgetItem *item)
{
    m_loadingFiles.insert(filePath);

    auto *watcher = new QFutureWatcher<RasterData*>(this);
    m_watcherToItem.insert(watcher, item);
    connect(watcher, &QFutureWatcher<RasterData*>::finished,
            this, &MainWindow::onLoadFinished);

    QFuture<RasterData*> future = QtConcurrent::run([filePath]() -> RasterData* {
        GDALAllRegister();
        GDALDataset *ds = static_cast<GDALDataset*>(
            GDALOpen(filePath.toUtf8().constData(), GA_ReadOnly));
        if (!ds) return nullptr;

        // 解析子数据集
        QStringList subs;
        if (char **meta = ds->GetMetadata("SUBDATASETS")) {
            for (int i = 0; meta[i]; ++i) {
                QString s(meta[i]);
                if (s.contains("_NAME="))
                    subs.append(s.section('=', 1));
            }
        }

        QString toLoad = subs.isEmpty() ? filePath : subs.first();
        GDALClose(ds);

        auto *r = new RasterData();
        if (!r->loadHDF4(toLoad)) {
            delete r;
            return nullptr;
        }
        return r;
    });

    watcher->setFuture(future);
}

/* ======================================================
 *  异步加载完成
 * ====================================================== */
void MainWindow::onLoadFinished()
{
    auto *watcher = static_cast<QFutureWatcher<RasterData*>*>(sender()); // ✅ 改这里
    if (!watcher) return;

    QListWidgetItem *item = m_watcherToItem.take(watcher);
    watcher->deleteLater();

    if (!item) return;
    QString filePath = item->toolTip();
    m_loadingFiles.remove(filePath);

    RasterData *r = watcher->result();
    if (!r) {
        item->setText(QFileInfo(filePath).fileName() + " (Failed)");
        item->setCheckState(Qt::Unchecked);
        return;
    }

    m_mapWidget->appendRaster(*r);
    delete r;

    RasterEntry entry;
    entry.filePath = filePath;
    GDALDataset *ds = static_cast<GDALDataset*>(GDALOpen(filePath.toUtf8().constData(), GA_ReadOnly));
    if (ds) {
        char **meta = ds->GetMetadata("SUBDATASETS");
        if (meta) {
            for (int i = 0; meta[i]; ++i) {
                QString s(meta[i]);
                if (s.contains("_NAME=")) entry.subdatasets.append(s.section('=', 1));
            }
        }
        GDALClose(ds);
    }
    m_rasters.append(entry);

    item->setText(QFileInfo(filePath).fileName());
    item->setCheckState(Qt::Checked);

    if (ui->fileList->selectedItems().isEmpty() || ui->fileList->currentItem() == item)
        updateDatasetCombo(ui->fileList->row(item));

    statusBar()->showMessage(QString("已加载: %1").arg(QFileInfo(filePath).fileName()), 3000);
}

/* ======================================================
 *  更新子数据集下拉框
 * ====================================================== */
void MainWindow::updateDatasetCombo(int fileIndex)
{
    ui->comboDatasets->clear();
    if (fileIndex < 0 || fileIndex >= m_rasters.size()) return;

    const auto &subs = m_rasters[fileIndex].subdatasets;
    for (const QString &s : subs)
        ui->comboDatasets->addItem(s.section(':', -1));

    if (!subs.isEmpty())
        ui->comboDatasets->setCurrentIndex(0);
}

/* ======================================================
 *  切换子数据集
 * ====================================================== */
void MainWindow::onDatasetChanged(int index)
{
    if (index < 0) return;

    m_mapWidget->clearAll();

    QString subName = ui->comboDatasets->itemText(index);
    if (subName.isEmpty()) return;

    for (const auto &entry : m_rasters) {
        if (!entry.visible || entry.subdatasets.isEmpty())
            continue;

        QString matchedPath;
        for (const QString &sub : entry.subdatasets) {
            if (sub.endsWith(subName)) {
                matchedPath = sub;
                break;
            }
        }
        if (matchedPath.isEmpty()) continue;

        auto *watcher = new QFutureWatcher<RasterData*>(this);
        connect(watcher, &QFutureWatcher<RasterData*>::finished, this,
                [this, watcher, matchedPath]() {
            std::unique_ptr<RasterData> r(watcher->result());
            watcher->deleteLater();
            if (!r) {
                QMessageBox::warning(this, "错误", "加载子数据集失败：" + matchedPath);
                return;
            }
            m_mapWidget->appendRaster(*r);
            statusBar()->showMessage("子数据集加载完成: " + matchedPath, 2000);
        });

        QFuture<RasterData*> future = QtConcurrent::run([matchedPath]() -> RasterData* {
            auto *r = new RasterData();
            if (!r->loadHDF4(matchedPath)) { delete r; return nullptr; }
            return r;
        });
        watcher->setFuture(future);
    }
}

/* ======================================================
 *  文件勾选切换（可见性控制）
 * ====================================================== */
void MainWindow::onFileItemChanged(QListWidgetItem *item)
{
    int i = ui->fileList->row(item);
    if (i < 0 || i >= m_rasters.size()) return;

    bool visible = (item->checkState() == Qt::Checked);
    m_rasters[i].visible = visible;
    m_mapWidget->setLayerVisible(i, visible);
}

/* ======================================================
 *  鼠标经纬与数值显示
 * ====================================================== */
void MainWindow::onMouseInfo(double lon, double lat, double val)
{
    QString valText = std::isfinite(val)
        ? QString("%1 °C").arg(val, 0, 'f', 2)
        : "无数据";
    statusBar()->showMessage(
        QString("经度: %1°, 纬度: %2°, 值: %3")
        .arg(lon, 0, 'f', 4)
        .arg(lat, 0, 'f', 4)
        .arg(valText));
}

/* ======================================================
 *  网格开关 & 重置视图
 * ====================================================== */
void MainWindow::onGridToggled(bool c)
{
    m_mapWidget->setShowGrid(c);
}

void MainWindow::on_resetView_clicked()
{
    if (m_mapWidget)
        m_mapWidget->resetView();
}
