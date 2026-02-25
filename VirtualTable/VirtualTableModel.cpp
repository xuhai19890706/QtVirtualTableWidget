#include "VirtualTableModel.h"
#include <QElapsedTimer>
#include <QThreadPool>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

VirtualTableModel::VirtualTableModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_blockSize(1000)
    , m_preloadPolicy(PreloadPolicy::Balanced)
    , m_loadingStatus(LoadingStatus::Idle)
    , m_visibleStartRow(0)
    , m_visibleEndRow(0)
    , m_scrollSpeed(0.0)
    , m_preloadBlocksAhead(2)
    , m_preloadBlocksBehind(1)
{
    // 根据预加载策略初始化预加载块数
    updatePreloadBlockCounts();
}

VirtualTableModel::~VirtualTableModel()
{
    // 取消所有正在进行的加载任务
    for (auto it = m_loadTasks.begin(); it != m_loadTasks.end(); ++it) {
        if (it.value() && it.value()->isRunning()) {
            it.value()->cancel();
            delete it.value();
        }
    }
    m_loadTasks.clear();
}

int VirtualTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !m_dataSource)
        return 0;
    return m_dataSource->rowCount();
}

int VirtualTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !m_dataSource)
        return 0;
    return m_dataSource->columnCount();
}

QVariant VirtualTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || !m_dataSource)
        return QVariant();

    int row = index.row();
    int col = index.column();

    if (row < 0 || row >= m_dataSource->rowCount() || col < 0 || col >= m_dataSource->columnCount())
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        // 获取数据所在的块
        int blockIndex = getBlockIndex(row);
        int rowInBlock = row % m_blockSize;

        // 检查块是否已加载
        {
            QMutexLocker locker(&m_dataMutex);
            auto it = m_dataBlocks.find(blockIndex);
            if (it != m_dataBlocks.end() && it.value().isValid) {
                // 更新最后访问时间（使用const_cast允许在const方法中修改mutable成员）
                DataBlock& block = const_cast<DataBlock&>(it.value());
                block.lastAccessTime = QDateTime::currentMSecsSinceEpoch();

                // 返回数据
                if (rowInBlock < block.data.size()) {
                    const QList<QVariant>& rowData = block.data[rowInBlock];
                    if (col < rowData.size()) {
                        return rowData[col];
                    }
                }
            }
        }

        // 如果块未加载，触发加载并返回占位符
        const_cast<VirtualTableModel*>(this)->loadBlock(blockIndex, true);
        return QString("......");
    }

    return QVariant();
}

QVariant VirtualTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (!m_dataSource)
        return QVariant();

    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            // 列标题
            QList<QString> headers = m_dataSource->headerData();
            if (section >= 0 && section < headers.size()) {
                return headers[section];
            }
            return QString("Column %1").arg(section + 1);
        } else {
            // 行标题（显示行号）
            return QString("%1").arg(section + 1);
        }
    }

    return QVariant();
}

void VirtualTableModel::setDataSource(std::shared_ptr<DataSource> source)
{
    beginResetModel();
    m_dataSource = source;
    m_dataBlocks.clear();
    m_loadTasks.clear();
    endResetModel();

    emit loadingStatusChanged(LoadingStatus::Idle);
}

void VirtualTableModel::setBlockSize(int blockSize)
{
    if (blockSize <= 0)
        return;

    if (blockSize != m_blockSize) {
        beginResetModel();
        m_blockSize = blockSize;
        m_dataBlocks.clear();
        m_loadTasks.clear();
        endResetModel();
    }
}

void VirtualTableModel::setPreloadPolicy(PreloadPolicy policy)
{
    if (policy != m_preloadPolicy) {
        m_preloadPolicy = policy;
        updatePreloadBlockCounts();

        // 如果有可见区域，重新预加载
        if (m_visibleStartRow != m_visibleEndRow) {
            int centerRow = (m_visibleStartRow + m_visibleEndRow) / 2;
            int centerBlock = getBlockIndex(centerRow);
            preloadBlocks(centerBlock);
        }
    }
}

void VirtualTableModel::jumpToRow(int rowIndex)
{
    if (!m_dataSource || rowIndex < 0 || rowIndex >= m_dataSource->rowCount())
        return;

    // 设置可见区域为目标行附近
    int visibleRows = m_visibleEndRow - m_visibleStartRow + 1;
    if (visibleRows <= 0)
        visibleRows = 50; // 默认可见50行

    int newStartRow = std::max(0, rowIndex - visibleRows / 2);
    int newEndRow = std::min(m_dataSource->rowCount() - 1, newStartRow + visibleRows - 1);

    setVisibleRange(newStartRow, newEndRow);
}

LoadingStatus VirtualTableModel::loadingStatus() const
{
    return m_loadingStatus;
}

void VirtualTableModel::setVisibleRange(int startRow, int endRow)
{
    if (!m_dataSource)
        return;

    // 确保范围有效
    startRow = std::max(0, startRow);
    endRow = std::min(m_dataSource->rowCount() - 1, endRow);

    if (startRow > endRow)
        return;

    m_visibleStartRow = startRow;
    m_visibleEndRow = endRow;

    // 加载可见区域的数据块
    int startBlock = getBlockIndex(startRow);
    int endBlock = getBlockIndex(endRow);

    // 更新加载状态
    if (startBlock <= endBlock) {
        setLoadingStatus(LoadingStatus::LoadingVisible);
    }

    // 加载可见区域的块
    for (int blockIndex = startBlock; blockIndex <= endBlock; ++blockIndex) {
        loadBlock(blockIndex, true);
    }

    // 预加载周围的块
    int centerBlock = (startBlock + endBlock) / 2;
    preloadBlocks(centerBlock);

    // 清理不需要的块
    cleanupBlocks();

    // 如果所有可见块都已加载，更新状态
    bool allVisibleLoaded = true;
    for (int blockIndex = startBlock; blockIndex <= endBlock; ++blockIndex) {
        QMutexLocker locker(&m_dataMutex);
        auto it = m_dataBlocks.find(blockIndex);
        if (it == m_dataBlocks.end() || !it.value().isValid) {
            allVisibleLoaded = false;
            break;
        }
    }

    if (allVisibleLoaded) {
        setLoadingStatus(LoadingStatus::Idle);
    }
}

void VirtualTableModel::setScrollSpeed(double speed)
{
    m_scrollSpeed = speed;

    // 根据滚动速度动态调整预加载策略
    if (speed > 5000.0) {
        // 快速滚动，减少预加载
        m_preloadBlocksAhead = std::max(1, m_preloadBlocksAhead / 2);
        m_preloadBlocksBehind = std::max(0, m_preloadBlocksBehind / 2);
    } else if (speed < 500.0 && speed > 0) {
        // 慢速滚动，恢复正常预加载
        updatePreloadBlockCounts();
    }
}

void VirtualTableModel::onBlockLoaded(int blockIndex, const QList<QList<QVariant>>& data)
{
    if (!m_dataSource)
        return;

    QMutexLocker locker(&m_dataMutex);

    // 更新数据块
    DataBlock& block = getBlock(blockIndex);
    block.data = data;
    block.isValid = true;
    block.lastAccessTime = QDateTime::currentMSecsSinceEpoch();

    // 计算受影响的行范围
    int startRow = blockIndex * m_blockSize;
    int endRow = std::min(startRow + data.size() - 1, m_dataSource->rowCount() - 1);

    // 通知视图数据已更改
    QModelIndex topLeft = createIndex(startRow, 0);
    QModelIndex bottomRight = createIndex(endRow, m_dataSource->columnCount() - 1);
    emit dataChanged(topLeft, bottomRight);

    // 检查是否所有可见块都已加载
    bool allVisibleLoaded = true;
    int visibleStartBlock = getBlockIndex(m_visibleStartRow);
    int visibleEndBlock = getBlockIndex(m_visibleEndRow);

    for (int b = visibleStartBlock; b <= visibleEndBlock; ++b) {
        auto it = m_dataBlocks.find(b);
        if (it == m_dataBlocks.end() || !it.value().isValid) {
            allVisibleLoaded = false;
            break;
        }
    }

    if (allVisibleLoaded && m_loadingStatus == LoadingStatus::LoadingVisible) {
        setLoadingStatus(LoadingStatus::Idle);
    }

    // 从加载任务表中移除已完成的任务
    m_loadTasks.remove(blockIndex);
}

int VirtualTableModel::getBlockIndex(int row) const
{
    return row / m_blockSize;
}

DataBlock& VirtualTableModel::getBlock(int blockIndex)
{
    if (!m_dataBlocks.contains(blockIndex)) {
        // 创建新的数据块
        DataBlock block;
        block.startRow = blockIndex * m_blockSize;
        block.count = m_blockSize;
        block.isValid = false;
        block.lastAccessTime = 0;
        m_dataBlocks[blockIndex] = block;
    }
    return m_dataBlocks[blockIndex];
}

void VirtualTableModel::loadBlock(int blockIndex, bool priority)
{
    if (!m_dataSource)
        return;

    // 检查块是否有效或正在加载
    {
        QMutexLocker locker(&m_dataMutex);
        auto it = m_dataBlocks.find(blockIndex);
        if (it != m_dataBlocks.end() && it.value().isValid) {
            // 块已加载，更新访问时间
            it.value().lastAccessTime = QDateTime::currentMSecsSinceEpoch();
            return;
        }

        // 检查是否正在加载
        if (m_loadTasks.contains(blockIndex) && m_loadTasks[blockIndex] && m_loadTasks[blockIndex]->isRunning()) {
            return;
        }
    }

    // 计算块的实际范围
    int startRow = blockIndex * m_blockSize;
    int count = m_blockSize;

    // 确保不超出总数据范围
    if (startRow >= m_dataSource->rowCount())
        return;

    if (startRow + count > m_dataSource->rowCount()) {
        count = m_dataSource->rowCount() - startRow;
    }

    // 如果没有数据需要加载，返回
    if (count <= 0)
        return;

    // 创建加载任务
    auto loadFunction = [this, startRow, count]() {
        return m_dataSource->loadData(startRow, count);
    };

    QFuture<QList<QList<QVariant>>> future = QtConcurrent::run(QThreadPool::globalInstance(), loadFunction);
    QFutureWatcher<QList<QList<QVariant>>>* watcher = new QFutureWatcher<QList<QList<QVariant>>>(this);

    connect(watcher, &QFutureWatcher<QList<QList<QVariant>>>::finished, this, [this, blockIndex, watcher]() {
        if (watcher->future().isResultReadyAt(0)) {
            onBlockLoaded(blockIndex, watcher->future().result());
        }
        watcher->deleteLater();
    });

    watcher->setFuture(future);

    // 存储加载任务（存储指针而不是值）
    m_loadTasks[blockIndex] = watcher;
}

void VirtualTableModel::preloadBlocks(int centerBlockIndex)
{
    if (!m_dataSource)
        return;

    // 计算预加载范围
    QPair<int, int> range = calculatePreloadRange(centerBlockIndex);
    int startBlock = range.first;
    int endBlock = range.second;

    // 更新加载状态
    if (startBlock <= endBlock) {
        setLoadingStatus(LoadingStatus::LoadingPreload);
    }

    // 加载预加载范围内的块
    for (int blockIndex = startBlock; blockIndex <= endBlock; ++blockIndex) {
        // 跳过已加载或正在加载的块
        bool shouldLoad = true;

        {
            QMutexLocker locker(&m_dataMutex);
            auto it = m_dataBlocks.find(blockIndex);
            if (it != m_dataBlocks.end() && it.value().isValid) {
                shouldLoad = false;
            }

            if (m_loadTasks.contains(blockIndex) && m_loadTasks[blockIndex] && m_loadTasks[blockIndex]->isRunning()) {
                shouldLoad = false;
            }
        }

        if (shouldLoad) {
            loadBlock(blockIndex, false);
        }
    }
}

void VirtualTableModel::cleanupBlocks()
{
    if (!m_dataSource || m_dataBlocks.size() <= 10) // 如果块数量较少，不进行清理
        return;

    QMutexLocker locker(&m_dataMutex);

    // 计算可见区域的块范围
    int visibleStartBlock = getBlockIndex(m_visibleStartRow);
    int visibleEndBlock = getBlockIndex(m_visibleEndRow);

    // 计算预加载范围
    int centerBlock = (visibleStartBlock + visibleEndBlock) / 2;
    QPair<int, int> preloadRange = calculatePreloadRange(centerBlock);
    int preloadStartBlock = preloadRange.first;
    int preloadEndBlock = preloadRange.second;

    // 找出需要保留的块
    QSet<int> blocksToKeep;
    for (int i = preloadStartBlock; i <= preloadEndBlock; ++i) {
        blocksToKeep.insert(i);
    }

    // 收集所有块的访问时间
    QList<QPair<qint64, int>> blockAccessTimes;
    for (auto it = m_dataBlocks.begin(); it != m_dataBlocks.end(); ++it) {
        if (!blocksToKeep.contains(it.key())) {
            blockAccessTimes.append(qMakePair(it.value().lastAccessTime, it.key()));
        }
    }

    // 按访问时间排序
    std::sort(blockAccessTimes.begin(), blockAccessTimes.end(),
        [](const QPair<qint64, int>& a, const QPair<qint64, int>& b) {
            return a.first > b.first; // 降序排列，最近访问的在前
        });

    // 保留最近访问的一些块（最多保留10个额外的块）
    int maxExtraBlocks = 10;
    for (int i = 0; i < std::min(maxExtraBlocks, blockAccessTimes.size()); ++i) {
        blocksToKeep.insert(blockAccessTimes[i].second);
    }

    // 删除不需要的块
    QList<int> blocksToRemove;
    for (auto it = m_dataBlocks.constBegin(); it != m_dataBlocks.constEnd(); ++it) {
        if (!blocksToKeep.contains(it.key())) {
            blocksToRemove.append(it.key());
        }
    }

    for (int blockIndex : blocksToRemove) {
        m_dataBlocks.remove(blockIndex);
    }
}

QPair<int, int> VirtualTableModel::calculatePreloadRange(int centerBlockIndex) const
{
    if (!m_dataSource)
        return qMakePair(0, 0);

    // 计算总块数
    int totalBlocks = (m_dataSource->rowCount() + m_blockSize - 1) / m_blockSize;

    // 计算预加载范围
    int startBlock = std::max(0, centerBlockIndex - m_preloadBlocksBehind);
    int endBlock = std::min(totalBlocks - 1, centerBlockIndex + m_preloadBlocksAhead);

    return qMakePair(startBlock, endBlock);
}

void VirtualTableModel::updatePreloadBlockCounts()
{
    // 根据预加载策略更新预加载块数
    switch (m_preloadPolicy) {
    case PreloadPolicy::Conservative:
        m_preloadBlocksAhead = 1;
        m_preloadBlocksBehind = 0;
        break;
    case PreloadPolicy::Balanced:
        m_preloadBlocksAhead = 2;
        m_preloadBlocksBehind = 1;
        break;
    case PreloadPolicy::Aggressive:
        m_preloadBlocksAhead = 5;
        m_preloadBlocksBehind = 2;
        break;
    }
}

void VirtualTableModel::setLoadingStatus(LoadingStatus status)
{
    if (m_loadingStatus != status) {
        m_loadingStatus = status;
        emit loadingStatusChanged(status);
    }
}
