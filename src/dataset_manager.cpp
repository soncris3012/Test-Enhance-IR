#include "dataset_manager.hpp"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>

namespace fs = std::filesystem;

namespace ir_enhance {

bool DatasetManager::isImageFile(const std::string& filename) {
    if (filename.empty() || filename[0] == '.') {
        return false;
    }
    fs::path p(filename);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    return (ext == ".png"  || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".bmp"  || ext == ".tif" || ext == ".tiff" ||
            ext == ".webp");
}

std::string DatasetManager::detectDatasetRoot(const std::string& overrideRoot) {
    if (!overrideRoot.empty()) {
        std::error_code ec;
        if (fs::exists(overrideRoot, ec) && fs::is_directory(overrideRoot, ec)) {
            // Check if there is an inner "raw" subfolder
            fs::path subRaw = fs::path(overrideRoot) / "raw";
            if (fs::exists(subRaw, ec) && fs::is_directory(subRaw, ec)) {
                return subRaw.string();
            }
            return overrideRoot;
        }
    }

    // Check environment variables
    const char* envKeys[] = {"IR_DATASET_DIR", "QART_DATASETS_DIR", "DATASET_ROOT"};
    for (const char* key : envKeys) {
        const char* val = std::getenv(key);
        if (val && *val) {
            std::error_code ec;
            if (fs::exists(val, ec) && fs::is_directory(val, ec)) {
                fs::path subRaw = fs::path(val) / "raw";
                if (fs::exists(subRaw, ec)) return subRaw.string();
                return val;
            }
        }
    }

    // Check user home directory
    std::string home;
#if defined(_WIN32)
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) home = userProfile;
#else
    const char* homeEnv = std::getenv("HOME");
    if (homeEnv) home = homeEnv;
#endif

    if (!home.empty()) {
        const std::string candidateRoots[] = {
            home + "/QART_Datasets/raw",
            home + "/QART_Datasets",
            home + "/datasets/raw",
            home + "/datasets"
        };
        for (const auto& c : candidateRoots) {
            std::error_code ec;
            if (fs::exists(c, ec) && fs::is_directory(c, ec)) {
                return c;
            }
        }
    }

    // Check workspace relative paths
    const std::string relativeCandidates[] = {
        "./QART_Datasets/raw",
        "../QART_Datasets/raw",
        "../../QART_Datasets/raw",
        "./datasets/raw",
        "./datasets"
    };
    for (const auto& rel : relativeCandidates) {
        std::error_code ec;
        if (fs::exists(rel, ec) && fs::is_directory(rel, ec)) {
            return fs::canonical(rel, ec).string();
        }
    }

    return "";
}

std::vector<std::string> DatasetManager::scanImageFiles(const std::string& dirPath, int maxLimit) {
    std::vector<std::string> files;
    std::error_code ec;
    if (dirPath.empty() || !fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) {
        return files;
    }

    for (const auto& entry : fs::directory_iterator(dirPath, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_regular_file(ec)) {
            std::string filename = entry.path().filename().string();
            if (isImageFile(filename)) {
                files.push_back(entry.path().string());
            }
        }
    }

    std::sort(files.begin(), files.end());

    if (maxLimit > 0 && static_cast<int>(files.size()) > maxLimit) {
        files.resize(static_cast<size_t>(maxLimit));
    }

    return files;
}

std::vector<DatasetEntry> DatasetManager::discoverDatasets(const std::string& datasetRoot,
                                                          const std::string& sampleImagesDir) {
    std::vector<DatasetEntry> result;
    std::error_code ec;

    // 1. Built-in Paper Samples
    if (!sampleImagesDir.empty() && fs::exists(sampleImagesDir, ec)) {
        auto sampleImgs = scanImageFiles(sampleImagesDir);
        if (!sampleImgs.empty()) {
            DatasetEntry e;
            e.id = "paper_samples";
            e.displayName = "Paper Samples (ICADI 2025 - " + std::to_string(sampleImgs.size()) + " imgs)";
            e.folderPath = sampleImagesDir;
            e.imageCount = static_cast<int>(sampleImgs.size());
            result.push_back(e);
        }
    }

    if (datasetRoot.empty() || !fs::exists(datasetRoot, ec) || !fs::is_directory(datasetRoot, ec)) {
        return result;
    }

    fs::path root(datasetRoot);

    // Candidates map: <id, subPath, prettyName>
    struct BenchmarkCandidate {
        std::string id;
        std::string subPath;
        std::string prettyName;
    };

    const std::vector<BenchmarkCandidate> candidates = {
        {"llvip_test",      "llvip/infrared/test",          "LLVIP (Test IR)"},
        {"llvip_train",     "llvip/infrared/train",         "LLVIP (Train IR)"},
        {"flir_val",        "flir/images_thermal_val/data", "FLIR (Thermal Val)"},
        {"flir_train",      "flir/images_thermal_train/data","FLIR (Thermal Train)"},
        {"msrs_test",       "msrs/test/ir",                 "MSRS (Test IR)"},
        {"msrs_train",      "msrs/train/ir",                "MSRS (Train IR)"},
        {"roadscene_ir",    "roadscene/infrared",           "RoadScene (IR)"},
        {"roadscene_crop",  "roadscene/cropinfrared",       "RoadScene (Crop IR)"}
    };

    for (const auto& c : candidates) {
        fs::path p = root / c.subPath;
        if (!fs::exists(p, ec)) {
            // Also test if root is parent of "raw"
            p = root / "raw" / c.subPath;
        }

        if (fs::exists(p, ec) && fs::is_directory(p, ec)) {
            // Fast count images
            int count = 0;
            for (const auto& entry : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, ec)) {
                if (entry.is_regular_file(ec) && isImageFile(entry.path().filename().string())) {
                    count++;
                }
            }
            if (count > 0) {
                DatasetEntry e;
                e.id = c.id;
                e.displayName = c.prettyName + " (" + std::to_string(count) + " imgs)";
                e.folderPath = p.string();
                e.imageCount = count;
                result.push_back(e);
            }
        }
    }

    return result;
}

} // namespace ir_enhance
