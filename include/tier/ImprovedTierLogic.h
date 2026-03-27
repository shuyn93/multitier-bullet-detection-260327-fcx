#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include "core/Types.h"

namespace bullet_detection {

// ===== IMPROVED MULTI-TIER CONFIGURATION =====

struct TierThresholds {
    // Tier 1 thresholds
    float tier1_strong_accept = 0.80f;      // Very confident ACCEPT
    float tier1_weak_accept = 0.65f;        // Weak ACCEPT
    float tier1_weak_reject = 0.35f;        // Weak REJECT
    float tier1_strong_reject = 0.20f;      // Very confident REJECT
    
    // Tier 2 thresholds
    float tier2_strong_accept = 0.75f;
    float tier2_weak_accept = 0.60f;
    float tier2_weak_reject = 0.40f;
    float tier2_strong_reject = 0.25f;
    
    // Escalation thresholds
    float escalation_margin = 0.25f;        // If margin < this, escalate
    float escalation_weak_margin = 0.10f;   // Even weaker margin
    bool escalate_rejects = true;            // Escalate weak rejects?
    
    // Confidence propagation weights
    float tier1_weight = 0.60f;             // How much to trust Tier 1
    float tier2_weight = 0.40f;             // How much to trust Tier 2
    
    // Early termination
    float strong_confidence_margin = 0.45f; // Don't escalate if margin > this
};

struct TierDecisionContext {
    float tier1_confidence = 0.5f;
    float tier1_margin = 0.0f;
    DecisionCode tier1_code = DecisionCode::LOW_CONFIDENCE;
    
    float tier2_confidence = 0.5f;
    float tier2_margin = 0.0f;
    DecisionCode tier2_code = DecisionCode::LOW_CONFIDENCE;
    
    float tier3_vote_ratio = 0.5f;          // Fraction of SVMs voting ACCEPT
    int tier3_accept_votes = 0;
    int tier3_total_svms = 3;
    
    int final_tier = 1;                     // Which tier made final decision
};

// ===== IMPROVED DECISION LOGIC =====

class ImprovedTierLogic {
public:
    ImprovedTierLogic(const TierThresholds& thresholds = TierThresholds());

    // Determine decision code from confidence
    DecisionCode confidenceToDecisionCode(
        float confidence,
        bool is_tier1_decision,
        float* out_margin = nullptr
    ) const;

    // Determine if should escalate to next tier
    bool shouldEscalateTier1(
        float tier1_confidence,
        const TierThresholds& thresholds
    ) const;

    bool shouldEscalateTier2(
        float tier2_confidence,
        const TierThresholds& thresholds
    ) const;

    // Combine confidences from multiple tiers
    float combineConfidences(
        float tier1_confidence,
        float tier2_confidence,
        const TierThresholds& thresholds
    ) const;

    // Tier 3 SVM ensemble decision
    ClassifierDecision decideTier3SVM(
        int accept_votes,
        int total_svms,
        const TierThresholds& thresholds
    ) const;

    // Full decision flow
    ClassifierDecision makeMultiTierDecision(
        float tier1_confidence,
        float tier2_confidence,
        int tier3_accept_votes,
        int tier3_total_svms,
        TierDecisionContext* out_context = nullptr
    ) const;

    void setThresholds(const TierThresholds& thresholds);
    TierThresholds getThresholds() const { return thresholds_; }

private:
    TierThresholds thresholds_;

    // Helper: Compute margin (distance from decision boundary)
    float computeMargin(float confidence) const {
        return std::abs(confidence - 0.5f);
    }

    // Helper: Perform early termination check
    bool canEarlyTerminate(float confidence, const TierThresholds& t) const;
};

// ===== STATISTICS & ANALYSIS =====

struct TierStatistics {
    int tier1_accept_count = 0;
    int tier1_escalate_count = 0;
    int tier1_reject_count = 0;
    
    int tier2_accept_count = 0;
    int tier2_escalate_count = 0;
    int tier2_reject_count = 0;
    
    int tier3_accept_count = 0;
    int tier3_reject_count = 0;
    
    float average_tier1_confidence = 0.5f;
    float average_tier2_confidence = 0.5f;
    
    // Escalation rates
    float tier1_escalation_rate = 0.0f;    // % of Tier 1 that escalate
    float tier2_escalation_rate = 0.0f;    // % of Tier 2 that escalate
    
    void computeRates(int total_samples) {
        if (total_samples > 0) {
            tier1_escalation_rate = static_cast<float>(tier1_escalate_count) / total_samples;
            tier2_escalation_rate = static_cast<float>(tier2_escalate_count) / 
                                   (tier1_escalate_count > 0 ? tier1_escalate_count : 1);
        }
    }
};

class TierAnalyzer {
public:
    // Track decision statistics
    static void recordDecision(
        TierStatistics& stats,
        DecisionCode code,
        int tier
    );

    // Generate statistics report
    static std::string generateReport(const TierStatistics& stats);

    // Recommend threshold changes based on statistics
    static TierThresholds recommendThresholds(
        const TierStatistics& stats,
        float target_tier1_rate = 0.70f,
        float target_tier2_rate = 0.20f,
        float target_tier3_rate = 0.10f
    );

    // Analyze tier agreement
    static float computeTierAgreement(
        const std::vector<float>& tier1_confidences,
        const std::vector<float>& tier2_confidences
    );
};

} // namespace bullet_detection
