#pragma once
#include <opencv2/core.hpp>

namespace ir_enhance {

enum class PipelineMode {
    SymmetricDualGamma = 0,   // Corrected monotonic Dual-gamma with signed detail & 16-bit adaptive NLM
    PaperMagnitudeFusion = 1, // Monotonic Dual-gamma on |I_de| + convex fusion (reproduces Fig. 2c / Fig. 3c)
    PaperLiteralEq8 = 2       // Literal printed Eq. (8) & Eq. (9) from the paper for ablation/comparison
};

enum class DetailNormMode {
    MinMax,     // Min-Max normalization to [0, 1]
    Shift05,    // Shift by +0.5: clamp(I_de + 0.5, 0, 1)
    Symmetric   // Centered around 0.5 with scale: clamp(0.5 + 2 * I_de, 0, 1)
};

struct EnhanceParams {
    int guidedRadius = 2;            // Radius r (paper default: 2)
    float guidedEps = 0.04f;         // Regularization parameter eps (paper default: 0.04)
    float nlmH = 10.0f;              // Base NLM filtering strength h (0 = off)
    bool adaptiveNlm = true;         // Scale h adaptively by MAD noise estimate of raw detail
    int nlmTemplateSize = 7;         // Template patch size (odd)
    int nlmSearchSize = 21;          // Search window size (odd)
    float gamma = 2.0f;              // Dual-gamma exponent gamma (paper default: 2.0)
    float alpha = 0.6f;              // Fusion weight alpha (paper default: 0.6)
    PipelineMode mode = PipelineMode::SymmetricDualGamma;
    DetailNormMode normMode = DetailNormMode::MinMax; // Used in PaperLiteralEq8 mode
};

struct IntermediateStages {
    cv::Mat input8u;                 // Original / normalized 8-bit input image
    cv::Mat inputFloat;              // Normalized float input in [0, 1]
    cv::Mat baseLayer;               // Smoothed base layer I_base in [0, 1]
    cv::Mat rawDetail;               // Raw detail layer I - I_base
    cv::Mat normalizedDetail;        // Detail layer normalized to [0, 1]
    cv::Mat denoisedDetail;          // Denoised detail layer via NLM in [0, 1]
    cv::Mat enhancedDetail;          // Dual-gamma enhanced detail layer I'_de in [0, 1]
    cv::Mat fusedResult;             // Final fused 8-bit output image I_out
    double elapsedMs = 0.0;          // Execution time in milliseconds
};

struct ImageMetrics {
    double imageEntropy = 0.0;       // IE
    double gradientEntropy = 0.0;    // GE
    double rmsContrast = 0.0;        // RMS contrast (normalized std dev)
    double rmse = 0.0;               // Root Mean Square Error in [0, 1] (vs reference/input)
    double psnr = 0.0;               // PSNR in dB (vs reference/input)
    double ssim = 0.0;               // SSIM in [0, 1] (vs reference/input)
};

class EnhancementPipeline {
public:
    EnhancementPipeline() = default;

    /**
     * @brief Run the complete 5-step infrared image enhancement algorithm
     */
    static IntermediateStages process(const cv::Mat& input, const EnhanceParams& params);

    /**
     * @brief Compute image quality metrics (IE, GE, RMS Contrast, and RMSE/PSNR/SSIM if ref provided)
     * @param img Single-channel or color image (converted to 8-bit grayscale internally)
     * @param ref Optional reference image for RMSE, PSNR & SSIM (can be empty)
     */
    static ImageMetrics computeMetrics(const cv::Mat& img, const cv::Mat& ref = cv::Mat());

    /**
     * @brief Compute normalized RMSE in [0, 1] between two 8-bit single-channel images
     */
    static double computeRMSE(const cv::Mat& img1, const cv::Mat& img2);

    /**
     * @brief Compute SSIM between two 8-bit single-channel images
     */
    static double computeSSIM(const cv::Mat& img1, const cv::Mat& img2);

    /**
     * @brief Compute PSNR between two 8-bit single-channel images
     */
    static double computePSNR(const cv::Mat& img1, const cv::Mat& img2);
};

} // namespace ir_enhance
