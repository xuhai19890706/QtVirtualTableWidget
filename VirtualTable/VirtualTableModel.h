#ifndef VIRTUALTABLEMODEL_H
#define VIRTUALTABLEMODEL_H

#include "DataSource.h"
#include <QAbstractTableModel>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QVariant>
#include <functional>
#include <memory>

/**
 * @brief 预加载策略枚举
 */
enum class PreloadPolicy {
    Conservative, // 保守策略：只预加载少量数据
    Balanced, // 平衡策略：预加载中等数量数据
    Aggressive // 激进策略：预加载大量数据
};

/**
 * @brief 加载状态枚举
 */
enum class LoadingStatus {
    Idle, // 空闲状态
    LoadingVisible, // 正在加载可见区域
    LoadingPreload, // 正在预加载
    LoadingAll // 正在加载所有数据
};

/**
 * @brief 数据块结构，用于存储和管理数据块
 */
struct DataBlock {
    int startRow; // 块起始行索引
    int count; // 块包含的行数
    QList<QList<QVariant>> data; // 块数据
    bool isValid; // 块数据是否有效
    qint64 lastAccessTime; // 最后访问时间
};

/**
 * @brief 虚拟表格模型类，实现千万级数据的高效加载和显示
 * 
 * 这个类是整个虚拟表格控件的核心，负责数据的分块加载、缓存管理和预加载策略
 */
class VirtualTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit VirtualTableModel(QObject* parent = nullptr);
    ~VirtualTableModel() override;

    // 重写的QAbstractItemModel方法
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
        int role = Qt::DisplayRole) const override;

    // 公共接口方法
    /**
     * @brief 设置数据源
     * @param source 数据源指针
     */
    void setDataSource(std::shared_ptr<DataSource> source);

    /**
     * @brief 设置数据块大小
     * @param blockSize 块大小
     */
    void setBlockSize(int blockSize);

    /**
     * @brief 设置预加载策略
     * @param policy 预加载策略
     */
    void setPreloadPolicy(PreloadPolicy policy);

    /**
     * @brief 直接跳转到指定行
     * @param rowIndex 目标行索引
     */
    void jumpToRow(int rowIndex);

    /**
     * @brief 获取当前加载状态
     * @return 加载状态
     */
    LoadingStatus loadingStatus() const;

    /**
     * @brief 设置可见区域范围，触发数据加载
     * @param startRow 可见区域起始行
     * @param endRow 可见区域结束行
     */
    void setVisibleRange(int startRow, int endRow);

    /**
     * @brief 设置滚动速度，用于动态调整预加载策略
     * @param speed 滚动速度（像素/秒）
     */
    void setScrollSpeed(double speed);

signals:
    /**
     * @brief 数据加载进度信号
     * @param progress 进度值（0-100）
     */
    void loadingProgress(int progress);

    /**
     * @brief 加载状态变化信号
     * @param status 新的加载状态
     */
    void loadingStatusChanged(LoadingStatus status);

private slots:
    /**
     * @brief 处理数据块加载完成
     * @param blockIndex 块索引
     * @param data 加载的数据
     */
    void onBlockLoaded(int blockIndex, const QList<QList<QVariant>>& data);

private:
    // 私有方法
    /**
     * @brief 获取指定行所在的数据块索引
     * @param row 行索引
     * @return 块索引
     */
    int getBlockIndex(int row) const;

    /**
     * @brief 获取指定块索引对应的块
     * @param blockIndex 块索引
     * @return 数据块引用
     */
    DataBlock& getBlock(int blockIndex);

    /**
     * @brief 加载指定块的数据
     * @param blockIndex 块索引
     * @param priority 是否高优先级加载
     */
    void loadBlock(int blockIndex, bool priority = false);

    /**
     * @brief 预加载数据块
     * @param centerBlockIndex 中心块索引（通常是可见区域的中心）
     */
    void preloadBlocks(int centerBlockIndex);

    /**
     * @brief 清理过期的数据块，释放内存
     */
    void cleanupBlocks();

    /**
     * @brief 计算预加载范围
     * @param centerBlockIndex 中心块索引
     * @return 预加载的块索引范围 [start, end]
     */
    QPair<int, int> calculatePreloadRange(int centerBlockIndex) const;

    /**
     * @brief 更新预加载块数量
     */
    void updatePreloadBlockCounts();

    /**
     * @brief 设置加载状态
     * @param status 新的加载状态
     */
    void setLoadingStatus(LoadingStatus status);

    // 私有成员变量
    std::shared_ptr<DataSource> m_dataSource; // 数据源
    int m_blockSize; // 数据块大小
    PreloadPolicy m_preloadPolicy; // 预加载策略
    mutable QHash<int, DataBlock> m_dataBlocks; // 数据块哈希表（标记为mutable以便在const方法中修改）
    mutable QMutex m_dataMutex; // 数据访问互斥锁
    LoadingStatus m_loadingStatus; // 当前加载状态
    int m_visibleStartRow; // 可见区域起始行
    int m_visibleEndRow; // 可见区域结束行
    double m_scrollSpeed; // 当前滚动速度
    int m_preloadBlocksAhead; // 前方预加载块数
    int m_preloadBlocksBehind; // 后方预加载块数
    QHash<int, QFutureWatcher<QList<QList<QVariant>>>*> m_loadTasks; // 加载任务表（存储指针）
};

#endif // VIRTUALTABLEMODEL_H
