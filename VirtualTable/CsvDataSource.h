#ifndef CSVDATASOURCE_H
#define CSVDATASOURCE_H

#include "DataSource.h"
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QList>
#include <QVariant>
#include <QHash>
#include <QMutex>
#include <memory>
#include <vector>

/**
 * @brief CSV文件数据源类，用于从CSV文件加载数据
 * 
 * 这个类实现了DataSource接口，可以从CSV文件中读取数据并提供给虚拟表格控件。
 * 支持分块加载，只在需要时读取文件的特定部分，适合处理大型CSV文件。
 */
class CsvDataSource : public DataSource
{
public:
    /**
     * @brief 构造函数
     * @param filePath CSV文件路径
     * @param hasHeader 是否包含表头
     * @param delimiter 分隔符，默认为逗号
     * @param maxCacheSize 最大缓存行数
     */
    CsvDataSource(const QString &filePath, bool hasHeader = true, char delimiter = ',', int maxCacheSize = 10000);
    ~CsvDataSource() override;

    // 实现DataSource接口
    int rowCount() const override;
    int columnCount() const override;
    QList<QList<QVariant>> loadData(int startRow, int count) override;
    QList<QString> headerData() const override;

    /**
     * @brief 获取文件路径
     * @return CSV文件路径
     */
    QString filePath() const;

    /**
     * @brief 检查文件是否有效
     * @return 文件是否有效
     */
    bool isValid() const;

    /**
     * @brief 获取错误信息
     * @return 错误信息，如果没有错误则返回空字符串
     */
    QString errorString() const;

private:
    // 私有方法
    /**
     * @brief 初始化数据源，读取文件头和计算总行数
     * @return 是否初始化成功
     */
    bool initialize();

    /**
     * @brief 从文件中读取指定行
     * @param rowIndex 行索引
     * @return 读取的行数据
     */
    QList<QVariant> readRow(int rowIndex);

    /**
     * @brief 解析CSV行
     * @param line CSV行字符串
     * @return 解析后的数据列表
     */
    QList<QVariant> parseLine(const QString &line);

    /**
     * @brief 定位到文件的指定行
     * @param rowIndex 行索引
     * @return 是否成功定位
     */
    bool seekToRow(int rowIndex);

    QString getLineFromMappedData(int rowIndex);

    /**
     * @brief 缓存行数据
     * @param rowIndex 行索引
     * @param data 行数据
     */
    void cacheRow(int rowIndex, const QList<QVariant> &data);

    /**
     * @brief 从缓存中获取行数据
     * @param rowIndex 行索引
     * @param data 输出参数，用于存储行数据
     * @return 是否从缓存中找到数据
     */
    bool getFromCache(int rowIndex, QList<QVariant> &data) const;

    /**
     * @brief 清理缓存，移除最旧的缓存项
     */
    void cleanupCache();

    // 私有成员变量
    QString m_filePath;               // CSV文件路径
    mutable QFile m_file;             // 文件对象
    bool m_hasHeader;                 // 是否包含表头
    char m_delimiter;                 // 分隔符
    int m_rowCount;                   // 总行数，-1表示未计算
    int m_columnCount;                // 总列数
    QList<QString> m_headers;         // 表头信息
    bool m_isValid;                   // 文件是否有效
    QString m_errorString;            // 错误信息
    mutable QMutex m_mutex;           // 互斥锁，用于线程安全

    // 内存映射相关
    qint64 m_fileSize;                // 文件大小
    std::vector<qint64> m_rowOffsets; // 存储每行的偏移量，用于快速定位

    // 缓存相关
    int m_maxCacheSize;               // 最大缓存行数
    QHash<int, QList<QVariant>> m_rowCache; // 行缓存
    QList<int> m_cacheOrder;          // 缓存顺序，用于LRU缓存策略

private:
    // 辅助方法
    bool ensureRowOffsetCalculated(int rowIndex);
    void ensureRowOffsetsCalculated(int startRow, int endRow);
    void calculateRowCount();
};

#endif // CSVDATASOURCE_H