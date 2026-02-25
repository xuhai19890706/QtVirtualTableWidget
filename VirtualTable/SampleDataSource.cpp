#include "SampleDataSource.h"
#include <chrono>
#include <string>

SampleDataSource::SampleDataSource(int rowCount, int columnCount)
    : m_rowCount(rowCount),
      m_columnCount(columnCount),
      m_rng(std::chrono::system_clock::now().time_since_epoch().count())
{
    // 生成表头信息
    for (int i = 0; i < columnCount; ++i) {
        m_headers.append(QString("Column %1").arg(i + 1));
    }
}

int SampleDataSource::rowCount() const
{
    return m_rowCount;
}

int SampleDataSource::columnCount() const
{
    return m_columnCount;
}

QList<QList<QVariant>> SampleDataSource::loadData(int startRow, int count)
{
    QList<QList<QVariant>> data;
    
    // 确保不超出范围
    int endRow = std::min(startRow + count, m_rowCount);
    
    // 模拟数据加载延迟（实际应用中可能从数据库或文件加载）
    // QThread::msleep(5); // 取消注释可模拟慢速数据源
    
    // 生成数据
    for (int row = startRow; row < endRow; ++row) {
        QList<QVariant> rowData;
        for (int col = 0; col < m_columnCount; ++col) {
            // 根据列索引生成不同类型的数据
            if (col == 0) {
                // 第一列是行号
                rowData.append(row + 1);
            } else if (col == 1) {
                // 第二列是随机整数
                std::uniform_int_distribution<int> dist(1000, 9999);
                rowData.append(dist(m_rng));
            } else if (col == 2) {
                // 第三列是随机浮点数
                std::uniform_real_distribution<double> dist(0.0, 100.0);
                rowData.append(QString::number(dist(m_rng), 'f', 2));
            } else if (col == 3) {
                // 第四列是随机字符串
                rowData.append(generateRandomString(10 + (row % 20)));
            } else {
                // 其他列是混合数据
                if (row % 3 == 0) {
                    rowData.append(generateRandomString(5));
                } else if (row % 3 == 1) {
                    std::uniform_int_distribution<int> dist(1, 100);
                    rowData.append(dist(m_rng));
                } else {
                    rowData.append(QString("Data-%1-%2").arg(row).arg(col));
                }
            }
        }
        data.append(rowData);
    }
    
    return data;
}

QList<QString> SampleDataSource::headerData() const
{
    return m_headers;
}

QString SampleDataSource::generateRandomString(int length)
{
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<int> dist(0, chars.size() - 1);
    
    QString result;
    for (int i = 0; i < length; ++i) {
        result.append(chars[dist(m_rng)]);
    }
    return result;
}
