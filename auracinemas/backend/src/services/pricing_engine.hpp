// ============================================================================
// AuraCinemas — Dynamic Pricing Engine
// Thread-safe surge pricing based on occupancy, time, and demand velocity.
// ============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

namespace aura {

struct PricingRule {
    int         rule_id;
    std::string rule_name;
    std::string rule_type;       // "temporal" | "occupancy" | "special"
    std::string condition_desc;
    double      modifier_percent;
    int         priority;
    bool        is_active;
};

struct ShowContext {
    int    total_capacity   = 0;
    int    booked_count     = 0;
    int    locked_count     = 0;
    int    requests_per_min = 0;
    std::string show_date;       // "YYYY-MM-DD"
    std::string start_time;      // "HH:MM"

    [[nodiscard]] double occupancy_percent() const {
        if (total_capacity <= 0) return 0.0;
        return (double(booked_count + locked_count) / double(total_capacity)) * 100.0;
    }
};

struct PricingResult {
    double base_price       = 0.0;
    double surge_multiplier = 1.0;
    double dynamic_price    = 0.0;
    double tax_amount       = 0.0;
    double convenience_fee  = 0.0;
    double final_price      = 0.0;
    std::vector<std::string> applied_rules;
};

class PricingEngine {
public:
    static constexpr double GST_RATE             = 0.18;
    static constexpr double CONVENIENCE_FEE      = 30.0;
    static constexpr double MAX_SURGE_MULTIPLIER = 3.0;
    static constexpr double MIN_SURGE_MULTIPLIER = 0.70;

    PricingEngine() = default;
    explicit PricingEngine(std::vector<PricingRule> rules)
        : rules_(std::move(rules)) { sort_rules(); }

    void load_rules(std::vector<PricingRule> rules) {
        std::lock_guard<std::mutex> lock(mu_);
        rules_ = std::move(rules);
        sort_rules();
    }

    [[nodiscard]]
    PricingResult calculate_price(double base_price, const ShowContext& ctx) const {
        std::lock_guard<std::mutex> lock(mu_);
        PricingResult r;
        r.base_price = base_price;
        double total_mod = 0.0;

        for (const auto& rule : rules_) {
            if (!rule.is_active) continue;
            if (evaluate_rule(rule, ctx)) {
                total_mod += rule.modifier_percent;
                r.applied_rules.push_back(rule.rule_name);
            }
        }

        // Demand velocity bonus
        double vb = velocity_bonus(ctx.requests_per_min);
        if (vb > 0.0) total_mod += vb;

        r.surge_multiplier = std::clamp(1.0 + total_mod / 100.0,
                                        MIN_SURGE_MULTIPLIER, MAX_SURGE_MULTIPLIER);
        r.dynamic_price    = std::round(base_price * r.surge_multiplier);
        r.tax_amount       = std::round(r.dynamic_price * GST_RATE * 100.0) / 100.0;
        r.convenience_fee  = CONVENIENCE_FEE;
        r.final_price      = r.dynamic_price + r.tax_amount + r.convenience_fee;
        return r;
    }

private:
    mutable std::mutex       mu_;
    std::vector<PricingRule>  rules_;

    void sort_rules() {
        std::sort(rules_.begin(), rules_.end(),
                  [](auto& a, auto& b){ return a.priority < b.priority; });
    }

    [[nodiscard]] bool evaluate_rule(const PricingRule& rule, const ShowContext& ctx) const {
        if (rule.rule_type == "occupancy") {
            double occ = ctx.occupancy_percent();
            if (rule.rule_name == "Low Occupancy")    return occ < 40.0;
            if (rule.rule_name == "Medium Occupancy")  return occ >= 40.0 && occ < 70.0;
            if (rule.rule_name == "High Occupancy")    return occ >= 70.0 && occ < 90.0;
            if (rule.rule_name == "Surge Demand")      return occ >= 90.0;
        }
        if (rule.rule_type == "temporal") {
            if (rule.rule_name == "Weekend Surge")     return is_weekend(ctx.show_date);
            if (rule.rule_name == "Prime Time Surge")  return is_prime_time(ctx.start_time);
            if (rule.rule_name == "Matinee Discount")  return is_matinee(ctx.start_time);
        }
        if (rule.rule_type == "special") {
            if (rule.rule_name == "National Holiday")  return is_national_holiday(ctx.show_date);
        }
        return false;
    }

    static bool is_weekend(const std::string& d) {
        if (d.size() < 10) return false;
        std::tm tm{};
        tm.tm_year = std::stoi(d.substr(0,4)) - 1900;
        tm.tm_mon  = std::stoi(d.substr(5,2)) - 1;
        tm.tm_mday = std::stoi(d.substr(8,2));
        std::mktime(&tm);
        return tm.tm_wday == 0 || tm.tm_wday == 5 || tm.tm_wday == 6;
    }

    static bool is_prime_time(const std::string& t) {
        if (t.size() < 5) return false;
        int mins = std::stoi(t.substr(0,2)) * 60 + std::stoi(t.substr(3,2));
        return mins >= 1140 && mins <= 1350; // 19:00–22:30
    }

    static bool is_matinee(const std::string& t) {
        if (t.size() < 5) return false;
        return std::stoi(t.substr(0,2)) < 12;
    }

    static bool is_national_holiday(const std::string& d) {
        if (d.size() < 10) return false;
        std::string mmdd = d.substr(5,5);
        return mmdd == "01-26" || mmdd == "08-15" || mmdd == "10-02";
    }

    static double velocity_bonus(int rpm) {
        if (rpm > 200) return 15.0;
        if (rpm > 100) return 8.0;
        if (rpm > 50)  return 3.0;
        return 0.0;
    }
};

} // namespace aura
