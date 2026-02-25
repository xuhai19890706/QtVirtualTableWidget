import csv
import random
import string
import time
from datetime import datetime, timedelta

def generate_large_csv(file_path, total_rows, batch_size=100000):
    """
    生成大体积CSV测试文件
    
    Args:
        file_path: 输出文件路径（如'large_test.csv'）
        total_rows: 总行数（建议1亿行左右可生成1GB左右文件，可根据字段调整）
        batch_size: 每次写入的批次行数（避免内存溢出）
    """
    # 定义CSV表头
    headers = ['id', 'name', 'age', 'email', 'phone', 'register_time', 'salary', 'address']
    
    # 预生成一些随机数据模板，提升生成速度
    first_names = ['Zhang', 'Li', 'Wang', 'Zhao', 'Chen', 'Yang', 'Huang', 'Zhou', 'Wu', 'Xu']
    last_names = ['Wei', 'Qiang', 'Fang', 'Ying', 'Jie', 'Hong', 'Lei', 'Mei', 'Juan', 'Ling']
    domains = ['gmail.com', 'yahoo.com', 'outlook.com', '163.com', 'qq.com']
    provinces = ['Beijing', 'Shanghai', 'Guangdong', 'Jiangsu', 'Zhejiang', 'Shandong', 'Sichuan']
    
    # 计算需要分多少批写入
    total_batches = (total_rows + batch_size - 1) // batch_size
    
    print(f"开始生成CSV文件，总行数：{total_rows}，分{total_batches}批写入")
    start_time = time.time()

    # 以追加模式打开文件（流式写入）
    with open(file_path, 'w', newline='', encoding='utf-8', buffering=1024*1024) as f:
        writer = csv.writer(f)
        # 先写入表头
        writer.writerow(headers)
        
        for batch in range(total_batches):
            # 计算当前批次的起始和结束行号
            start_row = batch * batch_size + 1
            end_row = min((batch + 1) * batch_size, total_rows)
            current_batch_size = end_row - start_row + 1
            
            # 生成当前批次的数据
            batch_data = []
            for row_id in range(start_row, end_row + 1):
                # 生成随机数据
                name = random.choice(first_names) + random.choice(last_names)
                age = random.randint(18, 60)
                email = name.lower() + str(random.randint(1000, 9999)) + '@' + random.choice(domains)
                phone = '1' + ''.join(random.choices(string.digits, k=10))
                # 随机生成注册时间（近10年）
                start_date = datetime.now() - timedelta(days=365*10)
                random_days = random.randint(0, 365*10)
                register_time = (start_date + timedelta(days=random_days)).strftime('%Y-%m-%d %H:%M:%S')
                salary = round(random.uniform(3000, 50000), 2)
                address = random.choice(provinces) + ' ' + ''.join(random.choices(string.ascii_letters + string.digits, k=10))
                
                # 组装一行数据
                batch_data.append([
                    row_id, name, age, email, phone, register_time, salary, address
                ])
            
            # 写入当前批次数据
            writer.writerows(batch_data)
            
            # 打印进度
            progress = (batch + 1) / total_batches * 100
            elapsed_time = time.time() - start_time
            print(f"进度：{progress:.1f}% | 已写入{end_row}行 | 耗时{elapsed_time:.2f}秒", end='\r')
    
    # 最终统计
    total_time = time.time() - start_time
    print(f"\n文件生成完成！路径：{file_path}")
    print(f"总耗时：{total_time:.2f}秒 | 平均速度：{total_rows/total_time:.0f}行/秒")

if __name__ == "__main__":
    # 生成约1GB的CSV文件（根据字段长度，1亿行左右约1GB，可调整total_rows）
    # 注意：生成前确保磁盘有足够空间
    generate_large_csv(
        file_path='large_test_data1.csv',
        total_rows=10000000,  # 1亿行，可根据需要调整（比如5亿行≈5GB）
        batch_size=100000       # 每批10万行，内存占用约几十MB
    )