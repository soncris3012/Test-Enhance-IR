#pragma once
#include <opencv2/core.hpp>

namespace ir_enhance {

/**
 * @brief Guided Filter implementation (He et al., 2012)
 *
 * @param I Guidance image (CV_32F, 1 channel, [0, 1])
 * @param p Filtering input image (CV_32F, 1 channel, [0, 1])
 * @param r Filter radius (window size = 2*r + 1)
 * @param eps Regularization parameter (e.g. 0.04)
 * @return cv::Mat Filtered output image (CV_32F, 1 channel)
 */
cv::Mat guidedFilter(const cv::Mat& I, const cv::Mat& p, int r, float eps);

/**
 * @brief Self-guided filter where guidance image I is the input image p
 */
inline cv::Mat selfGuidedFilter(const cv::Mat& I, int r, float eps) {
    return guidedFilter(I, I, r, eps);
}

} // namespace ir_enhance
