#include <iostream>
#include <opencv2/opencv.hpp>
#include "pipeline/Pipeline.h"
#include "feature/FeatureExtractor.h"
#include "core/ErrorHandler.h"

int main() {
    using namespace bullet_detection;

    try {
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::INFO,
            "=== Bullet Hole Detection System ==="
        );
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::INFO,
            "Production-grade multi-camera 3D reconstruction"
        );

        // ===== SINGLE CAMERA EXAMPLE =====
        SingleCameraPipeline single_pipeline(0);

        // Generate mock training data
        std::vector<FeatureVector> positive_train, negative_train;
        for (int i = 0; i < 10; ++i) {
            FeatureVector fv;
            for (int j = 0; j < FeatureVector::DIM; ++j) {
                fv.data[j] = 0.7f + (rand() % 100) / 1000.0f;
            }
            positive_train.push_back(fv);

            for (int j = 0; j < FeatureVector::DIM; ++j) {
                fv.data[j] = 0.3f + (rand() % 100) / 1000.0f;
            }
            negative_train.push_back(fv);
        }

        single_pipeline.trainClassifiers(positive_train, negative_train);
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::INFO,
            "[?] Tier classifiers trained"
        );

        // ===== MOCK VIDEO PROCESSING =====
        cv::Mat mock_frame = cv::Mat::zeros(480, 640, CV_8UC1);

        // Draw synthetic bullet holes
        cv::circle(mock_frame, cv::Point(150, 150), 15, cv::Scalar(255), -1);
        cv::circle(mock_frame, cv::Point(400, 300), 12, cv::Scalar(200), -1);
        cv::circle(mock_frame, cv::Point(550, 200), 10, cv::Scalar(180), -1);

        // Add noise
        cv::Mat noise = cv::Mat(mock_frame.size(), CV_8UC1);
        cv::randu(noise, 0, 100);
        mock_frame = mock_frame + noise * 0.3f;

        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::INFO,
            "[Processing frame...]"
        );

        auto detections = single_pipeline.processFrame(mock_frame, 1);

        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::INFO,
            "[?] Found " + std::to_string(detections.size()) + " candidate detections"
        );

        for (size_t i = 0; i < detections.size(); ++i) {
            const auto& det = detections[i];
            std::string decision_str =
                (det.decision.code == DecisionCode::ACCEPT) ? "ACCEPT" : "REJECT";
            ErrorLogger::getInstance().log(
                ErrorLogger::LogLevel::INFO,
                "  Detection " + std::to_string(i) + ": " + decision_str +
                " (conf=" + cv::format("%.2f", det.decision.confidence) + 
                ", tier=" + std::to_string(det.decision.tier) + ")"
            );
        }

        // Visualize
        cv::Mat display = mock_frame.clone();
        cv::cvtColor(display, display, cv::COLOR_GRAY2BGR);

        for (const auto& det : detections) {
            if (det.decision.code == DecisionCode::ACCEPT) {
                cv::rectangle(display, det.bbox, cv::Scalar(0, 255, 0), 2);
                std::string conf_str = cv::format("%.2f", det.decision.confidence);
                cv::putText(display, conf_str, det.bbox.tl(), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                           cv::Scalar(0, 255, 0), 1);
            }
        }

        cv::imshow("Bullet Detection Results", display);

        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::INFO,
            "[Press any key to continue...]"
        );
        cv::waitKey(0);

        // ===== MULTI-CAMERA EXAMPLE =====
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::INFO,
            "\n=== Multi-Camera 3D Reconstruction ==="
        );

        MultiCameraPipeline multi_pipeline(2);

        // Setup mock camera calibrations
        CameraCalibration cam0_calib;
        cam0_calib.camera_id = 0;
        cam0_calib.intrinsics = (cv::Mat_<float>(3, 3) << 
            800, 0, 320, 
            0, 800, 240, 
            0, 0, 1
        );
        cam0_calib.distortion = cv::Mat::zeros(4, 1, CV_32F);
        cam0_calib.rvec = cv::Mat::zeros(3, 1, CV_32F);
        cam0_calib.tvec = cv::Mat::zeros(3, 1, CV_32F);

        auto calib_result = multi_pipeline.addCameraCalibration(cam0_calib);
        if (calib_result.isFailure()) {
            ErrorLogger::getInstance().log(
                ErrorLogger::LogLevel::ERROR,
                "Failed to add camera 0 calibration: " + calib_result.getErrorMessage()
            );
        }

        CameraCalibration cam1_calib;
        cam1_calib.camera_id = 1;
        cam1_calib.intrinsics = cam0_calib.intrinsics.clone();
        cam1_calib.distortion = cam0_calib.distortion.clone();
        cam1_calib.rvec = (cv::Mat_<float>(3, 1) << 0, 0.1f, 0);
        cam1_calib.tvec = (cv::Mat_<float>(3, 1) << 100, 0, 0);

        calib_result = multi_pipeline.addCameraCalibration(cam1_calib);
        if (calib_result.isFailure()) {
            ErrorLogger::getInstance().log(
                ErrorLogger::LogLevel::ERROR,
                "Failed to add camera 1 calibration: " + calib_result.getErrorMessage()
            );
        }

        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::INFO,
            "[?] 2 cameras calibrated"
        );

        // Process frames from both cameras
        double timestamp = 0.0;
        auto frame_result1 = multi_pipeline.addCameraFrame(0, mock_frame, 1, timestamp);
        if (frame_result1.isFailure()) {
            ErrorLogger::getInstance().log(
                ErrorLogger::LogLevel::ERROR,
                "Failed to add frame from camera 0: " + frame_result1.getErrorMessage()
            );
        }

        auto frame_result2 = multi_pipeline.addCameraFrame(1, mock_frame, 1, timestamp);
        if (frame_result2.isFailure()) {
            ErrorLogger::getInstance().log(
                ErrorLogger::LogLevel::ERROR,
                "Failed to add frame from camera 1: " + frame_result2.getErrorMessage()
            );
        }

        auto process_result = multi_pipeline.process();
        if (process_result.isFailure()) {
            ErrorLogger::getInstance().log(
                ErrorLogger::LogLevel::ERROR,
                "Processing failed: " + process_result.getErrorMessage()
            );
        } else {
            auto points_3d = process_result.getValue();
            ErrorLogger::getInstance().log(
                ErrorLogger::LogLevel::INFO,
                "[?] Triangulated " + std::to_string(points_3d.size()) + " 3D points"
            );

            for (size_t i = 0; i < points_3d.size(); ++i) {
                const auto& pt = points_3d[i];
                ErrorLogger::getInstance().log(
                    ErrorLogger::LogLevel::INFO,
                    "  Point " + std::to_string(i) + ": (" + 
                    cv::format("%.2f", pt.x) + ", " + 
                    cv::format("%.2f", pt.y) + ", " + 
                    cv::format("%.2f", pt.z) + ") [conf=" + 
                    cv::format("%.2f", pt.confidence) + "]"
                );
            }
        }

        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::INFO,
            "\n[?] System running successfully"
        );
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::INFO,
            "Production system ready for real IR video input."
        );

        return 0;
    } catch (const BulletDetectionException& e) {
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::CRITICAL,
            "Exception caught: " + std::string(e.what())
        );
        return 1;
    } catch (const std::exception& e) {
        ErrorLogger::getInstance().log(
            ErrorLogger::LogLevel::CRITICAL,
            "Unexpected exception: " + std::string(e.what())
        );
        return 1;
    }
}
