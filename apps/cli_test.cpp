#include "enhancement_pipeline.hpp"
#include "dataset_manager.hpp"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <numeric>

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
        for (int depth = 0; depth <= 4; ++depth) {
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

void printHelp(const char* prog) {
    std::cout << "Usage: " << prog << " [options] [image_path [reference_path]]\n\n"
              << "Options:\n"
              << "  --mode <symmetric|magnitude|literal>   Pipeline mode (default: symmetric)\n"
              << "  --dataset <id>                         Benchmark dataset (llvip_test, flir_val, msrs_test, etc.)\n"
              << "  --dataset-root <path>                  Root dataset directory (e.g. ~/QART_Datasets/raw)\n"
              << "  --dir <path>                           Run batch evaluation on a custom image directory\n"
              << "  --limit <N>                            Limit number of images in batch evaluation\n"
              << "  --out-dir <path>                       Save enhanced images & CSV report to directory\n"
              << "  --list-datasets                        List all detected benchmark datasets and exit\n"
              << "  -h, --help                             Show this help message\n";
}

} // namespace

int main(int argc, char** argv) {
    std::cout << "=== IR Image Detail Enhancement CLI Testbed ===" << std::endl;

    ir_enhance::EnhanceParams params;
    params.guidedRadius = 2;
    params.guidedEps = 0.04f;
    params.nlmH = 10.0f;
    params.adaptiveNlm = true;
    params.gamma = 2.0f;
    params.alpha = 0.6f;
    params.mode = ir_enhance::PipelineMode::SymmetricDualGamma;
    params.normMode = ir_enhance::DetailNormMode::MinMax;

    std::string datasetId;
    std::string datasetRoot;
    std::string customDir;
    std::string outDir;
    int limit = -1;
    bool listOnly = false;

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
                std::cerr << "Unknown mode: " << modeStr << " (expected: symmetric | magnitude | literal)" << std::endl;
                return 1;
            }
        } else if (arg == "--dataset" && i + 1 < argc) {
            datasetId = argv[++i];
        } else if (arg == "--dataset-root" && i + 1 < argc) {
            datasetRoot = argv[++i];
        } else if (arg == "--dir" && i + 1 < argc) {
            customDir = argv[++i];
        } else if (arg == "--out-dir" && i + 1 < argc) {
            outDir = argv[++i];
        } else if (arg == "--limit" && i + 1 < argc) {
            limit = std::stoi(argv[++i]);
        } else if (arg == "--list-datasets") {
            listOnly = true;
        } else if (arg == "-h" || arg == "--help") {
            printHelp(argv[0]);
            return 0;
        } else {
            positional.push_back(arg);
        }
    }

    const std::string sampleDir = locateSampleDir(argc > 0 ? argv[0] : nullptr);
    const std::string detectedRoot = ir_enhance::DatasetManager::detectDatasetRoot(datasetRoot);
    auto discovered = ir_enhance::DatasetManager::discoverDatasets(detectedRoot, sampleDir);

    if (listOnly) {
        std::cout << "Dataset Root: " << (detectedRoot.empty() ? "(none)" : detectedRoot) << "\n";
        std::cout << "Discovered Datasets:\n";
        for (const auto& d : discovered) {
            std::cout << "  [" << d.id << "] " << d.displayName << "\n"
                      << "       Path: " << d.folderPath << " (" << d.imageCount << " images)\n";
        }
        return 0;
    }

    // Determine if batch evaluation mode
    std::string batchDir = customDir;
    if (batchDir.empty() && !datasetId.empty()) {
        for (const auto& d : discovered) {
            if (d.id == datasetId || d.displayName.find(datasetId) != std::string::npos) {
                batchDir = d.folderPath;
                break;
            }
        }
        if (batchDir.empty()) {
            std::cerr << "Error: dataset id '" << datasetId << "' not found. Available datasets:\n";
            for (const auto& d : discovered) {
                std::cerr << "  " << d.id << " (" << d.displayName << ")\n";
            }
            return 1;
        }
    }

    // BATCH MODE
    if (!batchDir.empty()) {
        std::cout << "Running Batch Evaluation on directory: " << batchDir << std::endl;
        auto files = ir_enhance::DatasetManager::scanImageFiles(batchDir, limit);
        if (files.empty()) {
            std::cerr << "No images found in: " << batchDir << std::endl;
            return 1;
        }
        std::cout << "Evaluating " << files.size() << " images..." << std::endl;

        if (!outDir.empty()) {
            fs::create_directories(outDir);
        }

        std::vector<double> times, ieIn, ieOut, geIn, geOut, rmsIn, rmsOut;
        std::ofstream csv;
        if (!outDir.empty()) {
            csv.open((fs::path(outDir) / "benchmark_report.csv").string());
            csv << "filename,time_ms,entropy_in,entropy_out,ge_in,ge_out,rms_in,rms_out\n";
        }

        for (size_t i = 0; i < files.size(); ++i) {
            const auto& fpath = files[i];
            cv::Mat img = cv::imread(fpath, cv::IMREAD_ANYDEPTH | cv::IMREAD_ANYCOLOR);
            if (img.empty()) continue;

            auto stages = ir_enhance::EnhancementPipeline::process(img, params);
            auto mIn = ir_enhance::EnhancementPipeline::computeMetrics(stages.input8u);
            auto mOut = ir_enhance::EnhancementPipeline::computeMetrics(stages.fusedResult, stages.input8u);

            times.push_back(stages.elapsedMs);
            ieIn.push_back(mIn.imageEntropy);
            ieOut.push_back(mOut.imageEntropy);
            geIn.push_back(mIn.gradientEntropy);
            geOut.push_back(mOut.gradientEntropy);
            rmsIn.push_back(mIn.rmsContrast);
            rmsOut.push_back(mOut.rmsContrast);

            if (csv.is_open()) {
                csv << fs::path(fpath).filename().string() << ","
                    << stages.elapsedMs << ","
                    << mIn.imageEntropy << "," << mOut.imageEntropy << ","
                    << mIn.gradientEntropy << "," << mOut.gradientEntropy << ","
                    << mIn.rmsContrast << "," << mOut.rmsContrast << "\n";
            }

            if (!outDir.empty()) {
                std::string outImg = (fs::path(outDir) / (fs::path(fpath).stem().string() + "_enhanced.png")).string();
                cv::imwrite(outImg, stages.fusedResult);
            }

            if ((i + 1) % 50 == 0 || i + 1 == files.size()) {
                std::cout << "Progress: " << (i + 1) << "/" << files.size()
                          << " (" << std::fixed << std::setprecision(1) << stages.elapsedMs << " ms/img)\r" << std::flush;
            }
        }
        std::cout << "\nBatch evaluation completed!\n\n";

        auto mean = [](const std::vector<double>& v) {
            return v.empty() ? 0.0 : std::accumulate(v.begin(), v.end(), 0.0) / v.size();
        };

        std::cout << "========================================================\n"
                  << "                  BENCHMARK RESULTS\n"
                  << "========================================================\n"
                  << "Total Images Tested : " << times.size() << "\n"
                  << "Average Latency     : " << mean(times) << " ms ("
                  << (1000.0 / mean(times)) << " FPS)\n"
                  << "Information Entropy : " << mean(ieIn) << " -> " << mean(ieOut)
                  << " (Δ +" << (mean(ieOut) - mean(ieIn)) << ")\n"
                  << "Gradient Entropy    : " << mean(geIn) << " -> " << mean(geOut)
                  << " (Δ +" << (mean(geOut) - mean(geIn)) << ")\n"
                  << "RMS Contrast        : " << mean(rmsIn) << " -> " << mean(rmsOut)
                  << " (Δ +" << (mean(rmsOut) - mean(rmsIn)) << ")\n"
                  << "========================================================\n";

        if (csv.is_open()) {
            std::cout << "Report saved to: " << (fs::path(outDir) / "benchmark_report.csv").string() << std::endl;
        }
        return 0;
    }

    // SINGLE IMAGE MODE
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

    auto stages = ir_enhance::EnhancementPipeline::process(input, params);
    std::cout << "Processing finished in: " << stages.elapsedMs << " ms" << std::endl;

    auto mIn = ir_enhance::EnhancementPipeline::computeMetrics(stages.input8u);
    auto mOut = ir_enhance::EnhancementPipeline::computeMetrics(stages.fusedResult, !ref.empty() ? ref : stages.input8u);

    std::cout << "\n--- Metrics Comparison ---\n"
              << "Input  - IE: " << mIn.imageEntropy
              << ", GE: " << mIn.gradientEntropy
              << ", RMS Contrast: " << mIn.rmsContrast << "\n"
              << "Output - IE: " << mOut.imageEntropy
              << ", GE: " << mOut.gradientEntropy
              << ", RMS Contrast: " << mOut.rmsContrast << "\n";

    if (!ref.empty()) {
        std::cout << "Vs Reference Target - RMSE: " << mOut.rmse
                  << ", PSNR: " << mOut.psnr << " dB"
                  << ", SSIM: " << mOut.ssim << "\n";
    }

    fs::create_directories("results");
    cv::imwrite("results/cli_out.png", stages.fusedResult);
    std::cout << "\nSaved enhanced output to: results/cli_out.png" << std::endl;

    return 0;
}
