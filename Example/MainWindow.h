#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include "VirtualTableView.h"
#include "VirtualTableModel.h"
#include "SampleDataSource.h"
#include "CsvDataSource.h"

/**
 * @brief 主窗口类，用于展示虚拟表格控件的功能
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    /**
     * @brief 处理数据量变化
     * @param index 选择的索引
     */
    void onDataSizeChanged(int index);
    
    /**
     * @brief 打开CSV文件
     */
    void onOpenCsvFile();
    
    /**
     * @brief 使用示例数据
     */
    void onUseSampleData();

    /**
     * @brief 处理预加载策略变化
     * @param index 选择的索引
     */
    void onPreloadPolicyChanged(int index);

    /**
     * @brief 处理块大小变化
     * @param value 新的块大小
     */
    void onBlockSizeChanged(int value);

    /**
     * @brief 处理缓冲区大小变化
     * @param value 新的缓冲区大小
     */
    void onBufferSizeChanged(int value);

    /**
     * @brief 处理跳转到指定行
     */
    void onJumpToRow();

    /**
     * @brief 处理模型加载状态变化
     * @param status 新的加载状态
     */
    void onLoadingStatusChanged(LoadingStatus status);

    /**
     * @brief 更新状态信息
     */
    void updateStatusInfo();

private:
    // 私有方法
    /**
     * @brief 初始化UI组件
     */
    void initializeUI();

    /**
     * @brief 创建控制面板
     * @return 控制面板布局
     */
    QVBoxLayout *createControlPanel();

    /**
     * @brief 更新数据模型
     */
    void updateDataModel();

    // 私有成员变量
    VirtualTableView *m_tableView;         // 虚拟表格视图
    VirtualTableModel *m_tableModel;       // 虚拟表格模型
    std::shared_ptr<DataSource> m_dataSource; // 数据源（基类指针，可指向SampleDataSource或CsvDataSource）
    QString m_csvFilePath;                 // CSV文件路径
    bool m_useSampleData;                  // 是否使用示例数据（true）或CSV数据（false）

    // 控制组件
    QComboBox *m_dataSizeComboBox;         // 数据量选择下拉框
    QComboBox *m_preloadPolicyComboBox;    // 预加载策略选择下拉框
    QSpinBox *m_blockSizeSpinBox;          // 块大小输入框
    QSpinBox *m_bufferSizeSpinBox;         // 缓冲区大小输入框
    QSpinBox *m_jumpToRowSpinBox;          // 跳转行号输入框
    QPushButton *m_jumpButton;             // 跳转按钮
    QProgressBar *m_loadingProgressBar;    // 加载进度条
    QLabel *m_statusLabel;                 // 状态标签
    QLabel *m_visibleRangeLabel;           // 可见范围标签

    QTimer m_statusUpdateTimer;            // 状态更新定时器
    int m_currentDataSize;                 // 当前数据量
    int m_columnCount;                     // 列数
};

#endif // MAINWINDOW_H