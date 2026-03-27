#include "tracking/TrackManager.h"
#include <algorithm>
#include <cmath>
#include "core/MathUtils.h"

namespace bullet_detection {

TrackManager::TrackManager() {}

uint64_t TrackManager::addDetection(
    const Point3D& point3d,
    const std::vector<cv::Point2f>& projections,
    double timestamp
) {
    double min_distance = max_track_distance_;
    int best_track_idx = -1;

    // Find closest existing track using optimized distance computation
    for (size_t i = 0; i < tracks_.size(); ++i) {
        if (tracks_[i].history.empty()) continue;

        const Point3D& last_point = tracks_[i].history.back();
        double dist = math_utils::distance3D(
            point3d.x, point3d.y, point3d.z,
            last_point.x, last_point.y, last_point.z
        );

        if (dist < min_distance) {
            min_distance = dist;
            best_track_idx = static_cast<int>(i);
        }
    }

    if (best_track_idx >= 0) {
        // Update existing track
        Track& track = tracks_[best_track_idx];
        track.history.push_back(point3d);
        track.last_update_time = timestamp;
        track.missed_frames = 0;
        return track.track_id;
    }

    // Create new track
    Track new_track;
    new_track.track_id = next_track_id_++;
    new_track.history.push_back(point3d);
    new_track.last_update_time = timestamp;
    new_track.missed_frames = 0;
    new_track.active = true;

    track_id_to_index_[new_track.track_id] = tracks_.size();
    tracks_.push_back(new_track);

    return new_track.track_id;
}

void TrackManager::updateTracks(double timestamp) {
    for (auto& track : tracks_) {
        if (track.active) {
            track.missed_frames++;
            if (track.missed_frames > 30) {
                track.active = false;
            }
        }
    }
}

void TrackManager::removeStaleTracks(double max_age) {
    double current_time = 0.0;
    
    // Find the latest timestamp (use most recent track update as reference)
    for (const auto& track : tracks_) {
        current_time = std::max(current_time, track.last_update_time);
    }
    
    auto it = tracks_.begin();
    while (it != tracks_.end()) {
        // Remove track if: (1) inactive AND (2) hasn't been updated in max_age seconds
        if (!it->active && (current_time - it->last_update_time > max_age)) {
            track_id_to_index_.erase(it->track_id);
            it = tracks_.erase(it);
        } else {
            ++it;
        }
    }
}

const Track* TrackManager::getTrackById(uint64_t track_id) const {
    auto it = track_id_to_index_.find(track_id);
    if (it != track_id_to_index_.end() && it->second < tracks_.size()) {
        return &tracks_[it->second];
    }
    return nullptr;
}

std::vector<Track> TrackManager::getConfirmedTracks() const {
    std::vector<Track> confirmed;
    for (const auto& track : tracks_) {
        if (track.active && track.history.size() > 2) {
            confirmed.push_back(track);
        }
    }
    return confirmed;
}

// ===== MOTION MODEL =====

MotionModel::MotionModel() {}

cv::Point3f MotionModel::predictNextPosition(
    const std::deque<Point3D>& history,
    double dt
) const {
    if (history.size() < 2) {
        return cv::Point3f(0, 0, 0);
    }

    const Point3D& last = history.back();
    const Point3D& prev = history[history.size() - 2];

    float vx = static_cast<float>((last.x - prev.x) / std::max(dt, 0.001));
    float vy = static_cast<float>((last.y - prev.y) / std::max(dt, 0.001));
    float vz = static_cast<float>((last.z - prev.z) / std::max(dt, 0.001));

    // Exponential smoothing
    float pred_x = last.x + alpha_ * vx;
    float pred_y = last.y + alpha_ * vy;
    float pred_z = last.z + alpha_ * vz;

    return cv::Point3f(pred_x, pred_y, pred_z);
}

} // namespace bullet_detection
