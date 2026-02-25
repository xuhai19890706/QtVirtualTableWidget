#ifndef SAMPLEDATASOURCE_H
#define SAMPLEDATASOURCE_H

#include "DataSource.h"
#include <random>

/**
 * @brief 示例数据源，用于生成测试数据
 * 
 * 这个类生成模拟数据，用于测试虚拟表格控件的性能
 */
class SampleDataSource : public DataSource
{
public:
    /**
     * @brief 构造函数
     * @param rowCount 数据总行数
     * @param columnCount 数据总列数
     */
    SampleDataSource(int rowCount, int columnCount);
    ~SampleDataSource() override = default;

    int rowCount() const override;
    int columnCount() const override;
    QList<QList<QVariant>> loadData(int startRow, int count) override;
    QList<QString> headerData() const override;

private:
    int m_rowCount;          // 总记录数
    int m_columnCount;       // 总列数
    QList<QString> m_headers; // 表头信息
    std::mt19937 m_rng;      // 随机数生成器

    /**
     * @brief 生成随机字符串
     * @param length 字符串长度
     * @return 生成的随机字符串
     */
    QString generateRandomString(int length);
};

#endif // SAMPLEDATASOURCE_H