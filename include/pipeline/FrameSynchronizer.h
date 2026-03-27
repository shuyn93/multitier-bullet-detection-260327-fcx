#pragma once

#include <map>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "core/Types.h"
#include "core/ErrorHandler.h"

namespace bullet_detection {

// ===== Frame Synchronizer for Multi-Camera Systems =====
class FrameSynchronizer {
public:
    FrameSynchronizer(int n_cameras, int max_buffer_size = 30, int sync_tolerance_ms = 50)
        : n_cameras_(n_cameras),
          max_buffer_size_(max_buffer_size),
          sync_tolerance_ms_(sync_tolerance_ms) {
        frame_buffers_.resize(n_cameras_);
    }

    // Thread-safe frame insertion
    Result<bool> addFrame(const CameraFrame& frame) {
        SAFE_CONTEXT();

        if (frame.camera_id < 0 || frame.camera_id >= n_cameras_) {
            return Result<bool>::failure(
                ErrorCode::INVALID_INPUT,
                "Camera ID out of range: " + std::to_string(frame.camera_id)
            );
        }

        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);

            auto& buffer = frame_buffers_[frame.camera_id];

            // Remove old frames if buffer exceeds size
            while (buffer.size() >= static_cast<size_t>(max_buffer_size_)) {
                buffer.pop_front();
            }

            buffer.push_back(frame);

            // Update timestamp for this camera
            last_frame_timestamp_[frame.camera_id] = frame.timestamp;
        }

        cv_sync_.notify_one();
        return Result<bool>::success(true);
    }

    // Get synchronized frames (all cameras at similar timestamps)
    Result<std::vector<CameraFrame>> getSynchronizedFrames(int timeout_ms = 1000) {
        SAFE_CONTEXT();

        std::unique_lock<std::mutex> lock(buffer_mutex_);

        // Wait for frames from all cameras
        auto start = std::chrono::steady_clock::now();
        while (!areBuffersSynchronized()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();

            if (elapsed > timeout_ms) {
                return Result<std::vector<CameraFrame>>::failure(
                    ErrorCode::SYNCHRONIZATION_ERROR,
                    "Frame synchronization timeout after " + std::to_string(timeout_ms) + "ms"
                );
            }

            cv_sync_.wait_for(lock, std::chrono::milliseconds(10));
        }

        // Extract synchronized frames
        std::vector<CameraFrame> synchronized;
        for (int cam_id = 0; cam_id < n_cameras_; ++cam_id) {
            if (!frame_buffers_[cam_id].empty()) {
                synchronized.push_back(frame_buffers_[cam_id].front());
                frame_buffers_[cam_id].pop_front();
            }
        }

        return Result<std::vector<CameraFrame>>::success(synchronized);
    }

    // Get frames by frame_id (exact match across cameras)
    Result<std::vector<CameraFrame>> getFramesByID(uint64_t frame_id) {
        SAFE_CONTEXT();

        std::lock_guard<std::mutex> lock(buffer_mutex_);

        std::vector<CameraFrame> result;

        for (int cam_id = 0; cam_id < n_cameras_; ++cam_id) {
            bool found = false;
            for (const auto& frame : frame_buffers_[cam_id]) {
                if (frame.frame_id == frame_id) {
                    result.push_back(frame);
                    found = true;
                    break;
                }
            }

            if (!found) {
                return Result<std::vector<CameraFrame>>::failure(
                    ErrorCode::SYNCHRONIZATION_ERROR,
                    "Frame ID " + std::to_string(frame_id) + " not found in camera " + 
                    std::to_string(cam_id)
                );
            }
        }

        return Result<std::vector<CameraFrame>>::success(result);
    }

    // Get buffer statistics
    struct BufferStats {
        std::vector<int> buffer_sizes;
        std::vector<double> latest_timestamps;
        int n_cameras;
    };

    BufferStats getBufferStats() const {
        std::lock_guard<std::mutex> lock(buffer_mutex_);

        BufferStats stats;
        stats.n_cameras = n_cameras_;

        for (int cam_id = 0; cam_id < n_cameras_; ++cam_id) {
            stats.buffer_sizes.push_back(frame_buffers_[cam_id].size());

            if (last_frame_timestamp_.find(cam_id) != last_frame_timestamp_.end()) {
                stats.latest_timestamps.push_back(last_frame_timestamp_.at(cam_id));
            } else {
                stats.latest_timestamps.push_back(-1.0);
            }
        }

        return stats;
    }

    // Clear all buffers
    void clear() {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        for (auto& buffer : frame_buffers_) {
            buffer.clear();
        }
        last_frame_timestamp_.clear();
    }

private:
    bool areBuffersSynchronized() const {
        // Check if all buffers have frames
        for (const auto& buffer : frame_buffers_) {
            if (buffer.empty()) {
                return false;
            }
        }

        // Check if timestamps are within tolerance
        double min_timestamp = std::numeric_limits<double>::max();
        double max_timestamp = std::numeric_limits<double>::lowest();

        for (const auto& buffer : frame_buffers_) {
            if (!buffer.empty()) {
                min_timestamp = std::min(min_timestamp, buffer.front().timestamp);
                max_timestamp = std::max(max_timestamp, buffer.front().timestamp);
            }
        }

        double time_diff_ms = (max_timestamp - min_timestamp) * 1000.0;
        return time_diff_ms <= sync_tolerance_ms_;
    }

    int n_cameras_;
    int max_buffer_size_;
    int sync_tolerance_ms_;

    std::vector<std::deque<CameraFrame>> frame_buffers_;
    mutable std::map<int, double> last_frame_timestamp_;

    mutable std::mutex buffer_mutex_;
    std::condition_variable cv_sync_;
};

} // namespace bullet_detection
