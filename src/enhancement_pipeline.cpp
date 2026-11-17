#include "enhancement_pipeline.hpp"
#include "guided_filter.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

namespace ir_enhance {

namespace {

cv::Mat toSingleChannel8u(const cv::Mat& src, cv::Mat* outFloat01 = nullptr) {
    if (src.empty()) {
        if (outFloat01) *outFloat01 = cv::Mat();
        return cv::Mat();
    }

    cv::Mat gray;
    if (src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    } else if (src.channels() == 4) {
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
    } else {
        gray = src;
    }

    if (gray.depth() == CV_8U) {
        cv::Mat u8 = gray.clone();
        if (outFloat01) {
            u8.convertTo(*outFloat01, CV_32F, 1.0 / 255.0);
        }
        return u8;
    }

    // 16-bit thermal TIFF or floating-point input: min-max scale to [0, 1] without clipping >255
    cv::Mat f32;
    gray.convertTo(f32, CV_32F);
    double minVal = 0.0, maxVal = 0.0;
    cv::minMaxLoc(f32, &minVal, &maxVal);

    cv::Mat norm01;
    if (maxVal - minVal > 1e-6) {
        norm01 = (f32 - static_cast<float>(minVal)) / static_cast<float>(maxVal - minVal);
    } else if (maxVal > 1.0) {
        norm01 = cv::Mat::zeros(f32.size(), CV_32F);
    } else {
        cv::min(cv::max(f32, 0.0f), 1.0f, norm01);
    }

    cv::Mat u8;
    norm01.convertTo(u8, CV_8U, 255.0);
    if (outFloat01) {
        *outFloat01 = norm01;
    }
    return u8;
}

float computeMADSigma(const cv::Mat& mat) {
    if (mat.empty()) return 0.0f;
    std::vector<float> vals;
    vals.reserve(mat.total());
    for (int r = 0; r < mat.rows; ++r) {
        const float* ptr = mat.ptr<float>(r);
        vals.insert(vals.end(), ptr, ptr + mat.cols);
    }
    if (vals.empty()) return 0.0f;

    const size_t mid = vals.size() / 2;
    std::nth_element(vals.begin(), vals.begin() + mid, vals.end());
    const float median = vals[mid];

    for (float& v : vals) {
        v = std::abs(v - median);
    }
    std::nth_element(vals.begin(), vals.begin() + mid, vals.end());
    return 1.4826f * vals[mid];
}

cv::Mat denoise16BitNLM(const cv::Mat& unit01, float h, int templateSize, int searchSize) {
    const int minDim = std::min(unit01.rows, unit01.cols);
    if (minDim < 5 || h <= 0.0f) {
        return unit01.clone();
    }

    const int maxOddDim = (minDim % 2 == 1) ? minDim : (minDim - 1);
    int tpl = std::max(3, templateSize | 1);
    tpl = std::min(tpl, maxOddDim);
    int srch = std::max(tpl, searchSize | 1);
    srch = std::min(srch, maxOddDim);

    cv::Mat clamped;
    cv::min(cv::max(unit01, 0.0f), 1.0f, clamped);

    cv::Mat detail16u;
    clamped.convertTo(detail16u, CV_16UC1, 65535.0);

    cv::Mat denoised16u;
    // OpenCV 16-bit NLM uses NORM_L1; ~140x matches 8-bit NORM_L2 strength h while keeping 16-bit precision
    const std::vector<float> hVec = {h * 140.0f};
    cv::fastNlMeansDenoising(detail16u, denoised16u, hVec, tpl, srch, cv::NORM_L1);

    cv::Mat out;
    denoised16u.convertTo(out, CV_32F, 1.0 / 65535.0);
    return out;
}

} // namespace

IntermediateStages EnhancementPipeline::process(const cv::Mat& input, const EnhanceParams& params) {
    auto startTime = std::chrono::high_resolution_clock::now();
    IntermediateStages stages;

    if (input.empty()) {
        return stages;
    }

    // 1. Prepare single-channel 8-bit and [0, 1] float input
    cv::Mat I;
    stages.input8u = toSingleChannel8u(input, &I);
    stages.inputFloat = I;

    // 2. Guided Image Filtering -> Base Layer (Section 3.1)
    stages.baseLayer = selfGuidedFilter(I, params.guidedRadius, params.guidedEps);

    // 3. Detail Layer Extraction (Section 3.2)
    stages.rawDetail = I - stages.baseLayer;

    double minRaw = 0.0, maxRaw = 0.0;
    cv::minMaxLoc(stages.rawDetail, &minRaw, &maxRaw);
    const float maxAbs = std::max({static_cast<float>(std::abs(minRaw)),
                                   static_cast<float>(std::abs(maxRaw)),
                                   1e-5f});

    // Normalize detail layer to [0, 1]
    cv::Mat normDetail;
    if (params.mode == PipelineMode::PaperLiteralEq8) {
        if (params.normMode == DetailNormMode::MinMax) {
            double range = maxRaw - minRaw;
            if (range < 1e-7) range = 1e-7;
            normDetail = (stages.rawDetail - static_cast<float>(minRaw)) / static_cast<float>(range);
        } else if (params.normMode == DetailNormMode::Shift05) {
            normDetail = stages.rawDetail + 0.5f;
        } else {
            normDetail = 0.5f + 2.0f * stages.rawDetail;
        }
    } else {
        // Symmetric zero-preserving normalization into [0, 1] centered at 0.5
        normDetail = 0.5f + 0.5f * (stages.rawDetail / maxAbs);
    }
    cv::min(cv::max(normDetail, 0.0f), 1.0f, normDetail);
    stages.normalizedDetail = normDetail;

    // 4. Adaptive 16-bit Non-Local Means (NLM) Filter (Section 3.2)
    if (params.nlmH > 0.0f) {
        float effectiveH = params.nlmH;
        if (params.adaptiveNlm) {
            // Compute MAD noise sigma in [0, 1] normalized detail space
            const float sigmaMad = computeMADSigma(normDetail);
            const float adaptiveScale = std::clamp(sigmaMad / 0.035f, 0.4f, 1.5f);
            effectiveH *= adaptiveScale;
        }
        stages.denoisedDetail = denoise16BitNLM(normDetail, effectiveH, params.nlmTemplateSize, params.nlmSearchSize);
    } else {
        stages.denoisedDetail = normDetail.clone();
    }

    // 5. Dual-Gamma Nonlinear Mapping & Fusion (Sections 3.3 & 3.4)
    const float gamma = std::max(params.gamma, 0.1f);
    const float invGamma = 1.0f / gamma;
    const float alpha = std::clamp(params.alpha, 0.0f, 1.0f);

    cv::Mat fusedFloat;
    if (params.mode == PipelineMode::SymmetricDualGamma) {
        // Recover signed clean detail in physical range
        cv::Mat cleanDetail = (stages.denoisedDetail - 0.5f) * (2.0f * maxAbs);
        stages.enhancedDetail = cv::Mat(I.size(), CV_32F);
        cv::Mat signedEnh(I.size(), CV_32F);

        for (int r = 0; r < I.rows; ++r) {
            const float* src = cleanDetail.ptr<float>(r);
            float* magOut = stages.enhancedDetail.ptr<float>(r);
            float* sigOut = signedEnh.ptr<float>(r);
            for (int c = 0; c < I.cols; ++c) {
                const float d = src[c];
                const float mag = std::clamp(std::abs(d) / maxAbs, 0.0f, 1.0f);
                const float enh = 0.5f * (std::pow(mag, invGamma) + 1.0f - std::pow(1.0f - mag, gamma));
                magOut[c] = enh;
                sigOut[c] = (d >= 0.0f ? 1.0f : -1.0f) * enh * maxAbs;
            }
        }
        // Detail-preserving fusion: at paper alpha=0.6 -> baseLayer + 1.2 * signedEnh
        fusedFloat = stages.baseLayer + (3.0f * (1.0f - alpha)) * signedEnh;
    } else if (params.mode == PipelineMode::PaperMagnitudeFusion) {
        cv::Mat cleanDetail = (stages.denoisedDetail - 0.5f) * (2.0f * maxAbs);
        stages.enhancedDetail = cv::Mat(I.size(), CV_32F);
        for (int r = 0; r < I.rows; ++r) {
            const float* src = cleanDetail.ptr<float>(r);
            float* out = stages.enhancedDetail.ptr<float>(r);
            for (int c = 0; c < I.cols; ++c) {
                const float mag = std::clamp(std::abs(src[c]), 0.0f, 1.0f);
                out[c] = 0.5f * (std::pow(mag, invGamma) + 1.0f - std::pow(1.0f - mag, gamma));
            }
        }
        fusedFloat = alpha * stages.baseLayer + (1.0f - alpha) * stages.enhancedDetail;
    } else {
        // Literal printed Eq. (8): 0.5 * [I_de^(1/gamma) + (1 - I_de)^gamma] & Eq. (9)
        cv::Mat term1, term2;
        cv::pow(cv::max(stages.denoisedDetail, 0.0f), invGamma, term1);
        cv::pow(cv::max(1.0f - stages.denoisedDetail, 0.0f), gamma, term2);
        stages.enhancedDetail = 0.5f * (term1 + term2);
        fusedFloat = alpha * stages.baseLayer + (1.0f - alpha) * stages.enhancedDetail;
    }

    cv::min(cv::max(fusedFloat, 0.0f), 1.0f, fusedFloat);
    fusedFloat.convertTo(stages.fusedResult, CV_8U, 255.0);

    auto endTime = std::chrono::high_resolution_clock::now();
    stages.elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return stages;
}

ImageMetrics EnhancementPipeline::computeMetrics(const cv::Mat& img, const cv::Mat& ref) {
    ImageMetrics metrics;
    if (img.empty()) return metrics;

    cv::Mat gray = toSingleChannel8u(img);

    // 1. Image Entropy (IE): -sum(p * log2(p))
    const int histSize = 256;
    const float range[] = {0, 256};
    const float* histRange = {range};
    cv::Mat hist;
    cv::calcHist(&gray, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange, true, false);

    const double totalPixels = static_cast<double>(gray.total());
    double entropy = 0.0;
    for (int i = 0; i < 256; ++i) {
        const float count = hist.at<float>(i);
        if (count > 0.0f) {
            const double p = count / totalPixels;
            entropy -= p * (std::log(p) / std::log(2.0));
        }
    }
    metrics.imageEntropy = entropy;

    // 2. RMS Contrast (normalized std dev of pixel intensities)
    cv::Scalar meanVal, stdDev;
    cv::meanStdDev(gray, meanVal, stdDev);
    metrics.rmsContrast = stdDev[0] / 255.0;

    // 3. Gradient Entropy (GE)
    cv::Mat gradX, gradY;
    cv::Sobel(gray, gradX, CV_32F, 1, 0, 3);
    cv::Sobel(gray, gradY, CV_32F, 0, 1, 3);
    cv::Mat gradMag;
    cv::magnitude(gradX, gradY, gradMag);

    double minGrad = 0.0, maxGrad = 0.0;
    cv::minMaxLoc(gradMag, &minGrad, &maxGrad);
    if (maxGrad - minGrad > 1e-5) {
        cv::Mat grad8u;
        gradMag.convertTo(grad8u, CV_8U, 255.0 / (maxGrad - minGrad),
                          -minGrad * 255.0 / (maxGrad - minGrad));
        cv::Mat gradHist;
        cv::calcHist(&grad8u, 1, 0, cv::Mat(), gradHist, 1, &histSize, &histRange, true, false);
        double ge = 0.0;
        for (int i = 0; i < 256; ++i) {
            const float count = gradHist.at<float>(i);
            if (count > 0.0f) {
                const double p = count / totalPixels;
                ge -= p * (std::log(p) / std::log(2.0));
            }
        }
        metrics.gradientEntropy = ge;
    }

    // 4. RMSE, PSNR, and SSIM if reference image provided
    if (!ref.empty() && ref.size() == gray.size()) {
        cv::Mat refGray = toSingleChannel8u(ref);
        metrics.rmse = computeRMSE(gray, refGray);
        metrics.psnr = computePSNR(gray, refGray);
        metrics.ssim = computeSSIM(gray, refGray);
    }

    return metrics;
}

double EnhancementPipeline::computeRMSE(const cv::Mat& img1, const cv::Mat& img2) {
    if (img1.empty() || img1.size() != img2.size()) return 0.0;
    cv::Mat diff;
    cv::absdiff(img1, img2, diff);
    diff.convertTo(diff, CV_32F);
    diff = diff.mul(diff);
    const double mse = cv::sum(diff)[0] / static_cast<double>(img1.total());
    return std::sqrt(mse) / 255.0;
}

double EnhancementPipeline::computePSNR(const cv::Mat& img1, const cv::Mat& img2) {
    if (img1.empty() || img1.size() != img2.size()) return 0.0;
    cv::Mat diff;
    cv::absdiff(img1, img2, diff);
    diff.convertTo(diff, CV_32F);
    diff = diff.mul(diff);

    const double sse = cv::sum(diff)[0];
    if (sse <= 1e-10) return 100.0;

    const double mse = sse / static_cast<double>(img1.total());
    return 10.0 * std::log10((255.0 * 255.0) / mse);
}

double EnhancementPipeline::computeSSIM(const cv::Mat& img1, const cv::Mat& img2) {
    if (img1.empty() || img1.size() != img2.size()) return 0.0;
    const double C1 = 6.5025, C2 = 58.5225; // (0.01*255)^2, (0.03*255)^2

    cv::Mat I1, I2;
    img1.convertTo(I1, CV_32F);
    img2.convertTo(I2, CV_32F);

    cv::Mat I1_2 = I1.mul(I1);
    cv::Mat I2_2 = I2.mul(I2);
    cv::Mat I1_I2 = I1.mul(I2);

    cv::Mat mu1, mu2;
    cv::GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);

    cv::Mat mu1_2 = mu1.mul(mu1);
    cv::Mat mu2_2 = mu2.mul(mu2);
    cv::Mat mu1_mu2 = mu1.mul(mu2);

    cv::Mat sigma1_2, sigma2_2, sigma12;
    cv::GaussianBlur(I1_2, sigma1_2, cv::Size(11, 11), 1.5);
    sigma1_2 -= mu1_2;

    cv::GaussianBlur(I2_2, sigma2_2, cv::Size(11, 11), 1.5);
    sigma2_2 -= mu2_2;

    cv::GaussianBlur(I1_I2, sigma12, cv::Size(11, 11), 1.5);
    sigma12 -= mu1_mu2;

    cv::Mat t1 = 2.0 * mu1_mu2 + C1;
    cv::Mat t2 = 2.0 * sigma12 + C2;
    cv::Mat t3 = t1.mul(t2);

    t1 = mu1_2 + mu2_2 + C1;
    t2 = sigma1_2 + sigma2_2 + C2;
    cv::Mat t4 = t1.mul(t2);

    cv::Mat ssim_map;
    cv::divide(t3, t4, ssim_map);

    return cv::mean(ssim_map)[0];
}

} // namespace ir_enhance
