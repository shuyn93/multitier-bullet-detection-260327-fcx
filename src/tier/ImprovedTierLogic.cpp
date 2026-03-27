#include "tier/ImprovedTierLogic.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace bullet_detection {

// ===== IMPROVED TIER LOGIC IMPLEMENTATION =====

ImprovedTierLogic::ImprovedTierLogic(const TierThresholds& thresholds)
    : thresholds_(thresholds) {}

DecisionCode ImprovedTierLogic::confidenceToDecisionCode(
    float confidence,
    bool is_tier1_decision,
    float* out_margin
) const {
    const TierThresholds& t = thresholds_;
    float threshold_strong_accept = is_tier1_decision ? t.tier1_strong_accept : t.tier2_strong_accept;
    float threshold_weak_accept = is_tier1_decision ? t.tier1_weak_accept : t.tier2_weak_accept;
    float threshold_weak_reject = is_tier1_decision ? t.tier1_weak_reject : t.tier2_weak_reject;
    float threshold_strong_reject = is_tier1_decision ? t.tier1_strong_reject : t.tier2_strong_reject;

    float margin = computeMargin(confidence);
    if (out_margin) *out_margin = margin;

    // Strong decisions (high margin)
    if (confidence >= threshold_strong_accept) {
        return DecisionCode::ACCEPT;
    } else if (confidence <= threshold_strong_reject) {
        return DecisionCode::REJECT;
    }
    // Weak decisions (low margin) - escalate for additional confirmation
    else if (confidence >= threshold_weak_accept) {
        return DecisionCode::ACCEPT;  // But borderline
    } else if (confidence <= threshold_weak_reject) {
        return DecisionCode::REJECT;  // But borderline
    }
    // Uncertain zone
    else {
        return DecisionCode::LOW_CONFIDENCE;
    }
}

bool ImprovedTierLogic::shouldEscalateTier1(
    float tier1_confidence,
    const TierThresholds& thresholds
) const {
    float margin = computeMargin(tier1_confidence);

    // Escalate if margin too small (uncertain)
    if (margin < thresholds.escalation_margin) {
        return true;
    }

    // Also escalate weak rejects if enabled
    if (thresholds.escalate_rejects &&
        tier1_confidence < thresholds.tier1_weak_reject &&
        tier1_confidence > thresholds.tier1_strong_reject) {
        return true;
    }

    return false;
}

bool ImprovedTierLogic::shouldEscalateTier2(
    float tier2_confidence,
    const TierThresholds& thresholds
) const {
    float margin = computeMargin(tier2_confidence);

    // Escalate if even smaller margin
    if (margin < thresholds.escalation_weak_margin) {
        return true;
    }

    return false;
}

float ImprovedTierLogic::combineConfidences(
    float tier1_confidence,
    float tier2_confidence,
    const TierThresholds& thresholds
) const {
    // Bayesian-inspired combination
    // Give more weight to tier that's more confident
    
    float t1_margin = computeMargin(tier1_confidence);
    float t2_margin = computeMargin(tier2_confidence);
    
    // Normalize margins to weights (higher margin = more confident)
    float max_margin = 0.5f;  // Maximum possible margin
    float w1 = std::max(0.1f, t1_margin / max_margin);  // Min 10%
    float w2 = std::max(0.1f, t2_margin / max_margin);
    float weight_sum = w1 + w2;
    
    w1 /= weight_sum;
    w2 /= weight_sum;
    
    // Apply configured weights
    w1 *= thresholds.tier1_weight;
    w2 *= thresholds.tier2_weight;
    weight_sum = thresholds.tier1_weight + thresholds.tier2_weight;
    w1 /= weight_sum;
    w2 /= weight_sum;
    
    float combined = w1 * tier1_confidence + w2 * tier2_confidence;
    return std::max(0.0f, std::min(1.0f, combined));
}

ClassifierDecision ImprovedTierLogic::decideTier3SVM(
    int accept_votes,
    int total_svms,
    const TierThresholds& thresholds
) const {
    if (total_svms == 0) {
        return {DecisionCode::LOW_CONFIDENCE, 0.5f, 0.0f, 0.0f, 3};
    }

    // Compute vote ratio
    float vote_ratio = static_cast<float>(accept_votes) / total_svms;
    
    // Compute confidence from vote ratio
    // 0 votes (0.0) ? confidence 0.0 (REJECT)
    // 1.5 votes (0.5) ? confidence 0.5 (UNCERTAIN)
    // 3 votes (1.0) ? confidence 1.0 (ACCEPT)
    float confidence = vote_ratio;
    
    // Compute margin (how decisive was the vote)
    float margin = std::abs(vote_ratio - 0.5f);  // 0 = tied, 0.5 = unanimous
    
    // Decision based on votes
    DecisionCode code;
    if (vote_ratio >= 0.67f) {  // 2+ out of 3
        code = DecisionCode::ACCEPT;
    } else if (vote_ratio <= 0.33f) {  // 1 or fewer out of 3
        code = DecisionCode::REJECT;
    } else {
        code = DecisionCode::LOW_CONFIDENCE;  // Tied or close
    }
    
    // Compute entropy for uncertain cases
    float entropy = 0.0f;
    if (code == DecisionCode::LOW_CONFIDENCE) {
        entropy = -confidence * std::log(std::max(1e-6f, confidence)) -
                  (1.0f - confidence) * std::log(std::max(1e-6f, 1.0f - confidence));
    }

    return {code, confidence, entropy, margin, 3};
}

ClassifierDecision ImprovedTierLogic::makeMultiTierDecision(
    float tier1_confidence,
    float tier2_confidence,
    int tier3_accept_votes,
    int tier3_total_svms,
    TierDecisionContext* out_context
) const {
    const TierThresholds& t = thresholds_;
    
    // Tier 1 decision
    float t1_margin = 0.0f;
    DecisionCode t1_code = confidenceToDecisionCode(tier1_confidence, true, &t1_margin);
    
    // Initialize output context
    if (out_context) {
        out_context->tier1_confidence = tier1_confidence;
        out_context->tier1_margin = t1_margin;
        out_context->tier1_code = t1_code;
    }
    
    // Check if should escalate from Tier 1
    bool escalate_to_tier2 = shouldEscalateTier1(tier1_confidence, t);
    
    if (!escalate_to_tier2) {
        // Tier 1 is confident enough - use its decision
        if (out_context) out_context->final_tier = 1;
        
        float entropy = 0.0f;
        if (t1_code == DecisionCode::LOW_CONFIDENCE) {
            entropy = -tier1_confidence * std::log(std::max(1e-6f, tier1_confidence)) -
                      (1.0f - tier1_confidence) * std::log(std::max(1e-6f, 1.0f - tier1_confidence));
        }
        
        return {t1_code, tier1_confidence, entropy, t1_margin, 1};
    }
    
    // Tier 2 decision
    float t2_margin = 0.0f;
    DecisionCode t2_code = confidenceToDecisionCode(tier2_confidence, false, &t2_margin);
    
    if (out_context) {
        out_context->tier2_confidence = tier2_confidence;
        out_context->tier2_margin = t2_margin;
        out_context->tier2_code = t2_code;
    }
    
    // Check if should escalate from Tier 2
    bool escalate_to_tier3 = shouldEscalateTier2(tier2_confidence, t);
    
    if (!escalate_to_tier3) {
        // Tier 2 is confident - combine with Tier 1
        if (out_context) out_context->final_tier = 2;
        
        float combined_confidence = combineConfidences(tier1_confidence, tier2_confidence, t);
        float combined_margin = computeMargin(combined_confidence);
        
        DecisionCode final_code = confidenceToDecisionCode(combined_confidence, false);
        
        float entropy = 0.0f;
        if (final_code == DecisionCode::LOW_CONFIDENCE) {
            entropy = -combined_confidence * std::log(std::max(1e-6f, combined_confidence)) -
                      (1.0f - combined_confidence) * std::log(std::max(1e-6f, 1.0f - combined_confidence));
        }
        
        return {final_code, combined_confidence, entropy, combined_margin, 2};
    }
    
    // Tier 3 decision (SVM ensemble)
    ClassifierDecision t3_decision = decideTier3SVM(tier3_accept_votes, tier3_total_svms, t);
    
    if (out_context) {
        out_context->final_tier = 3;
        out_context->tier3_vote_ratio = static_cast<float>(tier3_accept_votes) / tier3_total_svms;
        out_context->tier3_accept_votes = tier3_accept_votes;
        out_context->tier3_total_svms = tier3_total_svms;
    }
    
    return t3_decision;
}

void ImprovedTierLogic::setThresholds(const TierThresholds& thresholds) {
    thresholds_ = thresholds;
}

bool ImprovedTierLogic::canEarlyTerminate(
    float confidence,
    const TierThresholds& t
) const {
    float margin = computeMargin(confidence);
    return margin >= t.strong_confidence_margin;
}

// ===== TIER ANALYZER IMPLEMENTATION =====

void TierAnalyzer::recordDecision(
    TierStatistics& stats,
    DecisionCode code,
    int tier
) {
    if (tier == 1) {
        if (code == DecisionCode::ACCEPT) stats.tier1_accept_count++;
        else if (code == DecisionCode::LOW_CONFIDENCE) stats.tier1_escalate_count++;
        else stats.tier1_reject_count++;
    } else if (tier == 2) {
        if (code == DecisionCode::ACCEPT) stats.tier2_accept_count++;
        else if (code == DecisionCode::LOW_CONFIDENCE) stats.tier2_escalate_count++;
        else stats.tier2_reject_count++;
    } else if (tier == 3) {
        if (code == DecisionCode::ACCEPT) stats.tier3_accept_count++;
        else stats.tier3_reject_count++;
    }
}

std::string TierAnalyzer::generateReport(const TierStatistics& stats) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    
    int total_t1 = stats.tier1_accept_count + stats.tier1_escalate_count + stats.tier1_reject_count;
    int total_t2 = stats.tier2_accept_count + stats.tier2_escalate_count + stats.tier2_reject_count;
    int total_t3 = stats.tier3_accept_count + stats.tier3_reject_count;
    
    oss << "=== TIER DECISION STATISTICS ===" << std::endl;
    oss << std::endl << "TIER 1:" << std::endl;
    oss << "  ACCEPT: " << stats.tier1_accept_count << " (" 
        << (total_t1 > 0 ? 100.0f * stats.tier1_accept_count / total_t1 : 0.0f) << "%)" << std::endl;
    oss << "  ESCALATE: " << stats.tier1_escalate_count << " (" 
        << (total_t1 > 0 ? 100.0f * stats.tier1_escalate_count / total_t1 : 0.0f) << "%)" << std::endl;
    oss << "  REJECT: " << stats.tier1_reject_count << " (" 
        << (total_t1 > 0 ? 100.0f * stats.tier1_reject_count / total_t1 : 0.0f) << "%)" << std::endl;
    
    oss << std::endl << "TIER 2 (of escalated):" << std::endl;
    oss << "  ACCEPT: " << stats.tier2_accept_count << " (" 
        << (total_t2 > 0 ? 100.0f * stats.tier2_accept_count / total_t2 : 0.0f) << "%)" << std::endl;
    oss << "  ESCALATE: " << stats.tier2_escalate_count << " (" 
        << (total_t2 > 0 ? 100.0f * stats.tier2_escalate_count / total_t2 : 0.0f) << "%)" << std::endl;
    oss << "  REJECT: " << stats.tier2_reject_count << " (" 
        << (total_t2 > 0 ? 100.0f * stats.tier2_reject_count / total_t2 : 0.0f) << "%)" << std::endl;
    
    oss << std::endl << "TIER 3 (of escalated from Tier 2):" << std::endl;
    oss << "  ACCEPT: " << stats.tier3_accept_count << " (" 
        << (total_t3 > 0 ? 100.0f * stats.tier3_accept_count / total_t3 : 0.0f) << "%)" << std::endl;
    oss << "  REJECT: " << stats.tier3_reject_count << " (" 
        << (total_t3 > 0 ? 100.0f * stats.tier3_reject_count / total_t3 : 0.0f) << "%)" << std::endl;
    
    oss << std::endl << "ESCALATION RATES:" << std::endl;
    oss << "  Tier 1?2: " << stats.tier1_escalation_rate * 100.0f << "%" << std::endl;
    oss << "  Tier 2?3: " << stats.tier2_escalation_rate * 100.0f << "%" << std::endl;
    
    return oss.str();
}

TierThresholds TierAnalyzer::recommendThresholds(
    const TierStatistics& stats,
    float target_tier1_rate,
    float target_tier2_rate,
    float target_tier3_rate
) {
    TierThresholds recommended;
    
    // If Tier 1 escalation rate is too high, increase thresholds
    // If too low, decrease thresholds
    
    float current_t1_rate = stats.tier1_escalation_rate;
    float t1_error = current_t1_rate - target_tier1_rate;
    
    // Adjust threshold proportionally
    if (std::abs(t1_error) > 0.05f) {
        recommended.tier1_strong_accept += t1_error * 0.05f;
        recommended.tier1_strong_reject -= t1_error * 0.05f;
    }
    
    // Similar for Tier 2
    float current_t2_rate = stats.tier2_escalation_rate;
    float t2_error = current_t2_rate - target_tier2_rate;
    
    if (std::abs(t2_error) > 0.05f) {
        recommended.tier2_strong_accept += t2_error * 0.05f;
        recommended.tier2_strong_reject -= t2_error * 0.05f;
    }
    
    return recommended;
}

float TierAnalyzer::computeTierAgreement(
    const std::vector<float>& tier1_confidences,
    const std::vector<float>& tier2_confidences
) {
    if (tier1_confidences.size() != tier2_confidences.size() ||
        tier1_confidences.empty()) {
        return 0.0f;
    }
    
    float agreement = 0.0f;
    for (size_t i = 0; i < tier1_confidences.size(); ++i) {
        float diff = std::abs(tier1_confidences[i] - tier2_confidences[i]);
        agreement += 1.0f - diff;  // Perfect agreement = 1, max disagreement = 0
    }
    
    return agreement / tier1_confidences.size();
}

} // namespace bullet_detection
