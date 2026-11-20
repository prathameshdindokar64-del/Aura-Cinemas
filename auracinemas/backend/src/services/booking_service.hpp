// ============================================================================
// AuraCinemas — Thread-Safe Booking Service
// Manages the seat-locking → booking → payment lifecycle with SQLite
// transactions. Uses SERIALIZABLE isolation via BEGIN IMMEDIATE.
// ============================================================================
#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "pricing_engine.hpp"

// Forward-declare sqlite3 so we don't leak the C header into every TU
struct sqlite3;
struct sqlite3_stmt;

namespace aura {

// ---------------------------------------------------------------------------
// Data transfer objects
// ---------------------------------------------------------------------------
struct SeatSelection {
    int    seat_id      = 0;
    int    show_seat_id = 0;
    double base_price   = 0.0;
    std::string seat_label;    // "A-12"
    std::string category_name; // "Premium"
};

struct BookingRequest {
    int user_id  = 0;
    int show_id  = 0;
    std::vector<SeatSelection> seats;
    std::string payment_method;   // "UPI", "Credit Card", etc.
    std::string payment_gateway;  // "Razorpay", "PhonePe", etc.
};

struct BookingReceipt {
    int64_t     booking_id     = 0;
    int64_t     payment_id     = 0;
    std::string transaction_ref;
    double      base_total     = 0.0;
    double      surge_total    = 0.0;
    double      tax_amount     = 0.0;
    double      convenience_fee= 0.0;
    double      grand_total    = 0.0;
    std::string status;           // "confirmed"
    std::vector<PricingResult> seat_prices;
};

enum class BookingError {
    None,
    SeatsUnavailable,
    LockExpired,
    PaymentFailed,
    DatabaseError,
    InvalidRequest,
    ShowNotFound
};

struct BookingOutcome {
    BookingError                  error = BookingError::None;
    std::string                   message;
    std::optional<BookingReceipt> receipt;

    [[nodiscard]] bool ok() const { return error == BookingError::None; }
};

// ---------------------------------------------------------------------------
// Lock record for the temporary seat hold
// ---------------------------------------------------------------------------
struct SeatLock {
    int    show_id;
    int    user_id;
    std::vector<int> seat_ids;
    std::chrono::steady_clock::time_point expires_at;
};

// ============================================================================
// BookingService
// ============================================================================
class BookingService {
public:
    // Lock duration: 8 minutes to complete payment
    static constexpr int LOCK_DURATION_SECONDS = 480;
    // Max seats per single booking
    static constexpr int MAX_SEATS_PER_BOOKING = 10;

    // -----------------------------------------------------------------------
    // Constructor — takes a raw sqlite3* (caller manages DB lifetime)
    // -----------------------------------------------------------------------
    explicit BookingService(sqlite3* db, PricingEngine& engine)
        : db_(db), pricing_(engine) {}

    // -----------------------------------------------------------------------
    // Step 1: Lock seats — temporary hold before payment
    // Returns a lock_id on success, or an error.
    // -----------------------------------------------------------------------
    BookingOutcome lock_seats(int user_id, int show_id,
                              const std::vector<int>& seat_ids) {
        if (seat_ids.empty() || (int)seat_ids.size() > MAX_SEATS_PER_BOOKING) {
            return {BookingError::InvalidRequest,
                    "Select 1–" + std::to_string(MAX_SEATS_PER_BOOKING) + " seats."};
        }

        std::unique_lock<std::shared_mutex> lock(rw_mu_);

        // BEGIN IMMEDIATE — acquire a reserved lock on the DB immediately,
        // preventing other writers from proceeding (SERIALIZABLE behaviour).
        if (!exec_sql("BEGIN IMMEDIATE TRANSACTION")) {
            return {BookingError::DatabaseError, "Failed to begin transaction."};
        }

        // Verify all requested seats are 'available'
        for (int sid : seat_ids) {
            std::string status = get_show_seat_status(show_id, sid);
            if (status != "available") {
                exec_sql("ROLLBACK");
                return {BookingError::SeatsUnavailable,
                        "Seat " + std::to_string(sid) + " is " + status + "."};
            }
        }

        // Mark seats as 'locked'
        auto now_str = current_utc_datetime();
        auto expiry  = current_utc_plus_seconds(LOCK_DURATION_SECONDS);

        for (int sid : seat_ids) {
            std::string sql =
                "UPDATE show_seats SET status = 'locked', "
                "locked_by = " + std::to_string(user_id) + ", "
                "locked_until = '" + expiry + "' "
                "WHERE show_id = " + std::to_string(show_id) +
                " AND seat_id = " + std::to_string(sid) +
                " AND status = 'available'";

            if (!exec_sql(sql)) {
                exec_sql("ROLLBACK");
                return {BookingError::DatabaseError, "Failed to lock seat."};
            }
        }

        if (!exec_sql("COMMIT")) {
            exec_sql("ROLLBACK");
            return {BookingError::DatabaseError, "Commit failed."};
        }

        // Store in-memory lock record
        {
            std::lock_guard<std::mutex> lk(locks_mu_);
            std::string key = make_lock_key(user_id, show_id);
            locks_[key] = SeatLock{
                show_id, user_id, seat_ids,
                std::chrono::steady_clock::now() +
                    std::chrono::seconds(LOCK_DURATION_SECONDS)
            };
        }

        BookingReceipt receipt;
        receipt.status = "locked";
        return {BookingError::None, "Seats locked for 8 minutes.", receipt};
    }

    // -----------------------------------------------------------------------
    // Step 2: Confirm booking — convert locked seats to booked
    // -----------------------------------------------------------------------
    BookingOutcome confirm_booking(const BookingRequest& req) {
        if (req.seats.empty()) {
            return {BookingError::InvalidRequest, "No seats selected."};
        }

        std::unique_lock<std::shared_mutex> lock(rw_mu_);

        if (!exec_sql("BEGIN IMMEDIATE TRANSACTION")) {
            return {BookingError::DatabaseError, "Failed to begin transaction."};
        }

        // Verify locks are still valid
        for (const auto& s : req.seats) {
            std::string status = get_show_seat_status(req.show_id, s.seat_id);
            if (status != "locked") {
                exec_sql("ROLLBACK");
                return {BookingError::LockExpired,
                        "Seat " + s.seat_label + " lock expired."};
            }
        }

        // --- Price calculation via PricingEngine ---
        ShowContext ctx = build_show_context(req.show_id);
        std::vector<double> base_prices;
        base_prices.reserve(req.seats.size());
        for (const auto& s : req.seats) {
            base_prices.push_back(s.base_price);
        }

        auto bulk = pricing_.calculate_bulk(base_prices, ctx);

        // --- Insert booking row ---
        std::string ins_booking =
            "INSERT INTO bookings (user_id, show_id, num_seats, "
            "base_total, surge_total, tax_amount, convenience_fee, "
            "grand_total, booking_status) VALUES ("
            + std::to_string(req.user_id) + ", "
            + std::to_string(req.show_id) + ", "
            + std::to_string(req.seats.size()) + ", "
            + std::to_string(bulk.total_base) + ", "
            + std::to_string(bulk.total_surge) + ", "
            + std::to_string(bulk.total_tax) + ", "
            + std::to_string(bulk.total_convenience) + ", "
            + std::to_string(bulk.grand_total) + ", 'confirmed')";

        if (!exec_sql(ins_booking)) {
            exec_sql("ROLLBACK");
            return {BookingError::DatabaseError, "Failed to create booking."};
        }

        int64_t booking_id = last_insert_rowid();

        // --- Insert booking_seats rows & update show_seats to 'booked' ---
        for (size_t i = 0; i < req.seats.size(); ++i) {
            const auto& s  = req.seats[i];
            const auto& pr = bulk.per_seat[i];

            // Get show_seat_id
            int show_seat_id = get_show_seat_id(req.show_id, s.seat_id);
            if (show_seat_id <= 0) {
                exec_sql("ROLLBACK");
                return {BookingError::DatabaseError, "show_seat not found."};
            }

            // Mark as booked
            std::string upd =
                "UPDATE show_seats SET status = 'booked', "
                "booked_price = " + std::to_string(pr.dynamic_price) +
                " WHERE show_seat_id = " + std::to_string(show_seat_id);
            exec_sql(upd);

            // Insert booking_seats
            std::string ins_bs =
                "INSERT INTO booking_seats (booking_id, show_seat_id, "
                "seat_label, category_name, base_price, dynamic_price) VALUES ("
                + std::to_string(booking_id) + ", "
                + std::to_string(show_seat_id) + ", '"
                + s.seat_label + "', '"
                + s.category_name + "', "
                + std::to_string(s.base_price) + ", "
                + std::to_string(pr.dynamic_price) + ")";
            exec_sql(ins_bs);
        }

        // --- Insert payment record ---
        std::string txn_ref = generate_txn_ref(booking_id);
        std::string ins_pay =
            "INSERT INTO payments (booking_id, payment_method, "
            "payment_gateway, transaction_ref, amount_paid, "
            "payment_status) VALUES ("
            + std::to_string(booking_id) + ", '"
            + req.payment_method + "', '"
            + req.payment_gateway + "', '"
            + txn_ref + "', "
            + std::to_string(bulk.grand_total) + ", 'success')";
        exec_sql(ins_pay);

        int64_t payment_id = last_insert_rowid();

        if (!exec_sql("COMMIT")) {
            exec_sql("ROLLBACK");
            return {BookingError::DatabaseError, "Commit failed."};
        }

        // Clean up in-memory lock
        {
            std::lock_guard<std::mutex> lk(locks_mu_);
            locks_.erase(make_lock_key(req.user_id, req.show_id));
        }

        BookingReceipt receipt;
        receipt.booking_id      = booking_id;
        receipt.payment_id      = payment_id;
        receipt.transaction_ref = txn_ref;
        receipt.base_total      = bulk.total_base;
        receipt.surge_total     = bulk.total_surge;
        receipt.tax_amount      = bulk.total_tax;
        receipt.convenience_fee = bulk.total_convenience;
        receipt.grand_total     = bulk.grand_total;
        receipt.status          = "confirmed";
        receipt.seat_prices     = std::move(bulk.per_seat);

        return {BookingError::None, "Booking confirmed!", receipt};
    }

    // -----------------------------------------------------------------------
    // Cancel booking — refund seats back to available
    // -----------------------------------------------------------------------
    BookingOutcome cancel_booking(int booking_id, int user_id) {
        std::unique_lock<std::shared_mutex> lock(rw_mu_);

        if (!exec_sql("BEGIN IMMEDIATE TRANSACTION")) {
            return {BookingError::DatabaseError, "Failed to begin transaction."};
        }

        // Release booked show_seats back to available
        std::string upd =
            "UPDATE show_seats SET status = 'available', "
            "booked_price = NULL WHERE show_seat_id IN ("
            "  SELECT show_seat_id FROM booking_seats "
            "  WHERE booking_id = " + std::to_string(booking_id) + ")";
        exec_sql(upd);

        // Mark booking as cancelled
        std::string upd_bk =
            "UPDATE bookings SET booking_status = 'cancelled', "
            "cancelled_at = datetime('now') "
            "WHERE booking_id = " + std::to_string(booking_id) +
            " AND user_id = " + std::to_string(user_id);
        exec_sql(upd_bk);

        // Refund payment
        std::string upd_pay =
            "UPDATE payments SET payment_status = 'refunded' "
            "WHERE booking_id = " + std::to_string(booking_id);
        exec_sql(upd_pay);

        if (!exec_sql("COMMIT")) {
            exec_sql("ROLLBACK");
            return {BookingError::DatabaseError, "Commit failed."};
        }

        return {BookingError::None, "Booking cancelled and refund initiated."};
    }

    // -----------------------------------------------------------------------
    // Cleanup expired locks — call periodically from a background timer
    // -----------------------------------------------------------------------
    void release_expired_locks() {
        std::unique_lock<std::shared_mutex> lock(rw_mu_);
        exec_sql(
            "UPDATE show_seats SET status = 'available', "
            "locked_by = NULL, locked_until = NULL "
            "WHERE status = 'locked' AND locked_until < datetime('now')"
        );

        // Clean in-memory locks
        std::lock_guard<std::mutex> lk(locks_mu_);
        auto now = std::chrono::steady_clock::now();
        for (auto it = locks_.begin(); it != locks_.end(); ) {
            if (it->second.expires_at <= now) it = locks_.erase(it);
            else ++it;
        }
    }

    // -----------------------------------------------------------------------
    // Read-only: get seat map for a show (used by the frontend)
    // -----------------------------------------------------------------------
    struct SeatView {
        int seat_id; int show_seat_id;
        std::string row_label; int seat_number;
        std::string category; std::string status;
        double base_price;
    };

    std::vector<SeatView> get_seat_map(int show_id) {
        std::shared_lock<std::shared_mutex> lock(rw_mu_);
        std::vector<SeatView> result;

        std::string sql =
            "SELECT s.seat_id, ss.show_seat_id, s.row_label, s.seat_number, "
            "sc.category_name, ss.status, sc.base_price "
            "FROM show_seats ss "
            "JOIN seats s ON ss.seat_id = s.seat_id "
            "JOIN seat_categories sc ON s.category_id = sc.category_id "
            "WHERE ss.show_id = " + std::to_string(show_id) +
            " ORDER BY s.row_label, s.seat_number";

        sqlite3_stmt* stmt = nullptr;
        if (prepare_stmt(sql, &stmt)) {
            while (step_stmt(stmt)) {
                SeatView sv;
                sv.seat_id      = col_int(stmt, 0);
                sv.show_seat_id = col_int(stmt, 1);
                sv.row_label    = col_text(stmt, 2);
                sv.seat_number  = col_int(stmt, 3);
                sv.category     = col_text(stmt, 4);
                sv.status       = col_text(stmt, 5);
                sv.base_price   = col_double(stmt, 6);
                result.push_back(sv);
            }
            finalize_stmt(stmt);
        }
        return result;
    }

private:
    sqlite3*         db_;
    PricingEngine&   pricing_;
    mutable std::shared_mutex rw_mu_;   // readers-writer lock for DB access
    std::mutex       locks_mu_;          // protects locks_ map
    std::unordered_map<std::string, SeatLock> locks_;

    // -----------------------------------------------------------------------
    // SQLite helpers (thin wrappers)
    // -----------------------------------------------------------------------
    bool exec_sql(const std::string& sql);       // sqlite3_exec wrapper
    bool prepare_stmt(const std::string& sql, sqlite3_stmt** out);
    bool step_stmt(sqlite3_stmt* stmt);           // returns true if SQLITE_ROW
    void finalize_stmt(sqlite3_stmt* stmt);
    int64_t last_insert_rowid();
    int         col_int(sqlite3_stmt* s, int i);
    double      col_double(sqlite3_stmt* s, int i);
    std::string col_text(sqlite3_stmt* s, int i);

    // -----------------------------------------------------------------------
    // Internal queries
    // -----------------------------------------------------------------------
    std::string get_show_seat_status(int show_id, int seat_id) {
        std::string sql =
            "SELECT status FROM show_seats "
            "WHERE show_id = " + std::to_string(show_id) +
            " AND seat_id = " + std::to_string(seat_id);
        sqlite3_stmt* stmt = nullptr;
        std::string status = "unknown";
        if (prepare_stmt(sql, &stmt) && step_stmt(stmt)) {
            status = col_text(stmt, 0);
        }
        if (stmt) finalize_stmt(stmt);
        return status;
    }

    int get_show_seat_id(int show_id, int seat_id) {
        std::string sql =
            "SELECT show_seat_id FROM show_seats "
            "WHERE show_id = " + std::to_string(show_id) +
            " AND seat_id = " + std::to_string(seat_id);
        sqlite3_stmt* stmt = nullptr;
        int id = 0;
        if (prepare_stmt(sql, &stmt) && step_stmt(stmt)) {
            id = col_int(stmt, 0);
        }
        if (stmt) finalize_stmt(stmt);
        return id;
    }

    ShowContext build_show_context(int show_id) {
        ShowContext ctx;
        std::string sql =
            "SELECT s.show_date, s.start_time, sc.total_capacity, "
            "SUM(CASE WHEN ss.status='booked' THEN 1 ELSE 0 END), "
            "SUM(CASE WHEN ss.status='locked' THEN 1 ELSE 0 END) "
            "FROM shows s "
            "JOIN screens sc ON s.screen_id = sc.screen_id "
            "JOIN show_seats ss ON s.show_id = ss.show_id "
            "WHERE s.show_id = " + std::to_string(show_id) +
            " GROUP BY s.show_id";
        sqlite3_stmt* stmt = nullptr;
        if (prepare_stmt(sql, &stmt) && step_stmt(stmt)) {
            ctx.show_date       = col_text(stmt, 0);
            ctx.start_time      = col_text(stmt, 1);
            ctx.total_capacity  = col_int(stmt, 2);
            ctx.booked_count    = col_int(stmt, 3);
            ctx.locked_count    = col_int(stmt, 4);
        }
        if (stmt) finalize_stmt(stmt);
        return ctx;
    }

    // -----------------------------------------------------------------------
    // Utility
    // -----------------------------------------------------------------------
    static std::string make_lock_key(int user_id, int show_id) {
        return std::to_string(user_id) + ":" + std::to_string(show_id);
    }

    static std::string generate_txn_ref(int64_t booking_id) {
        auto now = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", std::gmtime(&now));
        return "TXN" + std::string(buf) + "B" + std::to_string(booking_id);
    }

    static std::string current_utc_datetime() {
        auto now = std::time(nullptr);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::gmtime(&now));
        return buf;
    }

    static std::string current_utc_plus_seconds(int secs) {
        auto now = std::time(nullptr) + secs;
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::gmtime(&now));
        return buf;
    }
};

} // namespace aura
