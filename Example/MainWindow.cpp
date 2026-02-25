#include "MainWindow.h"
#include <QApplication>
#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QThread>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_currentDataSize(1000000)
    , // 默认100万条数据
    m_columnCount(8)
    , // 默认8列
    m_useSampleData(true) // 默认使用示例数据
{
    // 设置窗口标题和大小
    setWindowTitle("虚拟表格控件 - 千万级数据演示");
    resize(1200, 800);

    // 初始化UI
    initializeUI();

    // 创建数据模型
    updateDataModel();

    // 启动状态更新定时器
    m_statusUpdateTimer.setInterval(1000); // 每秒更新一次
    connect(&m_statusUpdateTimer, &QTimer::timeout, this, &MainWindow::updateStatusInfo);
    m_statusUpdateTimer.start();

    // 更新初始状态信息
    updateStatusInfo();
}

MainWindow::~MainWindow()
{
    m_statusUpdateTimer.stop();
}

void MainWindow::onDataSizeChanged(int index)
{
    // 如果选择的是CSV文件，不更新数据量
    if (!m_useSampleData) {
        return;
    }

    // 根据选择更新数据量
    switch (index) {
    case 0: // 10万条
        m_currentDataSize = 100000;
        break;
    case 1: // 100万条
        m_currentDataSize = 1000000;
        break;
    case 2: // 1000万条
        m_currentDataSize = 10000000;
        break;
    case 3: // 自定义
        bool ok;
        int customSize = QInputDialog::getInt(this, "自定义数据量",
            "请输入数据量（条）:",
            m_currentDataSize, 1000, 100000000, 1000, &ok);
        if (ok) {
            m_currentDataSize = customSize;
            m_dataSizeComboBox->setItemText(3, QString("自定义: %1条").arg(m_currentDataSize));
        }
        return;
    }

    // 更新数据模型
    updateDataModel();
}

/**
 * @brief 打开CSV文件
 */
void MainWindow::onOpenCsvFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "打开CSV文件", "", "CSV Files (*.csv);;All Files (*.*)");
    if (filePath.isEmpty()) {
        return;
    }

    // 设置CSV文件路径
    m_csvFilePath = filePath;
    m_useSampleData = false;

    // 禁用数据量选择
    m_dataSizeComboBox->setEnabled(false);

    // 更新数据模型
    updateDataModel();
}

/**
 * @brief 使用示例数据
 */
void MainWindow::onUseSampleData()
{
    m_useSampleData = true;
    m_csvFilePath.clear();

    // 启用数据量选择
    m_dataSizeComboBox->setEnabled(true);

    // 更新数据模型
    updateDataModel();
}

void MainWindow::onPreloadPolicyChanged(int index)
{
    if (!m_tableModel)
        return;

    // 根据选择更新预加载策略
    switch (index) {
    case 0:
        m_tableModel->setPreloadPolicy(PreloadPolicy::Conservative);
        break;
    case 1:
        m_tableModel->setPreloadPolicy(PreloadPolicy::Balanced);
        break;
    case 2:
        m_tableModel->setPreloadPolicy(PreloadPolicy::Aggressive);
        break;
    }
}

void MainWindow::onBlockSizeChanged(int value)
{
    if (!m_tableModel)
        return;

    // 更新块大小
    m_tableModel->setBlockSize(value);
}

void MainWindow::onBufferSizeChanged(int value)
{
    if (!m_tableView)
        return;

    // 更新缓冲区大小
    m_tableView->setBufferSize(value);
}

void MainWindow::onJumpToRow()
{
    if (!m_tableView || !m_tableModel)
        return;

    int rowIndex = m_jumpToRowSpinBox->value() - 1; // 转换为0-based索引
    if (rowIndex >= 0 && rowIndex < m_tableModel->rowCount()) {
        m_tableView->jumpToRow(rowIndex);
    } else {
        QMessageBox::warning(this, "警告", "无效的行号！");
    }
}

void MainWindow::onLoadingStatusChanged(LoadingStatus status)
{
    // 根据加载状态更新UI
    switch (status) {
    case LoadingStatus::Idle:
        m_loadingProgressBar->setVisible(false);
        m_loadingProgressBar->setValue(0);
        break;
    case LoadingStatus::LoadingVisible:
        m_loadingProgressBar->setVisible(true);
        m_loadingProgressBar->setValue(33);
        break;
    case LoadingStatus::LoadingPreload:
        m_loadingProgressBar->setVisible(true);
        m_loadingProgressBar->setValue(66);
        break;
    case LoadingStatus::LoadingAll:
        m_loadingProgressBar->setVisible(true);
        m_loadingProgressBar->setValue(100);
        break;
    }
}

void MainWindow::updateStatusInfo()
{
    if (!m_tableView || !m_tableModel)
        return;

    // 更新可见范围信息
    int startRow = m_tableView->visibleStartRow();
    int endRow = m_tableView->visibleEndRow();
    m_visibleRangeLabel->setText(QString("可见范围: 第 %1-%2 行").arg(startRow + 1).arg(endRow + 1));

    // 更新状态标签
    QString statusText;
    switch (m_tableModel->loadingStatus()) {
    case LoadingStatus::Idle:
        statusText = "空闲";
        break;
    case LoadingStatus::LoadingVisible:
        statusText = "加载可见区域";
        break;
    case LoadingStatus::LoadingPreload:
        statusText = "预加载中";
        break;
    case LoadingStatus::LoadingAll:
        statusText = "加载全部数据";
        break;
    }

    m_statusLabel->setText(QString("状态: %1 | 总数据量: %2条").arg(statusText).arg(m_tableModel->rowCount()));
}

void MainWindow::initializeUI()
{
    // 创建主布局
    QWidget* centralWidget = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    setCentralWidget(centralWidget);

    // 创建控制面板
    QVBoxLayout* controlLayout = createControlPanel();
    mainLayout->addLayout(controlLayout, 0);

    // 创建表格视图
    m_tableView = new VirtualTableView(this);
    m_tableView->setFixedRowHeight(25); // 设置固定行高
    mainLayout->addWidget(m_tableView, 1);

    // 创建状态栏
    statusBar()->addWidget(m_statusLabel);
    statusBar()->addWidget(m_visibleRangeLabel);
}

QVBoxLayout* MainWindow::createControlPanel()
{
    QVBoxLayout* layout = new QVBoxLayout();
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    // 数据量设置
    QGroupBox* dataSizeGroup = new QGroupBox("数据量");
    QVBoxLayout* dataSizeLayout = new QVBoxLayout();
    m_dataSizeComboBox = new QComboBox();
    m_dataSizeComboBox->addItem("10万条");
    m_dataSizeComboBox->addItem("100万条");
    m_dataSizeComboBox->addItem("1000万条");
    m_dataSizeComboBox->addItem("自定义");
    m_dataSizeComboBox->setCurrentIndex(1); // 默认100万条
    connect(m_dataSizeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::onDataSizeChanged);
    dataSizeLayout->addWidget(m_dataSizeComboBox);
    dataSizeGroup->setLayout(dataSizeLayout);
    layout->addWidget(dataSizeGroup);

    // 数据源设置
    QGroupBox* dataSourceGroup = new QGroupBox("数据源");
    QVBoxLayout* dataSourceLayout = new QVBoxLayout();

    // 打开CSV文件按钮
    QPushButton* openCsvButton = new QPushButton("打开CSV文件");
    connect(openCsvButton, &QPushButton::clicked, this, &MainWindow::onOpenCsvFile);
    dataSourceLayout->addWidget(openCsvButton);

    // 使用示例数据按钮
    QPushButton* useSampleButton = new QPushButton("使用示例数据");
    connect(useSampleButton, &QPushButton::clicked, this, &MainWindow::onUseSampleData);
    dataSourceLayout->addWidget(useSampleButton);

    dataSourceGroup->setLayout(dataSourceLayout);
    layout->addWidget(dataSourceGroup);

    // 性能设置
    QGroupBox* performanceGroup = new QGroupBox("性能设置");
    QVBoxLayout* performanceLayout = new QVBoxLayout();

    // 预加载策略
    QHBoxLayout* preloadLayout = new QHBoxLayout();
    preloadLayout->addWidget(new QLabel("预加载策略:"));
    m_preloadPolicyComboBox = new QComboBox();
    m_preloadPolicyComboBox->addItem("保守");
    m_preloadPolicyComboBox->addItem("平衡");
    m_preloadPolicyComboBox->addItem("激进");
    m_preloadPolicyComboBox->setCurrentIndex(1); // 默认平衡
    connect(m_preloadPolicyComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::onPreloadPolicyChanged);
    preloadLayout->addWidget(m_preloadPolicyComboBox);
    performanceLayout->addLayout(preloadLayout);

    // 块大小
    QHBoxLayout* blockSizeLayout = new QHBoxLayout();
    blockSizeLayout->addWidget(new QLabel("块大小:"));
    m_blockSizeSpinBox = new QSpinBox();
    m_blockSizeSpinBox->setRange(100, 10000);
    m_blockSizeSpinBox->setSingleStep(100);
    m_blockSizeSpinBox->setValue(1000); // 默认1000条/块
    connect(m_blockSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &MainWindow::onBlockSizeChanged);
    blockSizeLayout->addWidget(m_blockSizeSpinBox);
    performanceLayout->addLayout(blockSizeLayout);

    // 缓冲区大小
    QHBoxLayout* bufferSizeLayout = new QHBoxLayout();
    bufferSizeLayout->addWidget(new QLabel("缓冲区:"));
    m_bufferSizeSpinBox = new QSpinBox();
    m_bufferSizeSpinBox->setRange(0, 500);
    m_bufferSizeSpinBox->setSingleStep(10);
    m_bufferSizeSpinBox->setValue(50); // 默认50行
    connect(m_bufferSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &MainWindow::onBufferSizeChanged);
    bufferSizeLayout->addWidget(m_bufferSizeSpinBox);
    performanceLayout->addLayout(bufferSizeLayout);

    performanceGroup->setLayout(performanceLayout);
    layout->addWidget(performanceGroup);

    // 跳转设置
    QGroupBox* jumpGroup = new QGroupBox("快速跳转");
    QHBoxLayout* jumpLayout = new QHBoxLayout();
    jumpLayout->addWidget(new QLabel("跳转到行:"));
    m_jumpToRowSpinBox = new QSpinBox();
    m_jumpToRowSpinBox->setRange(1, 10000000);
    m_jumpToRowSpinBox->setValue(1);
    jumpLayout->addWidget(m_jumpToRowSpinBox);
    m_jumpButton = new QPushButton("跳转");
    connect(m_jumpButton, &QPushButton::clicked, this, &MainWindow::onJumpToRow);
    jumpLayout->addWidget(m_jumpButton);
    jumpGroup->setLayout(jumpLayout);
    layout->addWidget(jumpGroup);

    // 加载进度
    m_loadingProgressBar = new QProgressBar();
    m_loadingProgressBar->setRange(0, 100);
    m_loadingProgressBar->setValue(0);
    m_loadingProgressBar->setVisible(false);
    layout->addWidget(m_loadingProgressBar);

    // 状态标签
    m_statusLabel = new QLabel("状态: 初始化中...");
    m_visibleRangeLabel = new QLabel("可见范围: -");
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_visibleRangeLabel);

    // 添加拉伸空间
    layout->addStretch();

    return layout;
}

void MainWindow::updateDataModel()
{
    // 根据标志创建数据源
    if (m_useSampleData) {
        // 使用示例数据
        m_dataSource = std::make_shared<SampleDataSource>(m_currentDataSize, m_columnCount);
    } else {
        // 使用CSV文件数据
        if (m_csvFilePath.isEmpty()) {
            QMessageBox::warning(this, "警告", "请先选择CSV文件！");
            return;
        }

        auto csvDataSource = std::make_shared<CsvDataSource>(m_csvFilePath);
        if (!csvDataSource->isValid()) {
            QMessageBox::critical(this, "错误", QString("无法加载CSV文件: %1").arg(csvDataSource->errorString()));
            return;
        }

        m_dataSource = csvDataSource;
        // 更新列数和行数
        m_columnCount = csvDataSource->columnCount();
        m_currentDataSize = csvDataSource->rowCount();
    }

    // 创建新的模型
    if (m_tableModel) {
        delete m_tableModel;
    }
    m_tableModel = new VirtualTableModel;
    m_tableModel->setDataSource(m_dataSource);
    m_tableModel->setBlockSize(m_blockSizeSpinBox->value());

    // 设置预加载策略
    onPreloadPolicyChanged(m_preloadPolicyComboBox->currentIndex());

    // 连接加载状态变化信号
    connect(m_tableModel, &VirtualTableModel::loadingStatusChanged,
        this, &MainWindow::onLoadingStatusChanged);

    // 设置模型到视图
    m_tableView->setVirtualModel(m_tableModel);

    // 更新跳转行号的范围
    m_jumpToRowSpinBox->setRange(1, m_currentDataSize);
    m_jumpToRowSpinBox->setValue(1);

    // 延迟执行跳转操作，确保视图已经完全初始化
    QTimer::singleShot(20, [this]() {
        if (m_tableView && m_tableModel) {
            m_tableView->jumpToRow(0);
        }
    });

    // 更新状态信息
    updateStatusInfo();
}
