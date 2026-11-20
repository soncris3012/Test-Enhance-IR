#pragma once
#include <string>
#include <vector>

namespace ir_enhance {

struct DatasetEntry {
    std::string id;          // unique id, e.g. "paper", "llvip_test", "flir_val"
    std::string displayName; // UI label, e.g. "LLVIP (Test IR - 3,463 imgs)"
    std::string folderPath;  // absolute directory containing the images
    int imageCount{0};
};

class DatasetManager {
public:
    // Auto-detect root dataset folder:
    // 1. overrideRoot if valid and not empty
    // 2. Env vars: IR_DATASET_DIR, QART_DATASETS_DIR
    // 3. User home locations: ~/QART_Datasets/raw, ~/QART_Datasets, ~/datasets
    // 4. Relative paths: ./QART_Datasets/raw, ../QART_Datasets/raw, ./datasets
    static std::string detectDatasetRoot(const std::string& overrideRoot = "");

    // Discovers known benchmark datasets (LLVIP, FLIR, MSRS, RoadScene, Paper Samples)
    // given a root directory and the local sample_images folder.
    static std::vector<DatasetEntry> discoverDatasets(const std::string& datasetRoot,
                                                      const std::string& sampleImagesDir = "");

    // Scans a folder for valid image files (.png, .jpg, .jpeg, .bmp, .tif, .tiff)
    // Sorted alphabetically. If maxLimit > 0, caps the return count.
    static std::vector<std::string> scanImageFiles(const std::string& dirPath, int maxLimit = -1);

    // Checks if a path is a supported image extension
    static bool isImageFile(const std::string& filename);
};

} // namespace ir_enhance
