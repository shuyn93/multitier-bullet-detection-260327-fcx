#pragma once
#include "core/Types.h"
#include "core/MathUtils.h"
#include <vector>
#include <map>
#include <deque>

namespace bullet_detection {

class TrackManager {
public:
    TrackManager();

    uint64_t addDetection(
        const Point3D& point3d,
        const std::vector<cv::Point2f>& projections,
        double timestamp
    );

    void updateTracks(double timestamp);
    void removeStaleTracks(double max_age = 5.0);

    const std::vector<Track>& getActiveTracks() const { return tracks_; }
    const Track* getTrackById(uint64_t track_id) const;

    std::vector<Track> getConfirmedTracks() const;

private:
    std::vector<Track> tracks_;
    std::map<uint64_t, size_t> track_id_to_index_;
    uint64_t next_track_id_ = 1;
    double max_track_distance_ = 50.0;  // pixels
};

class MotionModel {
public:
    MotionModel();

    cv::Point3f predictNextPosition(
        const std::deque<Point3D>& history,
        double dt
    ) const;

private:
    float alpha_ = 0.7f;  // Exponential smoothing factor
};

} // namespace bullet_detection
