#include "tracking/ImprovedTracking.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace bullet_detection {

// ===== KALMAN TRACKER IMPLEMENTATION =====

KalmanTracker::KalmanTracker() {
    std::memset(&state_, 0, sizeof(KalmanState));
}

void KalmanTracker::initialize(
    const Point3D& measurement,
    float pos_noise,
    float vel_noise
) {
    state_.x = measurement.x;
    state_.y = measurement.y;
    state_.z = measurement.z;
    state_.vx = 0.0f;
    state_.vy = 0.0f;
    state_.vz = 0.0f;

    // Initialize state covariance (high initial uncertainty)
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            state_.P[i][j] = 0.0f;
        }
    }
    state_.P[0][0] = pos_noise * pos_noise;
    state_.P[1][1] = pos_noise * pos_noise;
    state_.P[2][2] = pos_noise * pos_noise;
    state_.P[3][3] = vel_noise * vel_noise;
    state_.P[4][4] = vel_noise * vel_noise;
    state_.P[5][5] = vel_noise * vel_noise;

    // Process noise covariance (constant velocity model)
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            state_.Q[i][j] = 0.0f;
        }
    }
    float q_pos = 0.01f;
    float q_vel = 0.001f;
    state_.Q[0][0] = q_pos;
    state_.Q[1][1] = q_pos;
    state_.Q[2][2] = q_pos;
    state_.Q[3][3] = q_vel;
    state_.Q[4][4] = q_vel;
    state_.Q[5][5] = q_vel;

    // Measurement noise covariance
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            state_.R[i][j] = 0.0f;
        }
    }
    state_.R[0][0] = pos_noise * pos_noise;
    state_.R[1][1] = pos_noise * pos_noise;
    state_.R[2][2] = pos_noise * pos_noise;

    initialized_ = true;
}

void KalmanTracker::predict(double dt) {
    if (!initialized_) return;

    // Predict state: x_pred = F * x
    // For constant velocity: [x, y, z] = [x, y, z] + [vx, vy, vz] * dt
    state_.x += state_.vx * dt;
    state_.y += state_.vy * dt;
    state_.z += state_.vz * dt;

    // Predict covariance: P_pred = F * P * F^T + Q
    predictCovariance(dt);
}

void KalmanTracker::update(const Point3D& measurement) {
    if (!initialized_) {
        initialize(measurement);
        return;
    }

    // Compute residual
    float res_x = measurement.x - state_.x;
    float res_y = measurement.y - state_.y;
    float res_z = measurement.z - state_.z;
    cv::Point3f residual(res_x, res_y, res_z);

    // Compute Kalman gain: K = P * H^T * (H * P * H^T + R)^-1
    // For our case, H is identity for positions
    
    // S = H * P * H^T + R (innovation covariance)
    float S[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            S[i][j] = state_.P[i][j] + state_.R[i][j];
        }
    }

    // Compute Kalman gain (simplified for position measurements only)
    float K[6][3];
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 3; ++j) {
            K[i][j] = state_.P[i][j] / (S[j][j] + 1e-6f);
        }
    }

    // Update state: x_new = x_pred + K * residual
    state_.x += K[0][0] * res_x + K[0][1] * res_y + K[0][2] * res_z;
    state_.y += K[1][0] * res_x + K[1][1] * res_y + K[1][2] * res_z;
    state_.z += K[2][0] * res_x + K[2][1] * res_y + K[2][2] * res_z;
    state_.vx += K[3][0] * res_x + K[3][1] * res_y + K[3][2] * res_z;
    state_.vy += K[4][0] * res_x + K[4][1] * res_y + K[4][2] * res_z;
    state_.vz += K[5][0] * res_x + K[5][1] * res_y + K[5][2] * res_z;

    // Update covariance: P_new = (I - K * H) * P
    updateCovariance();
}

cv::Point3f KalmanTracker::getPredictedPosition() const {
    return cv::Point3f(state_.x, state_.y, state_.z);
}

float KalmanTracker::computeMahalanobisDistance(const Point3D& measurement) const {
    if (!initialized_) return 1e6f;

    // Compute residual
    float res_x = measurement.x - state_.x;
    float res_y = measurement.y - state_.y;
    float res_z = measurement.z - state_.z;

    // Compute innovation covariance: S = P + R (for position only)
    float S[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            S[i][j] = state_.P[i][j] + state_.R[i][j];
        }
    }

    // Invert S (simplified for diagonal-like)
    float S_inv[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            S_inv[i][j] = 0.0f;
        }
    }
    S_inv[0][0] = 1.0f / (S[0][0] + 1e-6f);
    S_inv[1][1] = 1.0f / (S[1][1] + 1e-6f);
    S_inv[2][2] = 1.0f / (S[2][2] + 1e-6f);

    // Mahalanobis distance: d = sqrt(residual^T * S_inv * residual)
    float d_squared = res_x * res_x * S_inv[0][0] + 
                     res_y * res_y * S_inv[1][1] + 
                     res_z * res_z * S_inv[2][2];

    return std::sqrt(d_squared);
}

bool KalmanTracker::isWithinGate(const Point3D& measurement, float threshold) const {
    return computeMahalanobisDistance(measurement) < threshold;
}

cv::Point3f KalmanTracker::getResidual(const Point3D& measurement) const {
    return cv::Point3f(
        measurement.x - state_.x,
        measurement.y - state_.y,
        measurement.z - state_.z
    );
}

void KalmanTracker::predictCovariance(double dt) {
    // P_pred = F * P * F^T + Q
    // For constant velocity: F has special structure
    float F[6][6];
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            F[i][j] = 0.0f;
        }
    }
    // Position propagation
    for (int i = 0; i < 3; ++i) {
        F[i][i] = 1.0f;
        F[i][i + 3] = dt;  // velocity affects position
    }
    // Velocity constant
    for (int i = 3; i < 6; ++i) {
        F[i][i] = 1.0f;
    }

    // Simplified: just increase uncertainty by process noise
    for (int i = 0; i < 3; ++i) {
        state_.P[i][i] += state_.Q[i][i] * dt * dt;
    }
    for (int i = 3; i < 6; ++i) {
        state_.P[i][i] += state_.Q[i][i];
    }
}

void KalmanTracker::updateCovariance() {
    // P_new = (I - K * H) * P (simplified)
    // For our case, reduce covariance based on measurement confidence
    for (int i = 0; i < 3; ++i) {
        state_.P[i][i] *= 0.5f;  // Reduce uncertainty
    }
}

// ===== TRACK ASSOCIATOR (HUNGARIAN ALGORITHM) =====

TrackAssociator::TrackAssociator() {}

std::vector<std::pair<int, int>> TrackAssociator::associateTracks(
    const std::vector<ImprovedTrack>& tracks,
    const std::vector<Point3D>& detections,
    float gating_threshold,
    float max_distance
) const {
    std::vector<std::pair<int, int>> associations;

    if (tracks.empty() || detections.empty()) {
        return associations;
    }

    // Build cost matrix
    std::vector<std::vector<float>> cost_matrix;
    std::vector<bool> track_valid, detection_valid;

    buildCostMatrix(tracks, detections, gating_threshold, max_distance,
                   cost_matrix, track_valid, detection_valid);

    // Apply Hungarian algorithm
    associations = hungarianAlgorithm(cost_matrix, track_valid, detection_valid);

    return associations;
}

void TrackAssociator::buildCostMatrix(
    const std::vector<ImprovedTrack>& tracks,
    const std::vector<Point3D>& detections,
    float gating_threshold,
    float max_distance,
    std::vector<std::vector<float>>& cost_matrix,
    std::vector<bool>& track_valid,
    std::vector<bool>& detection_valid
) const {
    cost_matrix.assign(tracks.size(), std::vector<float>(detections.size(), 1e6f));
    track_valid.assign(tracks.size(), true);
    detection_valid.assign(detections.size(), true);

    for (size_t t = 0; t < tracks.size(); ++t) {
        for (size_t d = 0; d < detections.size(); ++d) {
            // Compute Mahalanobis distance
            float dist = tracks[t].kalman.computeMahalanobisDistance(detections[d]);

            // Check if within gate
            if (dist < gating_threshold) {
                cost_matrix[t][d] = dist;
            }
        }
    }
}

std::vector<std::pair<int, int>> TrackAssociator::hungarianAlgorithm(
    const std::vector<std::vector<float>>& cost_matrix,
    const std::vector<bool>& track_valid,
    const std::vector<bool>& detection_valid
) const {
    std::vector<std::pair<int, int>> associations;

    // Simplified greedy Hungarian (full implementation would be more complex)
    std::vector<bool> used_detections(cost_matrix[0].size(), false);

    for (size_t t = 0; t < cost_matrix.size(); ++t) {
        if (!track_valid[t]) continue;

        float best_cost = 1e6f;
        int best_d = -1;

        for (size_t d = 0; d < cost_matrix[t].size(); ++d) {
            if (!detection_valid[d] || used_detections[d]) continue;
            if (cost_matrix[t][d] < best_cost) {
                best_cost = cost_matrix[t][d];
                best_d = d;
            }
        }

        if (best_d >= 0 && best_cost < 1e6f) {
            associations.push_back({t, best_d});
            used_detections[best_d] = true;
        }
    }

    return associations;
}

// ===== IMPROVED TRACK MANAGER =====

ImprovedTrackManager::ImprovedTrackManager() {}

void ImprovedTrackManager::updateWithDetections(
    const std::vector<Point3D>& detections,
    double timestamp,
    double dt
) {
    // Predict all tracks
    for (auto& track : tracks_) {
        track.kalman.predict(dt);
        if (!track.active) {
            track.missed_frames++;
        }
    }

    // Associate detections to tracks
    auto associations = associator_.associateTracks(tracks_, detections, 3.0f, 100.0f);

    std::vector<bool> matched_detections(detections.size(), false);

    for (const auto& [track_idx, det_idx] : associations) {
        updateTrack(tracks_[track_idx], detections[det_idx], timestamp);
        matched_detections[det_idx] = true;
    }

    // Create new tracks for unmatched detections
    for (size_t i = 0; i < detections.size(); ++i) {
        if (!matched_detections[i]) {
            createNewTrack(detections[i], timestamp);
        }
    }

    // Update track qualities
    for (auto& track : tracks_) {
        updateTrackQuality(track);
    }
}

void ImprovedTrackManager::predictTracks(double dt) {
    for (auto& track : tracks_) {
        track.kalman.predict(dt);
    }
}

std::vector<ImprovedTrack> ImprovedTrackManager::getConfirmedTracks() const {
    std::vector<ImprovedTrack> confirmed;
    for (const auto& track : tracks_) {
        if (track.confirmed && track.active) {
            confirmed.push_back(track);
        }
    }
    return confirmed;
}

void ImprovedTrackManager::cleanupTracks(double max_age, double max_missing_time) {
    auto it = tracks_.begin();
    double current_time = 0.0;

    // Find current time
    for (const auto& track : tracks_) {
        current_time = std::max(current_time, track.last_update_time);
    }

    while (it != tracks_.end()) {
        double age = current_time - it->creation_time;
        double missing_time = current_time - it->last_update_time;

        if ((age > max_age) || (!it->active && missing_time > max_missing_time)) {
            track_id_to_index_.erase(it->track_id);
            it = tracks_.erase(it);
        } else {
            ++it;
        }
    }
}

ImprovedTrackManager::TrackingStatistics ImprovedTrackManager::getStatistics() const {
    TrackingStatistics stats;
    stats.total_tracks = tracks_.size();
    stats.confirmed_tracks = 0;
    stats.tentative_tracks = 0;
    stats.average_smoothness = 0.0f;

    for (const auto& track : tracks_) {
        if (track.confirmed) stats.confirmed_tracks++;
        else stats.tentative_tracks++;
        stats.average_smoothness += track.smoothness_metric;
    }

    if (!tracks_.empty()) {
        stats.average_smoothness /= tracks_.size();
    }

    return stats;
}

std::string ImprovedTrackManager::generateDiagnosticReport() const {
    auto stats = getStatistics();
    return stats.toString();
}

void ImprovedTrackManager::createNewTrack(const Point3D& detection, double timestamp) {
    ImprovedTrack track;
    track.track_id = next_track_id_++;
    track.kalman.initialize(detection, 1.0f, 0.5f);
    track.history.push_back(detection);
    track.creation_time = timestamp;
    track.last_update_time = timestamp;
    track.missed_frames = 0;
    track.active = false;  // Not active until confirmed
    track.confirmed = false;

    track_id_to_index_[track.track_id] = tracks_.size();
    tracks_.push_back(track);
}

void ImprovedTrackManager::updateTrack(ImprovedTrack& track, const Point3D& detection, double timestamp) {
    track.kalman.update(detection);
    track.history.push_back(detection);
    track.last_update_time = timestamp;
    track.missed_frames = 0;
    track.consecutive_detections++;

    if (!track.active && track.consecutive_detections >= MIN_CONFIRMATION_FRAMES) {
        track.active = true;
    }
}

void ImprovedTrackManager::updateTrackQuality(ImprovedTrack& track) {
    if (track.history.size() >= 2) {
        track.smoothness_metric = TrackQuality::computeSmoothness(track.history, track.residuals);
        track.confirmed = isTrackConfirmed(track);
    }
}

bool ImprovedTrackManager::isTrackConfirmed(const ImprovedTrack& track) const {
    return track.active && 
           track.history.size() >= MIN_CONFIRMATION_FRAMES &&
           track.smoothness_metric < SMOOTHNESS_THRESHOLD;
}

// ===== TRACK QUALITY METRICS =====

float TrackQuality::computeSmoothness(
    const std::deque<Point3D>& history,
    const std::deque<float>& residuals
) {
    if (history.size() < 3) return 0.0f;

    float smoothness = 0.0f;
    for (size_t i = 2; i < history.size(); ++i) {
        // Compute jerk (acceleration change)
        float a1_x = (history[i-1].x - history[i-2].x) - (history[i].x - history[i-1].x);
        float a1_y = (history[i-1].y - history[i-2].y) - (history[i].y - history[i-1].y);
        float a1_z = (history[i-1].z - history[i-2].z) - (history[i].z - history[i-1].z);
        float jerk = std::sqrt(a1_x*a1_x + a1_y*a1_y + a1_z*a1_z);
        smoothness += jerk;
    }

    return smoothness / (history.size() - 2);
}

float TrackQuality::computeConsistency(const ImprovedTrack& track) {
    if (track.history.size() < 2) return 1.0f;
    return static_cast<float>(track.consecutive_detections) / track.history.size();
}

float TrackQuality::predictTrackLifetime(const ImprovedTrack& track) {
    // Simple heuristic: tracks with smooth motion last longer
    return track.smoothness_metric < 1.0f ? 5.0f : 2.0f;
}

// ===== STATISTICS =====

std::string ImprovedTrackManager::TrackingStatistics::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "=== TRACKING STATISTICS ===" << std::endl;
    oss << "Total: " << total_tracks << " | "
        << "Confirmed: " << confirmed_tracks << " | "
        << "Tentative: " << tentative_tracks << std::endl;
    oss << "Average Smoothness: " << average_smoothness << "mm" << std::endl;
    return oss.str();
}

} // namespace bullet_detection
