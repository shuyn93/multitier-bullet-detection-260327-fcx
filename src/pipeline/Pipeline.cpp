#include "pipeline/Pipeline.h"
#include <algorithm>
#include "core/ErrorHandler.h"
#include "core/FeatureValidator.h"

namespace bullet_detection {

// ===== SINGLE CAMERA PIPELINE =====

SingleCameraPipeline::SingleCameraPipeline(int camera_id)
    : camera_id_(camera_id) {}

std::vector<DetectionResult> SingleCameraPipeline::processFrame(
    const cv::Mat& frame_ir,
    uint64_t frame_id
) {
    SAFE_CONTEXT();
    std::vector<DetectionResult> results;

    if (frame_ir.empty()) {
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::WARNING,
            "processFrame: Empty frame received for camera " + std::to_string(camera_id_)
        );
        return results;
    }

    auto candidates = detector_.detectCandidates(frame_ir, frame_id);

    for (const auto& candidate : candidates) {
        if (candidate.bbox.width < 5 || candidate.bbox.height < 5) continue;

        auto features = feature_extractor_.extractFeatures(candidate.roi_image, candidate.contour);
        
        // Sanitize features to prevent NaN/Inf
        features = FeatureValidator::sanitize(features);

        FeatureVector fv = features.toFeatureVector();

        DetectionResult result = makeFinalDecision(candidate);
        results.push_back(result);
    }

    return results;
}

void SingleCameraPipeline::trainClassifiers(
    const std::vector<FeatureVector>& positive_samples,
    const std::vector<FeatureVector>& negative_samples
) {
    tier1_.trainModels(positive_samples, negative_samples);
    tier2_.trainModel(positive_samples, negative_samples);
    tier3_.trainModel(positive_samples, negative_samples);
}

DetectionResult SingleCameraPipeline::makeFinalDecision(const CandidateRegion& candidate) {
    FeatureVector fv = candidate.features.toFeatureVector();

    auto t1_decision = tier1_.predict(fv);

    ClassifierDecision final_decision = t1_decision;

    if (t1_decision.code == DecisionCode::LOW_CONFIDENCE) {
        auto t2_decision = tier2_.predict(fv);
        final_decision = t2_decision;

        if (t2_decision.code == DecisionCode::LOW_CONFIDENCE) {
            auto t3_code = tier3_.predict(fv);
            if (t3_code == DecisionCode::ACCEPT) {
                final_decision.code = DecisionCode::ACCEPT;
                final_decision.confidence = 0.95f;
            } else {
                final_decision.code = DecisionCode::REJECT;
                final_decision.confidence = 0.1f;
            }
            final_decision.tier = 3;
        }
    }

    DetectionResult result;
    result.bbox = candidate.bbox;
    result.decision = final_decision;
    result.features = fv;
    result.frame_id = candidate.frame_id;
    result.center = cv::Point2f(candidate.bbox.x + candidate.bbox.width / 2.0f,
                                candidate.bbox.y + candidate.bbox.height / 2.0f);

    return result;
}

// ===== MULTI CAMERA PIPELINE =====

MultiCameraPipeline::MultiCameraPipeline(int n_cameras)
    : n_cameras_(n_cameras),
      frame_sync_(std::make_unique<FrameSynchronizer>(n_cameras, 30, 50)) {
    if (n_cameras < 1) {
        throw BulletDetectionException(
            ErrorCode::INVALID_INPUT,
            "MultiCameraPipeline requires at least 1 camera"
        );
    }
    single_pipelines_.resize(n_cameras);
    for (int i = 0; i < n_cameras; ++i) {
        single_pipelines_[i] = SingleCameraPipeline(i);
    }
}

Result<bool> MultiCameraPipeline::addCameraCalibration(const CameraCalibration& calib) {
    SAFE_CONTEXT();

    if (calib.camera_id < 0 || calib.camera_id >= n_cameras_) {
        return Result<bool>::failure(
            ErrorCode::INVALID_INPUT,
            "Camera calibration ID out of range: " + std::to_string(calib.camera_id)
        );
    }

    if (calib.intrinsics.empty() || calib.intrinsics.size() != cv::Size(3, 3)) {
        return Result<bool>::failure(
            ErrorCode::INVALID_INPUT,
            "Invalid camera intrinsics matrix"
        );
    }

    try {
        camera_models_.push_back(CameraModel(calib));
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::INFO,
            "Camera " + std::to_string(calib.camera_id) + " calibration loaded"
        );
        return Result<bool>::success(true);
    } catch (const std::exception& e) {
        return Result<bool>::failure(
            ErrorCode::COMPUTATION_FAILURE,
            "Failed to add camera calibration: " + std::string(e.what())
        );
    }
}

Result<bool> MultiCameraPipeline::addCameraFrame(
    int camera_id,
    const cv::Mat& frame_ir,
    uint64_t frame_id,
    double timestamp
) {
    SAFE_CONTEXT();

    if (frame_ir.empty()) {
        return Result<bool>::failure(
            ErrorCode::INVALID_INPUT,
            "Empty frame received for camera " + std::to_string(camera_id)
        );
    }

    if (camera_id < 0 || camera_id >= n_cameras_) {
        return Result<bool>::failure(
            ErrorCode::INVALID_INPUT,
            "Camera ID out of range: " + std::to_string(camera_id)
        );
    }

    try {
        auto detections = single_pipelines_[camera_id].processFrame(frame_ir, frame_id);

        CameraFrame frame;
        frame.camera_id = camera_id;
        frame.frame_id = frame_id;
        frame.image = frame_ir.clone();
        frame.detections = detections;
        frame.timestamp = timestamp;

        auto sync_result = frame_sync_->addFrame(frame);
        if (sync_result.isFailure()) {
            return Result<bool>::failure(sync_result.getErrorCode(), sync_result.getErrorMessage());
        }

        return Result<bool>::success(true);
    } catch (const std::exception& e) {
        return Result<bool>::failure(
            ErrorCode::COMPUTATION_FAILURE,
            "Error processing frame: " + std::string(e.what())
        );
    }
}

Result<std::vector<Point3D>> MultiCameraPipeline::process() {
    SAFE_CONTEXT();

    auto sync_frames_result = frame_sync_->getSynchronizedFrames(1000);
    if (sync_frames_result.isFailure()) {
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::WARNING,
            sync_frames_result.getErrorMessage()
        );
        return Result<std::vector<Point3D>>::failure(
            sync_frames_result.getErrorCode(),
            sync_frames_result.getErrorMessage()
        );
    }

    auto frames = sync_frames_result.getValue();
    if (frames.size() < 2) {
        return Result<std::vector<Point3D>>::failure(
            ErrorCode::SYNCHRONIZATION_ERROR,
            "Insufficient synchronized frames: " + std::to_string(frames.size())
        );
    }

    auto triangulate_result = triangulateDetections(frames);
    if (triangulate_result.isFailure()) {
        return triangulate_result;
    }

    auto points_3d = triangulate_result.getValue();

    auto outlier_result = rejectOutliers(points_3d);
    if (outlier_result.isFailure()) {
        ErrorLogger::getInstance().log(ErrorLogger::LogLevel::WARNING, outlier_result.getErrorMessage());
    } else {
        points_3d = outlier_result.getValue();
    }

    performBundleAdjustment(points_3d);

    for (const auto& pt : points_3d) {
        track_manager_.addDetection(pt, pt.projections, frames.back().timestamp);
    }

    track_manager_.updateTracks(frames.back().timestamp);

    return Result<std::vector<Point3D>>::success(points_3d);
}

FrameSynchronizer::BufferStats MultiCameraPipeline::getBufferStats() const {
    return frame_sync_->getBufferStats();
}

Result<std::vector<Point3D>> MultiCameraPipeline::triangulateDetections(
    const std::vector<CameraFrame>& frames
) {
    SAFE_CONTEXT();
    std::vector<Point3D> points_3d;

    if (frames.size() < 2 || camera_models_.size() < 2) {
        return Result<std::vector<Point3D>>::failure(
            ErrorCode::INVALID_STATE,
            "Insufficient cameras or frames for triangulation"
        );
    }

    try {
        for (size_t i = 0; i < frames[0].detections.size(); ++i) {
            std::vector<cv::Point2f> projections;
            std::vector<int> valid_cameras;

            for (size_t cam_idx = 0; cam_idx < frames.size(); ++cam_idx) {
                if (i < frames[cam_idx].detections.size()) {
                    projections.push_back(frames[cam_idx].detections[i].center);
                    valid_cameras.push_back(cam_idx);
                }
            }

            if (projections.size() >= 2) {
                std::vector<CameraModel> selected_cameras;
                for (int cam_id : valid_cameras) {
                    if (cam_id < static_cast<int>(camera_models_.size())) {
                        selected_cameras.push_back(camera_models_[cam_id]);
                    }
                }

                if (selected_cameras.size() >= 2) {
                    Point3D pt_3d = triangulator_.triangulateMultiView(projections, selected_cameras);
                    if (pt_3d.confidence > 0.0 && std::isfinite(pt_3d.x) && 
                        std::isfinite(pt_3d.y) && std::isfinite(pt_3d.z)) {
                        pt_3d.point_id = points_3d.size();
                        points_3d.push_back(pt_3d);
                    }
                }
            }
        }

        return Result<std::vector<Point3D>>::success(points_3d);
    } catch (const std::exception& e) {
        return Result<std::vector<Point3D>>::failure(
            ErrorCode::COMPUTATION_FAILURE,
            "Triangulation failed: " + std::string(e.what())
        );
    }
}

Result<std::vector<Point3D>> MultiCameraPipeline::rejectOutliers(const std::vector<Point3D>& points) {
    SAFE_CONTEXT();
    std::vector<Point3D> inliers;

    try {
        for (const auto& point : points) {
            // Simple depth check with bounds
            if (point.z > 0.1 && point.z < 10000.0 &&
                std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z)) {
                inliers.push_back(point);
            }
        }

        return Result<std::vector<Point3D>>::success(inliers);
    } catch (const std::exception& e) {
        return Result<std::vector<Point3D>>::failure(
            ErrorCode::COMPUTATION_FAILURE,
            "Outlier rejection failed: " + std::string(e.what())
        );
    }
}

void MultiCameraPipeline::performBundleAdjustment(std::vector<Point3D>& points) {
    // Note: Bundle adjustment would need synchronized frames
    // For now, we skip if points are empty
    if (points.empty()) return;

    // Future: implement incremental bundle adjustment
    // bundle_adjuster_.optimizePoints(points, camera_models_, detections_per_frame, 2);
}

} // namespace bullet_detection
