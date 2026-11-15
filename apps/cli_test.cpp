#include "enhancement_pipeline.hpp"
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <filesystem>

int main(int argc, char** argv) {
    std::cout << "=== IR Image Detail Enhancement CLI Test ===" << std::endl;

    std::string sampleDir = "/Users/soncris/NCKH2627/infrared_image_enhancement/sample_images";
    if (!std::filesystem::exists(sampleDir)) {
        sampleDir = "../sample_images";
    }
    std::string testPath = sampleDir + "/scene_1_original.png";
    std::string refPath = sampleDir + "/scene_1_paper_ours.png";

    if (argc > 1) {
        testPath = argv[1];
    }
    if (argc > 2) {
        refPath = argv[2];
    }

    cv::Mat input = cv::imread(testPath, cv::IMREAD_GRAYSCALE);
    if (input.empty()) {
        std::cerr << "Failed to read input image: " << testPath << std::endl;
        return 1;
    }
    cv::Mat ref = cv::imread(refPath, cv::IMREAD_GRAYSCALE);

    std::cout << "Loaded image: " << testPath << " (" << input.cols << "x" << input.rows << ")" << std::endl;

    ir_enhance::EnhanceParams params;
    params.guidedRadius = 2;
    params.guidedEps = 0.04f;
    params.nlmH = 10.0f;
    params.gamma = 2.0f;
    params.alpha = 0.6f;
    params.normMode = ir_enhance::DetailNormMode::MinMax;

    auto stages = ir_enhance::EnhancementPipeline::process(input, params);
    std::cout << "Processing finished in: " << stages.elapsedMs << " ms" << std::endl;

    auto metricsIn = ir_enhance::EnhancementPipeline::computeMetrics(input);
    auto metricsOut = ir_enhance::EnhancementPipeline::computeMetrics(stages.fusedResult, ref);

    std::cout << "\n--- Metrics Comparison ---" << std::endl;
    std::cout << "Input  - IE: " << metricsIn.imageEntropy 
              << ", GE: " << metricsIn.gradientEntropy 
              << ", RMS: " << metricsIn.rmsContrast << std::endl;
    std::cout << "Output - IE: " << metricsOut.imageEntropy 
              << ", GE: " << metricsOut.gradientEntropy 
              << ", RMS: " << metricsOut.rmsContrast << std::endl;
    if (!ref.empty()) {
        std::cout << "Vs Paper Target - PSNR: " << metricsOut.psnr 
                  << " dB, SSIM: " << metricsOut.ssim << std::endl;
    }

    std::string outPath = "cli_out.png";
    if (std::filesystem::exists("../results")) {
        outPath = "../results/cli_out.png";
    }
    cv::imwrite(outPath, stages.fusedResult);
    std::cout << "\nSaved enhanced output to: " << outPath << std::endl;

    return 0;
}
