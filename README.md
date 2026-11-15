# Test-Enhance-IR: Infrared Image Detail Enhancement

Ứng dụng C++ / Qt 6 kiểm thử và trực quan hóa thuật toán **Tăng cường chi tiết ảnh nhiệt hồng ngoại (Infrared Image Detail Enhancement)** dựa trên bài báo khoa học:

> **"Infrared Image Detail Enhancement Algorithm Based on Guided Filtering and Adaptive Dual-Gamma Nonlinear Mapping"**  
> *Yuxuan Zhang, Guanghui Li, Jianfeng Zhang, Fanbin Zeng, Yizhuo Wang, Guoliang Hu* (ICADI 2025, ACM).

---

## 1. Tổng quan & Cơ sở lý thuyết thuật toán

Ảnh hồng ngoại nhiệt (Thermal Infrared - TIR) thường có đặc tính: độ tương phản tổng thể thấp, biên cạnh chi tiết bị nhòe và dễ bị nhiễu cảm biến nhiệt độ. Thuật toán trong bài báo giải quyết bằng quy trình phân rã phân tầng (hierarchical decomposition) kết hợp lọc phi cục bộ (NLM) và ánh xạ phi tuyến hai nhánh (Dual-$\gamma$):

```text
                     [Ảnh hồng ngoại gốc I]
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
             │                        [3. Adaptive Non-Local Means]
             │                            (Khử nhiễu chi tiết)
             │                                     │
             │                                     ▼
             │                           [4. Dual-Gamma Mapping]
             │                           (Cân bằng sáng/tối Eq 8)
             │                                     │
             │                                     ▼
             │                            [Lớp chi tiết I'_de]
             │                                     │
             └──────────────────┬──────────────────┘
                                │
                                ▼
                       [5. Weighted Fusion]
                 (Eq 9: I_out = α*I_base + (1-α)*I'_de)
                                │
                                ▼
                      [Ảnh tăng cường đầu ra]
```

### Chi tiết các công thức toán học:

1. **Phân rã ảnh bằng Guided Filter (Mục 3.1)**:
   - Tự dẫn đường ($I$ vừa là ảnh hướng dẫn vừa là ảnh đầu vào) để làm trơn ảnh nhưng giữ sắc nét các đường biên cạnh mạnh.
   - Tham số khuyến nghị từ bài báo: Bán kính cửa sổ lọc $r = 2$ (kích thước $(2r+1) \times (2r+1) = 5 \times 5$), hệ số điều hòa $\epsilon = 0.04$.
   - Tách thành: Lớp nền $I_{base}$ và lớp chi tiết thô $I_{de} = I - I_{base}$.

2. **Lọc nhiễu chi tiết thích nghi qua Non-Local Means (Mục 3.2)**:
   - Tận dụng tính tự tương đồng toàn cục (global-local structural similarity) của ảnh để triệt tiêu nhiễu tần số cao mà không làm mờ vân chi tiết:
     $$I'_{de}(x, y) = \frac{\sum_{w \in W} w(x,y) I_{de}(x,y)}{\sum_{w \in W} w(x,y)}$$
   - Trọng số $w(x, y)$ phụ thuộc vào khoảng cách patch Gaussian và tham số độ mạnh lọc $h = 10.0$ (search window $21 \times 21$, template patch $7 \times 7$).

3. **Ánh xạ phi tuyến thích nghi Dual-$\gamma$ (Mục 3.3, Công thức 8)**:
   - Khắc phục nhược điểm của Gamma đơn truyền thống (dễ gây cháy sáng vùng sáng hoặc mất chi tiết vùng tối):
     $$I'_{de} = \frac{1}{2} \left[ (I_{de})^{\frac{1}{\gamma}} + (1 - I_{de})^\gamma \right]$$
   - Nhánh $I_{de}^{1/\gamma}$ nâng độ sáng các chi tiết tối/yếu.
   - Nhánh $(1 - I_{de})^\gamma$ kéo lại các chi tiết vùng quá sáng, chống bão hòa (over-enhancement).
   - Tham số chuẩn: $\gamma = 2.0$.

4. **Hòa trộn có trọng số (Mục 3.4, Công thức 9)**:
   $$I_{out} = \alpha I_{base} + (1 - \alpha) I'_{de}$$
   - Tham số bài báo: $\alpha = 0.6$ (Lớp nền giữ 60%, lớp chi tiết tăng cường chiếm 40%).
   - Biểu thức tổ hợp lồi đảm bảo $I_{out} \in [0, 1]$, triệt tiêu hiện tượng tràn ngưỡng (clipping) của ảnh 8-bit.

---

## 2. Tính năng của ứng dụng

* **Giao diện Qt 6 chuẩn mực & trực quan**:
  - Thiết kế tông màu tối trung tính, tối giản (minimalist dark theme), thân thiện mắt, không màu mè rối mắt.
  - Tách rời giao diện qua file XML chuẩn [`apps/MainWindow.ui`](apps/MainWindow.ui), hỗ trợ kéo thả chỉnh sửa trực tiếp bằng **Qt Designer** hoặc **Qt Creator**.
* **Trực quan hóa đa góc nhìn (3 Tabs chuyên biệt)**:
  1. **Before / After Comparison**: Xem song song ảnh gốc và ảnh sau khi tăng cường. Hỗ trợ **Đồng bộ Zoom & Pan** (khi phóng to hoặc rê chuột ở một bên, bên còn lại sẽ tự động căn chỉnh cùng tọa độ).
  2. **Algorithm Pipeline (All 6 Stages)**: Hiển thị ma trận $2 \times 3$ trọn vẹn cả 6 giai đoạn xử lý trung gian:
     - 1. Ảnh gốc ($I$)
     - 2. Lớp nền Guided Filter ($I_{base}$)
     - 3. Lớp chi tiết thô ($I - I_{base}$)
     - 4. Chi tiết sau khử nhiễu NLM
     - 5. Chi tiết sau ánh xạ Dual-$\gamma$
     - 6. Kết quả hòa trộn cuối cùng ($I_{out}$)
  3. **Benchmarking vs Paper Ours**: Nạp ảnh kết quả gốc của bài báo để so sánh đối chứng, đồng thời tạo **Bản đồ nhiệt sai khác (Difference Heatmap)** trực quan bằng Jet colormap.
* **Bộ điều khiển tham số thời gian thực (Interactive Sliders)**:
  - Bán kính lọc $r$ và hệ số $\epsilon$ của Guided Filter.
  - Độ mạnh khử nhiễu $h$ của NLM.
  - Hệ số mũ $\gamma$ của hàm Dual-Gamma.
  - Trọng số hòa trộn $\alpha$.
  - 3 chế độ chuẩn hóa Detail: `Min-Max (chuẩn bài báo)`, `Shift (+0.5)`, `Symmetric Centered`.
  - Nút **"Defaults"**: Reset tức thì về đúng các giá trị gốc tác giả công bố ($r=2, \epsilon=0.04, h=10, \gamma=2, \alpha=0.6$).
* **Đo lường chỉ số định lượng thời gian thực**:
  - Thời gian xử lý từng khung hình (thường chỉ 16–20 ms).
  - Entropy thông tin ảnh (Image Entropy - IE).
  - Gradient Entropy (GE).
  - Độ tương phản RMS (Root Mean Square Contrast).
  - Đo PSNR (Peak Signal-to-Noise Ratio) & SSIM (Structural Similarity) khi nạp ảnh tham chiếu.
* **Xuất dữ liệu & Kết quả**:
  - Nút **"Save Output..."**: Lưu ảnh kết quả tăng cường định dạng PNG/JPG.
  - Nút **"Export All Stages..."**: Tự động kết xuất trọn bộ cả 6 ảnh giai đoạn trung gian ra thư mục được chọn.

---

## 3. Cấu trúc thư mục mã nguồn

```text
Test-Enhance-IR/
├── CMakeLists.txt                # Cấu hình biên dịch CMake C++17, Qt6, OpenCV
├── README.md                     # Tài liệu hướng dẫn sử dụng chi tiết
├── run_gui.sh                    # Script chạy nhanh ứng dụng giao diện
├── include/
│   ├── guided_filter.hpp         # Header thuật toán Guided Filter He et al.
│   ├── enhancement_pipeline.hpp  # Pipeline 5 bước & hàm đo đạc metrics
│   └── image_canvas.hpp          # Widget Qt hiển thị ảnh hỗ trợ Pan / Zoom
├── src/
│   ├── guided_filter.cpp         # Hiện thực Guided Filter O(1) dùng boxFilter
│   ├── enhancement_pipeline.cpp  # Hiện thực NLM, Dual-Gamma, Fusion, Metrics
│   └── image_canvas.cpp          # Xử lý sự kiện vẽ ảnh & tương tác chuột
├── apps/
│   ├── MainWindow.ui             # File giao diện Qt Designer XML
│   ├── MainWindow.hpp / .cpp     # Logic điều khiển giao diện & tương tác
│   ├── main.cpp                  # Điểm khởi chạy ứng dụng GUI
│   └── cli_test.cpp              # Ứng dụng dòng lệnh (CLI) chạy benchmark
├── sample_images/                # 8 Cảnh mẫu của bài báo + Fig 2 & Fig 3
└── results/                      # Thư mục chứa ảnh xuất ra
```

---

## 4. Hướng dẫn cài đặt & Biên dịch

### Yêu cầu hệ thống:
* C++ Compiler hỗ trợ C++17 (Clang trên macOS, GCC trên Linux, MSVC trên Windows).
* **CMake** $\ge$ 3.20.
* **Qt 6** (Widgets, Gui, Core).
* **OpenCV** $\ge$ 4.x / 5.x.

### Các bước biên dịch:

```bash
# 1. Di chuyển vào thư mục dự án
cd Test-Enhance-IR

# 2. Tạo thư mục build
mkdir -p build && cd build

# 3. Chạy CMake cấu hình
# (Trên macOS Homebrew: chỉ định CMAKE_PREFIX_PATH nếu cần)
cmake .. -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt;/opt/homebrew/opt/opencv" -DCMAKE_BUILD_TYPE=Release

# 4. Biên dịch dự án
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
```

---

## 5. Hướng dẫn sử dụng chi tiết

### 5.1. Khởi chạy Giao diện Đồ họa (GUI)

Bạn có thể chạy nhanh qua script:
```bash
./run_gui.sh
```
Hoặc chạy trực tiếp file nhị phân sau khi build:
```bash
./build/ir_enhance_gui
```

#### Quy trình thao tác trên UI:
1. **Mở ảnh hồng ngoại**:
   - Chọn nhanh một trong 8 cảnh mẫu của bài báo tại menu **Preset** (ví dụ `Scene 1`, `Scene 2`...). Khi chọn, chương trình sẽ tự động nạp cả ảnh mẫu đối chứng của tác giả (`_paper_ours.png`) vào Tab 3.
   - Hoặc bấm nút **"Open Custom..."** để nạp ảnh nhiệt bất kỳ từ máy tính của bạn.
2. **Tinh chỉnh tham số**:
   - Rê các thanh trượt `GF Radius`, `Regularization eps`, `NLM Strength`, `Dual-Gamma`, `Fusion Weight`.
   - Nếu tick chọn **Auto-update**, ảnh tăng cường và các chỉ số đo lường sẽ cập nhật ngay tức thì theo vị trí con trỏ chuột.
   - Bấm **"Defaults"** nếu muốn hoàn tác về cấu hình bài báo.
3. **Kiểm tra trực quan**:
   - Chuyển sang tab **"Before / After Comparison"**: Lăn chuột giữa để phóng to chi tiết các cành cây, mái ngói hoặc vạt cỏ; giữ chuột trái để kéo di chuyển ảnh. Cả hai ảnh sẽ di chuyển đồng bộ để bạn tiện soi chi tiết.
   - Chuyển sang tab **"Algorithm Pipeline"** để kiểm tra độ mịn của Base Layer, lượng nhiễu đã được khử ở NLM, và độ phân tách sáng tối của Dual-Gamma.
   - Chuyển sang tab **"Benchmarking vs Paper Ours"** để xem Heatmap sai số giữa thuật toán của bạn với ảnh của bài báo.
4. **Lưu kết quả**:
   - Bấm **"Save Output..."** để lưu ảnh đã tăng cường.
   - Bấm **"Export All Stages..."** để xuất tất cả 6 tầng trung gian.

---

### 5.2. Chạy Kiểm thử Dòng lệnh (CLI Benchmark)

Để kiểm thử tự động hàng loạt trên server hoặc terminal không cần mở GUI:
```bash
cd build
./ir_enhance_cli <đường_dẫn_ảnh_gốc> [<đường_dẫn_ảnh_tham_chiếu>]
```

Ví dụ chạy kiểm thử Scene 1:
```bash
./build/ir_enhance_cli sample_images/scene_1_original.png sample_images/scene_1_paper_ours.png
```
Kết quả hiển thị trên terminal:
```text
=== IR Image Detail Enhancement CLI Test ===
Loaded image: sample_images/scene_1_original.png (117x90)
Processing finished in: 18.6 ms

--- Metrics Comparison ---
Input  - IE: 5.983, GE: 5.232, RMS: 0.304
Output - IE: 5.660, GE: 5.002, RMS: 0.173
Vs Paper Target - PSNR: 13.14 dB, SSIM: 0.464

Saved enhanced output to: ../results/cli_out.png
```

---

## 6. Hướng dẫn tinh chỉnh tham số thực tế

| Tham số | Ý nghĩa | Mặc định | Tác động khi tăng / giảm |
|---|---|---|---|
| **Radius $r$** | Bán kính cửa sổ lọc Guided Filter | `2` | Tăng lên làm lớp nền mờ mịn hơn, đưa nhiều dải tần số thấp sang lớp chi tiết; giảm xuống giữ cấu trúc nền chi tiết hơn. |
| **Eps $\epsilon$** | Hệ số điều hòa Guided Filter | `0.040` | Tăng lên giúp bảo toàn mép cạnh tốt hơn trong vùng có độ biến thiên thấp. |
| **NLM $h$** | Độ mạnh khử nhiễu phi cục bộ | `10.0` | Tăng lên khử nhiễu hạt mạnh hơn nhưng nếu quá cao có thể làm mềm một số vân vi mô; đặt về `0` để tắt khử nhiễu. |
| **Gamma $\gamma$** | Hệ số mũ cân bằng sáng tối Dual-Gamma | `2.0` | Tăng lên giúp đẩy mạnh độ tương phản giữa các chi tiết tối và sáng trong lớp detail. |
| **Alpha $\alpha$** | Trọng số hòa trộn lớp Base / Detail | `0.60` | Tăng $\alpha$ làm ảnh nghiêng về độ mượt của lớp nền; giảm $\alpha$ làm các đường biên và vân chi tiết nổi khối mạnh hơn. |

---

## 7. Giấy phép (License)
Dự án được phát triển phục vụ mục đích nghiên cứu khoa học và học thuật.
