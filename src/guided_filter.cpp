#include "guided_filter.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>

namespace ir_enhance {

cv::Mat guidedFilter(const cv::Mat& I, const cv::Mat& p, int r, float eps) {
    CV_Assert(I.depth() == CV_32F && p.depth() == CV_32F);
    CV_Assert(I.channels() == 1 && p.channels() == 1);
    CV_Assert(I.size() == p.size());

    const int radius = std::max(1, r);
    const float regEps = std::max(1e-6f, eps);
    const cv::Size ksize(2 * radius + 1, 2 * radius + 1);

    cv::Mat mean_I, mean_p, mean_Ip, mean_II;
    cv::boxFilter(I, mean_I, CV_32F, ksize, cv::Point(-1, -1), true, cv::BORDER_REFLECT);
    cv::boxFilter(p, mean_p, CV_32F, ksize, cv::Point(-1, -1), true, cv::BORDER_REFLECT);
    cv::boxFilter(I.mul(p), mean_Ip, CV_32F, ksize, cv::Point(-1, -1), true, cv::BORDER_REFLECT);
    cv::boxFilter(I.mul(I), mean_II, CV_32F, ksize, cv::Point(-1, -1), true, cv::BORDER_REFLECT);

    cv::Mat cov_Ip = mean_Ip - mean_I.mul(mean_p);
    cv::Mat var_I = mean_II - mean_I.mul(mean_I);
    cv::max(var_I, 0.0f, var_I);

    // a = cov_Ip / (var_I + eps)
    cv::Mat a = cov_Ip / (var_I + regEps);
    // b = mean_p - a * mean_I
    cv::Mat b = mean_p - a.mul(mean_I);

    cv::Mat mean_a, mean_b;
    cv::boxFilter(a, mean_a, CV_32F, ksize, cv::Point(-1, -1), true, cv::BORDER_REFLECT);
    cv::boxFilter(b, mean_b, CV_32F, ksize, cv::Point(-1, -1), true, cv::BORDER_REFLECT);

    // q = mean_a * I + mean_b
    cv::Mat q = mean_a.mul(I) + mean_b;
    return q;
}

} // namespace ir_enhance
