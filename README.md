# Infrared Image Detail Enhancement (ICADI 2025)

Triển khai và kiểm thử thuật toán tăng cường chi tiết ảnh hồng ngoại (Infrared Image Detail Enhancement) dựa trên bài báo:
> **"Infrared Image Detail Enhancement Algorithm Based on Guided Filtering and Adaptive Dual-Gamma Nonlinear Mapping"**  
> *Yuxuan Zhang, Guanghui Li, Jianfeng Zhang, Fanbin Zeng, Yizhuo Wang, Guoliang Hu* (ICADI 2025, ACM).

---

## 1. Tóm tắt ý tưởng & Luồng xử lý thuật toán (Pipeline)

Quy trình thuật toán gồm 5 bước chính:
```text
           [Input Infrared Image I]
                      │
           ┌──────────┴──────────┐
           │                     │
           ▼                     │
[1. Guided Filtering]           │
           │                     │
           ▼                     ▼
     [Base Layer] ──( Trừ )──► [2. Raw Detail Layer]
        (I_base)                  (I_de = I - I_base)
           │                             │
           │                             ▼
           │                   [3. Non-Local Means]
           │                   (Adaptive Denoising)
           │                             │
           │                             ▼
           │                   [4. Dual-Gamma Mapping]
           │                   (Eq 8: Cân bằng sáng tối)
           │                             │
           │                             ▼
           │                    [Enhanced Detail Layer]
           │                             │
           └──────────┬──────────────────┘
                      │
                      ▼
             [5. Weighted Fusion]
        (Eq 9: I_out = α*I_base + (1-α)*I'_de)
                      │
                      ▼
           [Output Infrared Image]
```

### Các công thức cốt lõi:
1. **Guided Filtering (Phân rã ảnh)**:
   - Tách ảnh thành lớp nền $I_{base}$ (chứa cấu trúc tổng thể và độ sáng bức xạ nền) và lớp chi tiết $I_{de} = I - I_{base}$.
   - Tham số bài báo khuyến nghị: Bán kính $r = 2$ (cửa sổ $5 \times 5$), hệ số điều hòa $\epsilon = 0.04$.
2. **Khử nhiễu chi tiết (Adaptive Non-Local Means - NLM)**:
   - Lớp chi tiết sau khi tách chứa nhiều nhiễu cảm biến tần số cao.
   - NLM tìm kiếm các vùng tương đồng để triệt tiêu nhiễu mà không làm cùn mép cạnh.
   - Tham số: Độ mạnh lọc $h = 10.0$, template window $7 \times 7$, search window $21 \times 21$.
3. **Dual-$\gamma$ Mapping (Tăng cường phi tuyến cân bằng sáng tối)**:
   $$I'_{de} = \frac{1}{2} \left[ (I_{de})^{\frac{1}{\gamma}} + (1 - I_{de})^\gamma \right]$$
   - Vế $I_{de}^{1/\gamma}$ nâng sáng các chi tiết tối/yếu.
   - Vế $(1 - I_{de})^\gamma$ cân bằng các chi tiết vùng sáng, chống cháy sáng (over-enhancement).
   - Trung bình cộng giúp chuyển tiếp mượt mà, tự nhiên. Tham số bài báo: $\gamma = 2.0$.
4. **Hòa trộn có trọng số (Weighted Fusion)**:
   $$I_{out} = \alpha I_{base} + (1 - \alpha) I'_{de}$$
   - Tham số bài báo: $\alpha = 0.6$ (Base layer chiếm 60%, Enhanced detail chiếm 40%).
   - Tự nhiên bảo toàn dynamic range trong $[0, 1]$ (không bị tràn ngưỡng 8-bit).

---

## 2. Cấu trúc Project C++ / Qt6

```text
infrared_image_enhancement/
├── CMakeLists.txt              # Cấu hình build CMake C++17, Qt6, OpenCV
├── run_gui.sh                  # Script 1-click chạy UI
├── include/
│   ├── guided_filter.hpp       # Tự triển khai Guided Filter O(1) chuẩn He et al.
│   ├── enhancement_pipeline.hpp# Pipeline xử lý 5 bước & tính toán metrics
│   └── image_canvas.hpp        # Widget hiển thị ảnh Qt với Zoom/Pan đồng bộ
├── src/
│   ├── guided_filter.cpp
│   ├── enhancement_pipeline.cpp
│   └── image_canvas.cpp
├── apps/
│   ├── cli_test.cpp            # Benchmark CLI chạy hàng loạt qua terminal
│   ├── MainWindow.hpp          # Giao diện chính Qt6
│   ├── MainWindow.cpp
│   └── main.cpp
├── sample_images/              # 8 Scenes trích xuất từ bài báo + Fig 2 & Fig 3
└── results/                    # Thư mục lưu kết quả ảnh test
```

---

## 3. Cách chạy

### Cách 1: Mở giao diện đồ họa (GUI)
```bash
cd /Users/soncris/nckh2026/infrared_image_enhancement
./run_gui.sh
```
*(hoặc `cd /Users/soncris/NCKH2627/infrared_image_enhancement && ./run_gui.sh`)*

### Cách 2: Chạy benchmark CLI
```bash
cd /Users/soncris/nckh2026/infrared_image_enhancement/build
./ir_enhance_cli ../sample_images/scene_1_original.png ../sample_images/scene_1_paper_ours.png
```

---

## 4. Tính năng trên UI
1. **Side-by-Side Comparison**: Xem ảnh gốc và ảnh sau tăng cường cạnh nhau, chuột cuộn Zoom và kéo Pan đồng bộ 2 bên.
2. **Algorithm Pipeline Tab**: Xem trực tiếp kết quả của cả 6 giai đoạn (Ảnh gốc $\to$ Base $\to$ Detail $\to$ Denoised $\to$ Dual-Gamma $\to$ Fused).
3. **Benchmarking Tab**: So sánh trực tiếp kết quả triển khai với ảnh kết quả mẫu của nhóm tác giả trong bài báo kèm Heatmap sai khác.
4. **Điều chỉnh tham số thời gian thực (Real-time Slider)**:
   - Bán kính $r$ và $\epsilon$ của Guided Filter
   - Độ mạnh khử nhiễu $h$ của NLM
   - Tham số $\gamma$ của Dual-Gamma
   - Trọng số hòa trộn $\alpha$
   - Chế độ chuẩn hóa Detail Layer (Min-Max, Shift +0.5, Symmetric)
5. **Đo đạc chỉ số khách quan**: Image Entropy (IE), Gradient Entropy (GE), RMS Contrast, PSNR & SSIM.
6. **Lưu & Xuất**: Nút lưu ảnh đơn lẻ hoặc xuất toàn bộ 6 bước trung gian ra đĩa.
