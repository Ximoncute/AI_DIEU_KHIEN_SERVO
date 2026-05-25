### Egde Impluse 
data set 
https://studio.edgeimpulse.com/studio/1008244/acquisition/training?page=1

<img width="1110" height="1276" alt="image" src="https://github.com/user-attachments/assets/760784e3-8f9e-4fc5-9571-28029e5cf165" />
# 🎮 AI Điều Khiển Servo

[![GitHub stars](https://img.shields.io/github/stars/Ximoncute/AI_DIEU_KHIEN_SERVO)](https://github.com/Ximoncute/AI_DIEU_KHIEN_SERVO/stargazers)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

> **Dự án AI nhúng điều khiển Servo bằng giọng nói với độ chính xác lên đến 90.9%**

## 📌 Giới thiệu

Dự án này sử dụng **TinyML** và **Edge Impulse** để xây dựng mô hình nhận dạng giọng nói, điều khiển động cơ Servo qua 4 lệnh cơ bản: **BẬT**, **TẮT**, **MỘT** và lọc **NHIỄU**. Mô hình được tối ưu hóa để chạy trên vi điều khiển với kích thước nhỏ gọn và hiệu suất cao.

## 🧠 Mô hình AI

### Kiến trúc
- **Loại mô hình:** Neural Network (NN)
- **Lượng tử hóa:** INT8 (tối ưu cho vi điều khiển)
- **Đầu vào:** Phổ tần số giọng nói (MFCC)
- **Đầu ra:** 4 lớp (NOISE, OFF, ON, ONE)

### Hiệu suất trên tập validation

| Chỉ số | Giá trị |
|--------|---------|
| **Độ chính xác** | **90.9%** |
| **Loss** | 0.37 |
| **AUC-ROC** | 0.98 |
| **Precision (trung bình)** | 0.91 |
| **Recall (trung bình)** | 0.91 |
| **F1-score (trung bình)** | 0.91 |

### Ma trận nhầm lẫn

|         | NOISE | OFF  | ON   | ONE  |
|---------|-------|------|------|------|
| **NOISE** | 86.1% | 4.3% | 5.5% | 4.1% |
| **OFF**   | 2.8%  | 94.2%| 2.8% | 0.2% |
| **ONE**   | 4.3%  | 0.2% | 2.1% | 93.4%|
| **ON**    | 4.6%  | 5.0% | 88.8%| 1.7% |

### F1-score từng lớp
- **NOISE:** 0.85
- **OFF:** 0.93
- **ON:** 0.90  
- **ONE:** 0.94

