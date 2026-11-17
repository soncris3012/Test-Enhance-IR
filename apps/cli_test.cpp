#include "enhancement_pipeline.hpp"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string locateSampleDir(const char* argv0) {
    std::vector<fs::path> starts = {fs::current_path()};
    if (argv0 && *argv0) {
        std::error_code ec;
        const fs::path exeDir = fs::weakly_canonical(fs::path(argv0), ec).parent_path();
        if (!exeDir.empty()) {
            starts.push_back(exeDir);
        }
    }
    for (const fs::path& base : starts) {
        fs::path cur = base;
        for (int depth = 0; depth <= 3; ++depth) {
            const fs::path candidate = cur / "sample_images";
            if (fs::exists(candidate) && fs::is_directory(candidate)) {
                return candidate.string();
            }
            if (!cur.has_parent_path() || cur == cur.parent_path()) break;
            cur = cur.parent_path();
        }
    }
    return "sample_images";
}

} // namespace

int main(int argc, char** argv) {
    std::cout << "=== IR Image Detail Enhancement CLI Test ===" << std::endl;

    ir_enhance::EnhanceParams params;
    params.guidedRadius = 2;
    params.guidedEps = 0.04f;
    params.nlmH = 10.0f;
    params.adaptiveNlm = true;
    params.gamma = 2.0f;
    params.alpha = 0.6f;
    params.mode = ir_enhance::PipelineMode::SymmetricDualGamma;
    params.normMode = ir_enhance::DetailNormMode::MinMax;

    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) {
            const std::string modeStr = argv[++i];
            if (modeStr == "symmetric") {
                params.mode = ir_enhance::PipelineMode::SymmetricDualGamma;
            } else if (modeStr == "magnitude") {
                params.mode = ir_enhance::PipelineMode::PaperMagnitudeFusion;
            } else if (modeStr == "literal") {
                params.mode = ir_enhance::PipelineMode::PaperLiteralEq8;
            } else {
                std::cerr << "Unknown mode: " << modeStr
                          << " (expected: symmetric | magnitude | literal)" << std::endl;
                return 1;
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0]
                      << " [--mode symmetric|magnitude|literal] [<input_image> [<ref_image>]]"
                      << std::endl;
            return 0;
        } else {
            positional.push_back(arg);
        }
    }

    const std::string sampleDir = locateSampleDir(argc > 0 ? argv[0] : nullptr);
    std::string testPath = (fs::path(sampleDir) / "scene_1_original.png").string();
    std::string refPath = (fs::path(sampleDir) / "scene_1_paper_ours.png").string();

    if (!positional.empty()) {
        testPath = positional[0];
        refPath.clear();
    }
    if (positional.size() >= 2) {
        refPath = positional[1];
    }

    cv::Mat input = cv::imread(testPath, cv::IMREAD_ANYDEPTH | cv::IMREAD_ANYCOLOR);
    if (input.empty()) {
        std::cerr << "Failed to read input image: " << testPath << std::endl;
        return 1;
    }

    cv::Mat ref;
    if (!refPath.empty() && fs::exists(refPath)) {
        ref = cv::imread(refPath, cv::IMREAD_ANYDEPTH | cv::IMREAD_ANYCOLOR);
    }

    std::cout << "Loaded image: " << testPath << " (" << input.cols << "x" << input.rows << ")" << std::endl;

    const auto stages = ir_enhance::EnhancementPipeline::process(input, params);
    std::cout << "Processing finished in: " << stages.elapsedMs << " ms" << std::endl;

    const auto metricsIn = ir_enhance::EnhancementPipeline::computeMetrics(stages.input8u);
    const auto metricsVsIn = ir_enhance::EnhancementPipeline::computeMetrics(stages.fusedResult, stages.input8u);

    std::cout << "\n--- Metrics Comparison ---" << std::endl;
    std::cout << "Input  - IE: " << metricsIn.imageEntropy
              << ", GE: " << metricsIn.gradientEntropy
              << ", RMS Contrast: " << metricsIn.rmsContrast << std::endl;
    std::cout << "Output - IE: " << metricsVsIn.imageEntropy
              << ", GE: " << metricsVsIn.gradientEntropy
              << ", RMS Contrast: " << metricsVsIn.rmsContrast << std::endl;
    std::cout << "Vs Input        - RMSE: " << metricsVsIn.rmse
              << ", PSNR: " << metricsVsIn.psnr
              << " dB, SSIM: " << metricsVsIn.ssim << std::endl;

    if (!ref.empty() && ref.size() == stages.fusedResult.size()) {
        const auto metricsVsRef = ir_enhance::EnhancementPipeline::computeMetrics(stages.fusedResult, ref);
        std::cout << "Vs Paper Target - RMSE: " << metricsVsRef.rmse
                  << ", PSNR: " << metricsVsRef.psnr
                  << " dB, SSIM: " << metricsVsRef.ssim << std::endl;
    }

    std::string outPath = "cli_out.png";
    if (fs::exists("results") && fs::is_directory("results")) {
        outPath = "results/cli_out.png";
    } else if (fs::exists("../results") && fs::is_directory("../results")) {
        outPath = "../results/cli_out.png";
    }
    if (cv::imwrite(outPath, stages.fusedResult)) {
        std::cout << "\nSaved enhanced output to: " << outPath << std::endl;
    } else {
        std::cerr << "\nWarning: failed to save enhanced output to: " << outPath << std::endl;
    }

    return 0;
}
