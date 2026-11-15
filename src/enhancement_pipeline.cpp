#include "enhancement_pipeline.hpp"
#include "guided_filter.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>
#include <chrono>
#include <cmath>
#include <numeric>

namespace ir_enhance {

IntermediateStages EnhancementPipeline::process(const cv::Mat& input, const EnhanceParams& params) {
    auto startTime = std::chrono::high_resolution_clock::now();
    IntermediateStages stages;

    // 1. Prepare 8-bit single-channel input
    cv::Mat gray;
    if (input.channels() == 3 || input.channels() == 4) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = input.clone();
    }
    if (gray.depth() != CV_8U) {
        gray.convertTo(gray, CV_8U);
    }
    stages.input8u = gray;

    // Convert to CV_32F in [0, 1]
    cv::Mat I;
    gray.convertTo(I, CV_32F, 1.0 / 255.0);
    stages.inputFloat = I;

    // 2. Step 1: Guided Image Filtering -> Base Layer (Section 3.1)
    stages.baseLayer = selfGuidedFilter(I, params.guidedRadius, params.guidedEps);

    // 3. Step 2: Detail Layer Extraction (Section 3.2)
    stages.rawDetail = I - stages.baseLayer;

    // Normalization to [0, 1]
    cv::Mat normDetail;
    if (params.normMode == DetailNormMode::MinMax) {
        double minVal = 0.0, maxVal = 0.0;
        cv::minMaxLoc(stages.rawDetail, &minVal, &maxVal);
        double range = maxVal - minVal;
        if (range < 1e-7) range = 1e-7;
        normDetail = (stages.rawDetail - static_cast<float>(minVal)) / static_cast<float>(range);
    } else if (params.normMode == DetailNormMode::Shift05) {
        normDetail = stages.rawDetail + 0.5f;
    } else { // Symmetric
        normDetail = 0.5f + 2.0f * stages.rawDetail;
    }
    cv::min(cv::max(normDetail, 0.0f), 1.0f, normDetail);
    stages.normalizedDetail = normDetail;

    // 4. Step 3: Adaptive Non-Local Means (NLM) Filter for Noise Suppression (Section 3.2)
    if (params.nlmH > 0.0f) {
        cv::Mat detail8u;
        normDetail.convertTo(detail8u, CV_8U, 255.0);
        cv::Mat denoised8u;
        cv::fastNlMeansDenoising(detail8u, denoised8u, params.nlmH, params.nlmTemplateSize, params.nlmSearchSize);
        denoised8u.convertTo(stages.denoisedDetail, CV_32F, 1.0 / 255.0);
    } else {
        stages.denoisedDetail = normDetail.clone();
    }

    // 5. Step 4: Dual-Gamma Nonlinear Mapping (Section 3.3, Equation 8)
    // I'_de = 0.5 * [ (I_de)^(1/gamma) + (1 - I_de)^gamma ]
    cv::Mat term1, term2;
    float invGamma = 1.0f / (params.gamma > 0.01f ? params.gamma : 1.0f);
    cv::pow(cv::max(stages.denoisedDetail, 0.0f), invGamma, term1);
    cv::pow(cv::max(1.0f - stages.denoisedDetail, 0.0f), params.gamma, term2);
    stages.enhancedDetail = 0.5f * (term1 + term2);

    // 6. Step 5: Image Fusion (Section 3.4, Equation 9)
    // I_out = alpha * I_base + (1 - alpha) * I'_de
    cv::Mat fusedFloat = params.alpha * stages.baseLayer + (1.0f - params.alpha) * stages.enhancedDetail;
    cv::min(cv::max(fusedFloat, 0.0f), 1.0f, fusedFloat);
    fusedFloat.convertTo(stages.fusedResult, CV_8U, 255.0);

    auto endTime = std::chrono::high_resolution_clock::now();
    stages.elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return stages;
}

ImageMetrics EnhancementPipeline::computeMetrics(const cv::Mat& img, const cv::Mat& ref) {
    ImageMetrics metrics;
    if (img.empty()) return metrics;

    cv::Mat gray = img;
    if (gray.channels() > 1) {
        cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
    }

    // 1. Image Entropy (IE): -sum(p * log2(p))
    int histSize = 256;
    float range[] = {0, 256};
    const float* histRange = {range};
    cv::Mat hist;
    cv::calcHist(&gray, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange, true, false);

    double totalPixels = static_cast<double>(gray.total());
    double entropy = 0.0;
    for (int i = 0; i < 256; ++i) {
        float count = hist.at<float>(i);
        if (count > 0.0f) {
            double p = count / totalPixels;
            entropy -= p * (std::log(p) / std::log(2.0));
        }
    }
    metrics.imageEntropy = entropy;

    // 2. RMS Contrast (Standard deviation of pixel intensities)
    cv::Scalar meanVal, stdDev;
    cv::meanStdDev(gray, meanVal, stdDev);
    metrics.rmsContrast = stdDev[0] / 255.0; // Normalized RMS in [0, 1]

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
        gradMag.convertTo(grad8u, CV_8U, 255.0 / (maxGrad - minGrad), -minGrad * 255.0 / (maxGrad - minGrad));
        cv::Mat gradHist;
        cv::calcHist(&grad8u, 1, 0, cv::Mat(), gradHist, 1, &histSize, &histRange, true, false);
        double ge = 0.0;
        for (int i = 0; i < 256; ++i) {
            float count = gradHist.at<float>(i);
            if (count > 0.0f) {
                double p = count / totalPixels;
                ge -= p * (std::log(p) / std::log(2.0));
            }
        }
        metrics.gradientEntropy = ge;
    }

    // 4. PSNR and SSIM if reference image provided
    if (!ref.empty() && ref.size() == gray.size()) {
        cv::Mat refGray = ref;
        if (refGray.channels() > 1) {
            cv::cvtColor(refGray, refGray, cv::COLOR_BGR2GRAY);
        }
        metrics.psnr = computePSNR(gray, refGray);
        metrics.ssim = computeSSIM(gray, refGray);
    }

    return metrics;
}

double EnhancementPipeline::computePSNR(const cv::Mat& img1, const cv::Mat& img2) {
    cv::Mat diff;
    cv::absdiff(img1, img2, diff);
    diff.convertTo(diff, CV_32F);
    diff = diff.mul(diff);

    cv::Scalar s = cv::sum(diff);
    double sse = s[0];
    if (sse <= 1e-10) return 100.0; // Almost identical

    double mse = sse / static_cast<double>(img1.total());
    return 10.0 * std::log10((255.0 * 255.0) / mse);
}

double EnhancementPipeline::computeSSIM(const cv::Mat& img1, const cv::Mat& img2) {
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

    cv::Scalar mssim = cv::mean(ssim_map);
    return mssim[0];
}

} // namespace ir_enhance
