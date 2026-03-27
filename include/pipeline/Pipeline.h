#pragma once
#include <vector>
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <opencv2/opencv.hpp>
#include "candidate/CandidateDetector.h"
#include "feature/FeatureExtractor.h"
#include "tier/Tiers.h"
#include "calibration/CameraModel.h"
#include "reconstruction/Triangulation.h"
#include "tracking/TrackManager.h"
#include "core/ErrorHandler.h"
#include "core/FeatureValidator.h"
#include "pipeline/FrameSynchronizer.h"

namespace bullet_detection {

class SingleCameraPipeline {
public:
    SingleCameraPipeline(int camera_id = 0);

    std::vector<DetectionResult> processFrame(
        const cv::Mat& frame_ir,
        uint64_t frame_id
    );

    void trainClassifiers(
        const std::vector<FeatureVector>& positive_samples,
        const std::vector<FeatureVector>& negative_samples
    );

private:
    int camera_id_;
    CandidateDetector detector_;
    FeatureExtractor feature_extractor_;
    Tier1Classifier tier1_;
    Tier2Classifier tier2_;
    Tier3Classifier tier3_;
    ConfidenceCalibrator calibrator_;

    DetectionResult makeFinalDecision(const CandidateRegion& candidate);
};

class MultiCameraPipeline {
public:
    MultiCameraPipeline(int n_cameras = 2);

    Result<bool> addCameraCalibration(const CameraCalibration& calib);
    Result<bool> addCameraFrame(int camera_id, const cv::Mat& frame_ir, uint64_t frame_id, double timestamp);

    Result<std::vector<Point3D>> process();

    // Get synchronization statistics
    FrameSynchronizer::BufferStats getBufferStats() const;

private:
    int n_cameras_;
    std::vector<CameraModel> camera_models_;
    std::vector<SingleCameraPipeline> single_pipelines_;
    std::unique_ptr<FrameSynchronizer> frame_sync_;
    std::mutex sync_mutex_;

    Triangulator triangulator_;
    OutlierRejector outlier_rejector_;
    BundleAdjuster bundle_adjuster_;
    TrackManager track_manager_;

    Result<std::vector<Point3D>> triangulateDetections(const std::vector<CameraFrame>& frames);
    Result<std::vector<Point3D>> rejectOutliers(const std::vector<Point3D>& points);
    void performBundleAdjustment(std::vector<Point3D>& points);
};

} // namespace bullet_detection
