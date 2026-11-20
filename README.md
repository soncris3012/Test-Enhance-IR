# Test-Enhance-IR: Infrared Image Detail Enhancement

Ứng dụng C++17 / Qt 6 kiểm thử và trực quan hóa thuật toán **Tăng cường chi tiết ảnh nhiệt hồng ngoại (Infrared Image Detail Enhancement)** dựa trên bài báo khoa học:

> **"Infrared Image Detail Enhancement Algorithm Based on Guided Filtering and Adaptive Dual-Gamma Nonlinear Mapping"**  
> *Yuxuan Zhang, Guanghui Li, Jianfeng Zhang, Fanbin Zeng, Yizhuo Wang, Guoliang Hu* (ICADI 2025, ACM).

---

## 1. Tổng quan & Cơ sở lý thuyết thuật toán

Ảnh hồng ngoại nhiệt (Thermal Infrared - TIR) thường có đặc tính: độ tương phản tổng thể thấp, biên cạnh chi tiết bị nhòe và dễ bị nhiễu cảm biến nhiệt độ. Thuật toán phân rã ảnh thành lớp nền và lớp chi tiết, khử nhiễu chi tiết bằng lọc phi cục bộ thích nghi 16-bit (NLM), sau đó tăng cường biên/chi tiết bằng ánh xạ phi tuyến hai nhánh (Dual-$\gamma$):

```text
                     [Ảnh hồng ngoại gốc I (8-bit / 16-bit)]
                                        │
                     ┌──────────────────┴──────────────────┐
                     │                                     │
                     ▼                                     │
          [1. Guided Image Filtering]                      │
                     │                                     │
                     ▼                                     ▼
             [Lớp nền I_base] ──────( Phép trừ )────► [2. Lớp chi tiết I_de]
           (Cấu trúc & độ sáng)                          (Nhiễu + chi tiết)
                     │                                     │
                     │                                     ▼
                     │                        [3. Adaptive 16-bit NLM]
                     │                         (Khử nhiễu theo MAD σ)
                     │                                     │
                     │                                     ▼
                     │                           [4. Dual-Gamma Mapping]
                     │                          (Tăng cường chi tiết 2 nhánh)
                     │                                     │
                     │                                     ▼
                     │                            [Lớp chi tiết I'_de]
                     │                                     │
                     └──────────────────┬──────────────────┘
                                        │
                                        ▼
                               [5. Weighted Fusion]
                                        │
                                        ▼
                              [Ảnh tăng cường đầu ra]
```

### Chi tiết các bước & lưu ý toán học:

1. **Phân rã ảnh bằng Guided Filter (Mục 3.1)**:
   - Tự dẫn đường ($I$ vừa là ảnh hướng dẫn vừa là ảnh đầu vào), kẹp phương sai cục bộ $\sigma_k^2 \ge 0$ để tránh sai số dấu phẩy động ở vùng đồng nhất sáng.
   - Tham số mặc định từ bài báo: Bán kính $r = 2$ (cửa sổ $5 \times 5$), hệ số điều hòa $\epsilon = 0.04$.
   - Tách thành: Lớp nền $I_{base}$ và lớp chi tiết có dấu $I_{de} = I - I_{base}$.

2. **Lọc nhiễu chi tiết thích nghi qua Adaptive 16-bit NLM (Mục 3.2)**:
   - Chuẩn hóa đối xứng giữ nguyên tâm $0$ tại $0.5$ và lượng tử hóa **16-bit (`CV_16UC1`, `NORM_L1`)** thay cho 8-bit để bảo toàn các thang chuyển nhiệt tinh tế.
   - Khi bật **Adaptive NLM**, hệ số lọc $h$ được tự động co giãn dựa trên ước lượng nhiễu **MAD (Median Absolute Deviation)** của tầng chi tiết.

3. **Ánh xạ phi tuyến Dual-$\gamma$ & 3 chế độ kiểm thử (Mục 3.3 & 3.4)**:
   - **Ghi chú kỹ thuật về Công thức (8) trong bài báo**: Công thức in trong bài báo là $f(x) = \frac{1}{2}[x^{1/\gamma} + (1 - x)^\gamma]$ (bị thiếu `1 -` ở số hạng thứ hai so với hàm bi-gamma đơn điệu chuẩn). Quanh điểm không chi tiết $x = 0.5$, đạo hàm $f'(0.5) < 0$ làm nghịch đảo cạnh biên và đẩy vùng phẳng lên giá trị nền xám $0.48$, dẫn đến giảm Entropy ảnh (IE) và Gradient Entropy (GE).
   - Để kiểm chứng trực quan và đối chiếu khoa học, ứng dụng cung cấp **3 chế độ (Mode)** chuyển đổi trực tiếp trên UI & CLI:
     1. **`Symmetric Dual-Gamma (Recommended)`** *(Mặc định)*: Áp dụng đường cong đơn điệu $g(m) = \frac{1}{2}[m^{1/\gamma} + 1 - (1 - m)^\gamma]$ trên biên độ chi tiết $m \in [0, 1]$ kết hợp bảo toàn dấu cạnh $\text{sign}(I_{de})$ khi cộng lại vào $I_{base}$. Tăng rõ rệt cả **IE** lẫn **GE** mà không gây xám nền.
     2. **`Paper Magnitude (Fig 2c / 3c)`**: Áp dụng hàm đơn điệu lên $|I_{de}|$ và hòa trộn lồi $\alpha I_{base} + (1 - \alpha) I'_{de}$, tái hiện đúng hiệu ứng nền đen của lớp chi tiết (Hình 3c) và ảnh kết quả (Hình 2c).
     3. **`Paper Literal Eq. 8 (Original)`**: Chạy nguyên văn Công thức (8) và (9) như bản in của bài báo (kèm tùy chọn `Min-Max`, `Shift +0.5`, `Symmetric Centered`) để phục vụ so sánh / ablation.

---

## 2. Tính năng của ứng dụng

* **Hỗ trợ đa định dạng ảnh nhiệt**: Hỗ trợ ảnh 8-bit (PNG/JPG/BMP), ảnh màu (tự chuyển grayscale) và **ảnh nhiệt 16-bit TIFF (`CV_16U`)** với chuẩn hóa min-max không mất dữ liệu dải động cao.
* **Bộ nạp Dataset Benchmark hồng ngoại toàn diện (LLVIP, FLIR, MSRS, RoadScene)**:
  - Tự động nhận diện thư mục dataset trên máy cá nhân (`~/QART_Datasets/raw`, `QART_Datasets/`, hoặc biến môi trường `$IR_DATASET_DIR`).
  - Hỗ trợ nút **📁 Set Root...** để đổi thư mục dataset sang bất kỳ đường dẫn nào trên máy khác (lưu cấu hình lâu dài qua `QSettings`).
  - Nút **Open Folder...** cho phép mở thư mục ảnh bất kỳ trên ổ cứng.
  - Bộ điều hướng ảnh: Nút **◀ Prev** / **Next ▶** hoặc **phím mũi tên Trái / Phải (Left / Right Arrow)** giúp lướt nhanh hàng nghìn ảnh nhiệt thời gian thực.
* **Giao diện Qt 6 tối giản & Chống bẹp font (`apps/MainWindow.ui`)**:
  - Thiết kế tông tối trung tính, bọc trong `QScrollArea` cuộn mượt mà trên mọi độ phân giải màn hình.
  - Khóa `minimumHeight` chống hiện tượng vỡ/cắt chữ trong Qt Creator Designer.
* **Trực quan hóa 2 Tabs với Zoom/Pan đồng bộ**:
  1. **Before / After Comparison**: So sánh song song ảnh gốc và ảnh tăng cường.
  2. **Algorithm Pipeline (All 6 Stages)**: Quan sát cả 6 tầng trung gian ($I \to I_{base} \to I_{de} \to \text{NLM} \to \text{Dual-}\gamma \to I_{out}$).
  - **Tương tác chuột**: Lăn chuột giữa để zoom tại vị trí con trỏ, kéo chuột trái để pan, **nhấp đúp chuột trái (double-click)** để reset về fit-to-window; đồng bộ hóa zoom/pan/reset trên toàn bộ các khung hình trong cùng tab.
* **Đo lường chỉ số định lượng thời gian thực**:
  - Thời gian xử lý (ms), Image Entropy (**IE**), Gradient Entropy (**GE**), **RMS Contrast**.
  - **RMSE**, **PSNR**, **SSIM** tự động đo so với **Reference** (nếu đã nạp) hoặc so với **Original Input** (nếu chưa nạp Reference).
* **Xuất kết quả an toàn**: Lưu ảnh đầu ra (`Save Output...`) hoặc xuất đồng loạt cả 6 tầng (`Export All Stages...`) với kiểm tra lỗi ghi file rõ ràng.

---

## 3. Cấu trúc thư mục mã nguồn

```text
Test-Enhance-IR/
├── CMakeLists.txt                # CMake C++17: tách core (OpenCV-only) và GUI (Qt6)
├── README.md                     # Tài liệu hướng dẫn sử dụng chi tiết
├── run_gui.sh                    # Script tự cấu hình/build và chạy ứng dụng GUI
├── include/
│   ├── guided_filter.hpp         # Header thuật toán Guided Filter He et al.
│   ├── enhancement_pipeline.hpp  # Pipeline 5 bước, 3 chế độ xử lý & hàm đo metrics
│   └── image_canvas.hpp          # Widget Qt hiển thị ảnh (Zoom / Pan / Double-click reset)
├── src/
│   ├── guided_filter.cpp         # Guided Filter O(1) dùng boxFilter + bảo vệ phương sai âm
│   ├── enhancement_pipeline.cpp  # Xử lý 8/16-bit, 16-bit Adaptive NLM, Dual-Gamma, Metrics
│   └── image_canvas.cpp          # Vẽ ảnh & đồng bộ tương tác chuột
├── apps/
│   ├── MainWindow.ui             # File giao diện Qt Designer XML
│   ├── MainWindow.hpp / .cpp     # Logic điều khiển giao diện & tương tác
│   ├── main.cpp                  # Điểm khởi chạy ứng dụng GUI
│   └── cli_test.cpp              # Ứng dụng dòng lệnh (CLI) thuần C++/OpenCV
├── sample_images/                # 8 Cảnh mẫu (Scene 1–8) & Fig 2–3 tách sạch không viền
└── results/                      # Thư mục lưu kết quả test CLI
```

---

## 4. Hướng dẫn cài đặt & Biên dịch

### Yêu cầu hệ thống:
* Trình biên dịch hỗ trợ C++17 (Clang, GCC hoặc MSVC).
* **CMake** $\ge$ 3.20.
* **Qt 6** (`Core`, `Gui`, `Widgets`) — chỉ cần cho `ir_enhance_gui`.
* **OpenCV** $\ge$ 4.x / 5.x.

### Các bước biên dịch:

```bash
# 1. Di chuyển vào thư mục dự án
cd Test-Enhance-IR

# 2. Cấu hình CMake (ví dụ trên macOS Homebrew; Linux/Windows thường tự nhận diện)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix opencv)"

# 3. Biên dịch cả CLI (ir_enhance_cli) và GUI (ir_enhance_gui)
cmake --build build -j4
```

---

## 5. Hướng dẫn sử dụng

### 5.1. Giao diện Đồ họa (GUI)

Khởi chạy bằng script (tự động build lại nếu có thay đổi code):
```bash
./run_gui.sh
```
Hoặc chạy trực tiếp file thực thi:
```bash
./build/ir_enhance_gui
```

**Thao tác nhanh trên UI**:
1. **Chọn Dataset**: Chọn một trong các tập dữ liệu có sẵn (`Paper Samples`, `LLVIP (Test/Train)`, `FLIR`, `MSRS`, `RoadScene`) hoặc bấm **Open Folder...** để mở thư mục bất kỳ.
2. **Duyệt ảnh**: Dùng nút **◀** / **▶** hoặc nhấn **phím mũi tên Trái / Phải** trên bàn phím để chuyển đổi tức thì giữa hàng ngàn ảnh trong dataset.
3. **Đổi thư mục Dataset Root**: Bấm icon **📁** bên cạnh ô Dataset để trỏ tới thư mục dữ liệu trên máy tính khác (được lưu vĩnh viễn qua `QSettings`).
4. **Mở ảnh đơn / Reference**: Bấm **Open File...** hoặc **Load Ref...** để nạp ảnh tùy ý.
5. **Chọn Mode & Tinh chỉnh**: Chọn `Symmetric Dual-Gamma`, `Paper Magnitude`, hoặc `Paper Literal Eq. 8` và kéo các thanh trượt tham số (`r`, `eps`, `h`, `gamma`, `alpha`).
6. **Tương tác Canvas**: Cuộn chuột để phóng to, kéo chuột trái để di chuyển, **nhấp đúp chuột trái (double-click)** để khôi phục vừa khung hình.
7. **Lưu kết quả**: Bấm **Save Output...** hoặc **Export All Stages...** để lưu ảnh kết quả.

### 5.2. Kiểm thử Dòng lệnh (CLI Benchmark)

`ir_enhance_cli` không phụ thuộc thư viện Qt GUI, có thể chạy trực tiếp trên server/headless với cả ảnh đơn lẫn **đánh giá hàng loạt (Batch Evaluation)**:

```bash
# Liệt kê các dataset tự động nhận diện được trên máy:
./build/ir_enhance_cli --list-datasets

# Chạy đánh giá hàng loạt 50 ảnh từ LLVIP Test và xuất báo cáo CSV:
./build/ir_enhance_cli --dataset llvip_test --limit 50 --out-dir results/llvip_eval

# Chạy trên một thư mục ảnh bất kỳ:
./build/ir_enhance_cli --dir /path/to/my/infrared_images --limit 100 --out-dir results/custom_eval

# Chạy kiểm thử trên 1 ảnh đơn lẻ (kèm reference nếu có):
./build/ir_enhance_cli sample_images/scene_1_original.png sample_images/scene_1_paper_ours.png
```

Kết quả mẫu khi chạy đánh giá hàng loạt trên LLVIP:
```text
=== IR Image Detail Enhancement CLI Testbed ===
Running Batch Evaluation on directory: /Users/soncris/QART_Datasets/raw/llvip/infrared/test
Evaluating 10 images...
Progress: 10/10 (258.8 ms/img)
Batch evaluation completed!

========================================================
                  BENCHMARK RESULTS
========================================================
Total Images Tested : 10
Average Latency     : 257.3 ms (3.9 FPS)
Information Entropy : 7.37 -> 7.39 (Δ +0.02)
Gradient Entropy    : 3.31 -> 3.36 (Δ +0.05)
RMS Contrast        : 0.201 -> 0.205 (Δ +0.004)
========================================================
Report saved to: results/llvip_test_eval/benchmark_report.csv
```

---

## 6. Bảng hướng dẫn tinh chỉnh tham số

| Tham số | Ý nghĩa | Mặc định | Tác động khi tăng / giảm |
|---|---|---|---|
| **Radius $r$** | Bán kính cửa sổ lọc Guided Filter | `2` | Tăng lên làm lớp nền mịn hơn, đẩy thêm chi tiết trung bình sang lớp detail; giảm xuống giữ cấu trúc nền sát ảnh gốc hơn. |
| **Eps $\epsilon$** | Hệ số điều hòa Guided Filter | `0.040` | Tăng lên làm trơn mạnh hơn các vùng độ biến thiên thấp trong khi vẫn giữ các biên mạnh. |
| **NLM $h$** | Độ mạnh khử nhiễu phi cục bộ 16-bit | `10.0` | Tăng lên khử nhiễu hạt mạnh hơn; đặt `0` để tắt bước khử nhiễu NLM. |
| **Adaptive NLM** | Tự co giãn $h$ theo nhiễu MAD | `Bật` | Tự điều chỉnh lực lọc phù hợp từng ảnh; tắt nếu muốn cố định $h$ tuyệt đối. |
| **Gamma $\gamma$** | Hệ số mũ Dual-Gamma | `2.0` | Tăng lên đẩy mạnh độ tương phản các chi tiết yếu trong lớp detail. |
| **Alpha $\alpha$** | Trọng số hòa trộn Base / Detail | `0.60` | Giảm $\alpha$ tăng độ nổi khối chi tiết (ở Symmetric mode) hoặc tăng tỷ trọng $I'_{de}$ (ở Paper mode); $\alpha = 1.0$ trả về thuần lớp nền $I_{base}$. |

---

## 7. Giấy phép (License)
Dự án được phát triển phục vụ mục đích nghiên cứu khoa học và học thuật.
