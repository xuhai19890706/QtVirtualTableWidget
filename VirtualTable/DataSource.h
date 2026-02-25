#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <QList>
#include <QVariant>
#include <QString>

/**
 * @brief 数据源接口类，用于提供表格数据
 * 
 * 实现此接口可以从不同来源（如数据库、文件、网络等）加载数据
 */
class DataSource
{
public:
    virtual ~DataSource() = default;

    /**
     * @brief 获取总记录数
     * @return 数据总行数
     */
    virtual int rowCount() const = 0;

    /**
     * @brief 获取列数
     * @return 数据总列数
     */
    virtual int columnCount() const = 0;

    /**
     * @brief 加载指定范围的数据
     * @param startRow 起始行索引
     * @param count 要加载的行数
     * @return 加载的数据列表，每行包含多列数据
     */
    virtual QList<QList<QVariant>> loadData(int startRow, int count) = 0;

    /**
     * @brief 获取表头信息
     * @return 表头标题列表
     */
    virtual QList<QString> headerData() const = 0;
};

#endif // DATASOURCE_H