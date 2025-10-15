#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QFutureWatcher>
#include <QHash>
#include <QSet>

#include "viewwidget.h"
#include "rasterdata.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief 主窗口类
 *
 * 负责：
 * - 文件与文件夹选择、异步加载；
 * - 控制图层可见性；
 * - 子数据集切换；
 * - 与 ViewWidget 交互（坐标提示、重置视图、网格开关）。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    /* === 用户交互 === */
    void on_btnReload_clicked();                     // 打开文件/文件夹（异步加载）
    void on_resetView_clicked();                     // 重置视图
    void onGridToggled(bool checked);                // 网格显示开关
    void onDatasetChanged(int index);                // 切换子数据集
    void onFileItemChanged(QListWidgetItem *item);   // 文件可见性变化
    void onMouseInfo(double lon, double lat, double value); // 鼠标经纬与数值显示

    /* === 异步加载回调 === */
    void onLoadFinished();

private:
    /* === 成员 === */
    Ui::MainWindow *ui = nullptr;
    ViewWidget *m_mapWidget = nullptr;

    struct RasterEntry {
        QString filePath;         // 文件路径
        QStringList subdatasets;  // 子数据集名称列表
        bool visible = true;      // 是否显示
    };

    QList<RasterEntry> m_rasters;   // 已加载的栅格列表
    QHash<QFutureWatcher<RasterData*>*, QListWidgetItem*> m_watcherToItem; // 异步加载项对应表
    QSet<QString> m_loadingFiles;   // 当前正在加载的文件路径集合（防重复）

    /* === 辅助方法 === */
    void startAsyncLoad(const QString &filePath, QListWidgetItem *item);  // 启动异步加载
    void updateDatasetCombo(int fileIndex);                               // 更新子数据集下拉框
};

#endif // MAINWINDOW_H
