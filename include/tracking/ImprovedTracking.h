#pragma once

#include <vector>
#include <deque>
#include <map>
#include <opencv2/opencv.hpp>
#include "core/Types.h"

namespace bullet_detection {

// ===== KALMAN FILTER STATE & COVARIANCE =====

struct KalmanState {
    // State: [x, y, z, vx, vy, vz]
    float x, y, z;           // Position
    float vx, vy, vz;        // Velocity
    
    // Covariance matrices (stored as symmetric)
    float P[6][6];           // State covariance
    float Q[6][6];           // Process noise covariance
    float R[3][3];           // Measurement noise covariance
};

struct AssociationCost {
    int track_idx;
    int detection_idx;
    float cost;              // Mahalanobis distance or other cost
};

// ===== KALMAN FILTER FOR 3D TRACKING =====

class KalmanTracker {
public:
    KalmanTracker();
    
    // Initialize tracker with first detection
    void initialize(
        const Point3D& measurement,
        float pos_noise = 1.0f,  // mm
        float vel_noise = 0.5f   // mm/frame
    );

    // Predict next state
    void predict(double dt);

    // Update state with measurement
    void update(const Point3D& measurement);

    // Get predicted position
    cv::Point3f getPredictedPosition() const;

    // Get current state with uncertainty
    KalmanState getState() const { return state_; }

    // Compute Mahalanobis distance to measurement
    float computeMahalanobisDistance(const Point3D& measurement) const;

    // Check if measurement is within gate
    bool isWithinGate(const Point3D& measurement, float threshold = 3.0f) const;

    // Get measurement residual
    cv::Point3f getResidual(const Point3D& measurement) const;

private:
    KalmanState state_;
    bool initialized_ = false;

    // Helper functions
    void predictCovariance(double dt);
    void updateState(const cv::Point3f& residual);
    void updateCovariance();
    
    // Matrix operations
    void multiplyMatrices3x3(float A[3][3], float B[3][3], float C[3][3]) const;
    float invertMatrix3x3(float M[3][3], float inv[3][3]) const;
};

// ===== IMPROVED TRACK MANAGEMENT =====

struct ImprovedTrack {
    uint64_t track_id;
    KalmanTracker kalman;
    std::deque<Point3D> history;
    std::deque<float> residuals;      // Track measurement residuals
    
    double creation_time;
    double last_update_time;
    int missed_frames;
    int consecutive_detections = 0;   // For confirmation
    bool active = false;
    bool confirmed = false;            // Only when mature
    
    float smoothness_metric = 0.0f;   // Lower = smoother
};

// ===== TRACK ASSOCIATION (Hungarian Algorithm) =====

class TrackAssociator {
public:
    TrackAssociator();

    // Find optimal associations using Hungarian algorithm
    std::vector<std::pair<int, int>> associateTracks(
        const std::vector<ImprovedTrack>& tracks,
        const std::vector<Point3D>& detections,
        float gating_threshold = 3.0f,
        float max_distance = 100.0f    // mm
    ) const;

private:
    // Build cost matrix
    void buildCostMatrix(
        const std::vector<ImprovedTrack>& tracks,
        const std::vector<Point3D>& detections,
        float gating_threshold,
        float max_distance,
        std::vector<std::vector<float>>& cost_matrix,
        std::vector<bool>& track_valid,
        std::vector<bool>& detection_valid
    ) const;

    // Hungarian algorithm (Kuhn-Munkres)
    std::vector<std::pair<int, int>> hungarianAlgorithm(
        const std::vector<std::vector<float>>& cost_matrix,
        const std::vector<bool>& track_valid,
        const std::vector<bool>& detection_valid
    ) const;
};

// ===== IMPROVED TRACK MANAGER =====

class ImprovedTrackManager {
public:
    ImprovedTrackManager();

    // Process detections and update tracks
    void updateWithDetections(
        const std::vector<Point3D>& detections,
        double timestamp,
        double dt
    );

    // Predict tracks (useful for missing detections)
    void predictTracks(double dt);

    // Get active confirmed tracks
    std::vector<ImprovedTrack> getConfirmedTracks() const;

    // Get all active tracks (including tentative)
    const std::vector<ImprovedTrack>& getActiveTracks() const { return tracks_; }

    // Remove old/dead tracks
    void cleanupTracks(
        double max_age = 5.0,           // seconds
        double max_missing_time = 1.0   // seconds
    );

    // Get track statistics
    struct TrackingStatistics {
        int total_tracks;
        int confirmed_tracks;
        int tentative_tracks;
        float average_smoothness;
        std::string toString() const;
    };
    TrackingStatistics getStatistics() const;

    // Diagnostics
    std::string generateDiagnosticReport() const;

private:
    std::vector<ImprovedTrack> tracks_;
    std::map<uint64_t, size_t> track_id_to_index_;
    uint64_t next_track_id_ = 1;

    TrackAssociator associator_;

    // Track quality thresholds
    static constexpr int MIN_CONFIRMATION_FRAMES = 5;  // Need 5 detections
    static constexpr float MAX_RESIDUAL_THRESHOLD = 2.0f;  // mm
    static constexpr float SMOOTHNESS_THRESHOLD = 1.5f;  // mm
    
    // Helper functions
    void createNewTrack(const Point3D& detection, double timestamp);
    void updateTrack(ImprovedTrack& track, const Point3D& detection, double timestamp);
    void predictTrack(ImprovedTrack& track, double dt);
    
    void updateTrackQuality(ImprovedTrack& track);
    bool isTrackConfirmed(const ImprovedTrack& track) const;
};

// ===== KALMAN FILTER STATE SMOOTHER =====

class RauchTanimotoStriebel {
public:
    // Apply backward smoothing to track history
    static void smoothTrack(
        std::deque<Point3D>& history,
        const std::deque<float>& residuals
    );

private:
    // Backward Kalman equations
    static void backwardPass(
        std::deque<KalmanState>& states,
        std::deque<KalmanState>& smoothed_states,
        double dt
    );
};

// ===== TRACK QUALITY METRICS =====

class TrackQuality {
public:
    // Compute smoothness metric (lower = better)
    static float computeSmoothness(
        const std::deque<Point3D>& history,
        const std::deque<float>& residuals
    );

    // Compute track consistency
    static float computeConsistency(const ImprovedTrack& track);

    // Predict track lifetime
    static float predictTrackLifetime(const ImprovedTrack& track);
};

} // namespace bullet_detection
