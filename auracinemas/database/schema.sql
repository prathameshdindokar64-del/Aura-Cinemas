-- ============================================================================
-- AuraCinemas — Database Schema (DDL)
-- High-Throughput Online Movie Ticket Booking Platform for the Indian Market
-- Database: SQLite (WAL mode enabled)
-- ============================================================================
-- Version : 1.0.0
-- Author  : AuraCinemas Engineering
-- Date    : 2026-08-25
-- ============================================================================

PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;
PRAGMA busy_timeout = 5000;

-- ============================================================================
-- 1. USERS
--    Stores customer & admin accounts. Passwords are bcrypt-hashed.
-- ============================================================================
CREATE TABLE IF NOT EXISTS users (
    user_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    full_name       TEXT        NOT NULL,
    email           TEXT        NOT NULL UNIQUE,
    phone           TEXT        NOT NULL UNIQUE,
    password_hash   TEXT        NOT NULL,
    role            TEXT        NOT NULL DEFAULT 'customer'
                                CHECK (role IN ('customer', 'admin')),
    city            TEXT,
    created_at      DATETIME    NOT NULL DEFAULT (datetime('now')),
    updated_at      DATETIME    NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_users_phone ON users(phone);

-- Auto-update the updated_at column on user row changes
CREATE TRIGGER IF NOT EXISTS trg_users_updated_at
AFTER UPDATE ON users
FOR EACH ROW
BEGIN
    UPDATE users SET updated_at = datetime('now') WHERE user_id = OLD.user_id;
END;

-- ============================================================================
-- 2. MOVIES
--    Indian & international titles. rating is on a 0–10 scale.
-- ============================================================================
CREATE TABLE IF NOT EXISTS movies (
    movie_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    title           TEXT        NOT NULL,
    language        TEXT        NOT NULL DEFAULT 'Hindi',
    genre           TEXT        NOT NULL,
    duration_mins   INTEGER     NOT NULL CHECK (duration_mins > 0),
    certification   TEXT        NOT NULL DEFAULT 'U/A'
                                CHECK (certification IN ('U', 'U/A', 'A', 'S')),
    release_date    DATE        NOT NULL,
    synopsis        TEXT,
    poster_url      TEXT,
    trailer_url     TEXT,
    director        TEXT,
    cast_list       TEXT,                   -- Comma-separated actor names
    rating          REAL        DEFAULT 0.0 CHECK (rating >= 0.0 AND rating <= 10.0),
    is_active       INTEGER     NOT NULL DEFAULT 1,
    created_at      DATETIME    NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_movies_title     ON movies(title);
CREATE INDEX idx_movies_language  ON movies(language);
CREATE INDEX idx_movies_active    ON movies(is_active);

-- ============================================================================
-- 3. CITIES
--    Indian metro & tier-2 cities where AuraCinemas operates.
-- ============================================================================
CREATE TABLE IF NOT EXISTS cities (
    city_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    city_name   TEXT    NOT NULL UNIQUE,
    state       TEXT    NOT NULL
);

-- ============================================================================
-- 4. THEATERS (Cinemas / Multiplexes)
--    Each theater belongs to a city and has multiple screens.
-- ============================================================================
CREATE TABLE IF NOT EXISTS theaters (
    theater_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    theater_name    TEXT        NOT NULL,
    city_id         INTEGER     NOT NULL,
    address         TEXT,
    latitude        REAL,
    longitude       REAL,
    total_screens   INTEGER     NOT NULL DEFAULT 1,
    is_active       INTEGER     NOT NULL DEFAULT 1,
    created_at      DATETIME    NOT NULL DEFAULT (datetime('now')),

    FOREIGN KEY (city_id) REFERENCES cities(city_id)
);

CREATE INDEX idx_theaters_city ON theaters(city_id);

-- ============================================================================
-- 5. SCREENS (Auditoriums within a theater)
--    Each screen has a type (IMAX, Dolby Atmos, etc.) and a capacity.
-- ============================================================================
CREATE TABLE IF NOT EXISTS screens (
    screen_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    theater_id      INTEGER     NOT NULL,
    screen_name     TEXT        NOT NULL,       -- e.g., "Audi 1", "IMAX", "Screen 3"
    screen_type     TEXT        NOT NULL DEFAULT 'Standard'
                                CHECK (screen_type IN ('Standard', 'IMAX', 'Dolby Atmos', '4DX', 'ScreenX')),
    total_rows      INTEGER     NOT NULL CHECK (total_rows > 0),
    seats_per_row   INTEGER     NOT NULL CHECK (seats_per_row > 0),
    total_capacity  INTEGER     NOT NULL CHECK (total_capacity > 0),

    FOREIGN KEY (theater_id) REFERENCES theaters(theater_id)
);

CREATE INDEX idx_screens_theater ON screens(theater_id);

-- ============================================================================
-- 6. SEAT CATEGORIES & PRICING
--    Defines tier names, base INR prices, and UI colours.
-- ============================================================================
CREATE TABLE IF NOT EXISTS seat_categories (
    category_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    category_name   TEXT        NOT NULL UNIQUE
                                CHECK (category_name IN ('Standard', 'Premium', 'Recliner', 'VIP', 'Balcony')),
    base_price      REAL        NOT NULL CHECK (base_price > 0),  -- INR
    color_hex       TEXT        DEFAULT '#6366F1'                  -- UI rendering color
);

-- ============================================================================
-- 7. SEATS (Individual seat map for each screen)
--    Each row of seats belongs to a category (Standard, Premium, etc.).
-- ============================================================================
CREATE TABLE IF NOT EXISTS seats (
    seat_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    screen_id       INTEGER     NOT NULL,
    category_id     INTEGER     NOT NULL,
    row_label       TEXT        NOT NULL,       -- e.g., "A", "B", "C" …
    seat_number     INTEGER     NOT NULL,       -- e.g., 1, 2, 3 …
    is_available    INTEGER     NOT NULL DEFAULT 1,

    UNIQUE (screen_id, row_label, seat_number),
    FOREIGN KEY (screen_id)   REFERENCES screens(screen_id),
    FOREIGN KEY (category_id) REFERENCES seat_categories(category_id)
);

CREATE INDEX idx_seats_screen   ON seats(screen_id);
CREATE INDEX idx_seats_category ON seats(category_id);

-- ============================================================================
-- 8. SHOWS (Showtimes linking Movies ↔ Screens)
--    A show = one screening of a movie on a specific screen at a set time.
-- ============================================================================
CREATE TABLE IF NOT EXISTS shows (
    show_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    movie_id        INTEGER     NOT NULL,
    screen_id       INTEGER     NOT NULL,
    show_date       DATE        NOT NULL,
    start_time      TIME        NOT NULL,
    end_time        TIME        NOT NULL,
    is_active       INTEGER     NOT NULL DEFAULT 1,
    created_at      DATETIME    NOT NULL DEFAULT (datetime('now')),

    FOREIGN KEY (movie_id)  REFERENCES movies(movie_id),
    FOREIGN KEY (screen_id) REFERENCES screens(screen_id)
);

CREATE INDEX idx_shows_movie      ON shows(movie_id);
CREATE INDEX idx_shows_screen     ON shows(screen_id);
CREATE INDEX idx_shows_date       ON shows(show_date);
CREATE INDEX idx_shows_movie_date ON shows(movie_id, show_date);

-- ============================================================================
-- 9. SHOW SEATS (Per-show seat availability snapshot)
--    This is the CORE table for concurrency-safe booking.
--    Each row = one seat for one specific show.
--    Status transitions: available → locked → booked   (happy path)
--                        locked   → available           (lock expired)
--                        booked   → available           (cancellation)
-- ============================================================================
CREATE TABLE IF NOT EXISTS show_seats (
    show_seat_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    show_id         INTEGER     NOT NULL,
    seat_id         INTEGER     NOT NULL,
    status          TEXT        NOT NULL DEFAULT 'available'
                                CHECK (status IN ('available', 'locked', 'booked', 'blocked')),
    locked_until    DATETIME,                  -- Temporary hold timestamp
    locked_by       INTEGER,                   -- user_id holding the lock
    booked_price    REAL,                      -- Final price after dynamic pricing

    UNIQUE (show_id, seat_id),
    FOREIGN KEY (show_id) REFERENCES shows(show_id),
    FOREIGN KEY (seat_id) REFERENCES seats(seat_id)
);

CREATE INDEX idx_show_seats_show    ON show_seats(show_id);
CREATE INDEX idx_show_seats_status  ON show_seats(status);

-- ============================================================================
-- 10. BOOKINGS
--     One booking = one transaction by one user for one show.
--     Grand total = surge_total + tax_amount + convenience_fee.
-- ============================================================================
CREATE TABLE IF NOT EXISTS bookings (
    booking_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id         INTEGER     NOT NULL,
    show_id         INTEGER     NOT NULL,
    num_seats       INTEGER     NOT NULL CHECK (num_seats > 0),
    base_total      REAL        NOT NULL,      -- Sum of base prices
    surge_total     REAL        NOT NULL,      -- Sum after dynamic pricing
    tax_amount      REAL        NOT NULL DEFAULT 0.0,  -- GST (18%)
    convenience_fee REAL        NOT NULL DEFAULT 0.0,
    grand_total     REAL        NOT NULL,      -- Final payable amount
    booking_status  TEXT        NOT NULL DEFAULT 'confirmed'
                                CHECK (booking_status IN ('pending', 'confirmed', 'cancelled', 'expired')),
    booked_at       DATETIME    NOT NULL DEFAULT (datetime('now')),
    cancelled_at    DATETIME,

    FOREIGN KEY (user_id) REFERENCES users(user_id),
    FOREIGN KEY (show_id) REFERENCES shows(show_id)
);

CREATE INDEX idx_bookings_user   ON bookings(user_id);
CREATE INDEX idx_bookings_show   ON bookings(show_id);
CREATE INDEX idx_bookings_status ON bookings(booking_status);

-- ============================================================================
-- 11. BOOKING_SEATS (Many-to-Many: Booking ↔ Seats booked)
--     Records which exact seats were booked, at what price.
-- ============================================================================
CREATE TABLE IF NOT EXISTS booking_seats (
    booking_seat_id INTEGER PRIMARY KEY AUTOINCREMENT,
    booking_id      INTEGER     NOT NULL,
    show_seat_id    INTEGER     NOT NULL,
    seat_label      TEXT        NOT NULL,      -- e.g., "A-12" for display
    category_name   TEXT        NOT NULL,
    base_price      REAL        NOT NULL,
    dynamic_price   REAL        NOT NULL,      -- Price after surge calculation

    UNIQUE (booking_id, show_seat_id),
    FOREIGN KEY (booking_id)   REFERENCES bookings(booking_id),
    FOREIGN KEY (show_seat_id) REFERENCES show_seats(show_seat_id)
);

CREATE INDEX idx_booking_seats_booking ON booking_seats(booking_id);

-- ============================================================================
-- 12. PAYMENTS
--     One payment per booking. Supports Indian payment methods.
-- ============================================================================
CREATE TABLE IF NOT EXISTS payments (
    payment_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    booking_id      INTEGER     NOT NULL UNIQUE,
    payment_method  TEXT        NOT NULL DEFAULT 'UPI'
                                CHECK (payment_method IN ('UPI', 'RuPay', 'Credit Card', 'Debit Card', 'Net Banking', 'Wallet')),
    payment_gateway TEXT,                      -- e.g., "Razorpay", "Paytm", "PhonePe"
    transaction_ref TEXT        UNIQUE,
    amount_paid     REAL        NOT NULL,
    currency        TEXT        NOT NULL DEFAULT 'INR',
    payment_status  TEXT        NOT NULL DEFAULT 'success'
                                CHECK (payment_status IN ('pending', 'success', 'failed', 'refunded')),
    paid_at         DATETIME    NOT NULL DEFAULT (datetime('now')),

    FOREIGN KEY (booking_id) REFERENCES bookings(booking_id)
);

CREATE INDEX idx_payments_booking ON payments(booking_id);
CREATE INDEX idx_payments_status  ON payments(payment_status);

-- ============================================================================
-- 13. PRICING RULES (Configurable surge / discount parameters)
--     rule_type: temporal (time-based), occupancy (fill-based), special (events)
-- ============================================================================
CREATE TABLE IF NOT EXISTS pricing_rules (
    rule_id             INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_name           TEXT    NOT NULL UNIQUE,
    rule_type           TEXT    NOT NULL CHECK (rule_type IN ('temporal', 'occupancy', 'special')),
    condition_desc      TEXT    NOT NULL,       -- Human-readable condition
    modifier_percent    REAL    NOT NULL,       -- +ve = surcharge, -ve = discount
    priority            INTEGER NOT NULL DEFAULT 0,
    is_active           INTEGER NOT NULL DEFAULT 1,
    created_at          DATETIME NOT NULL DEFAULT (datetime('now'))
);

-- ============================================================================
-- 14. SURGE ANALYTICS (Real-time demand tracking per show)
--     Periodic snapshots for the dynamic-pricing engine.
-- ============================================================================
CREATE TABLE IF NOT EXISTS surge_analytics (
    analytics_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    show_id             INTEGER     NOT NULL,
    snapshot_time       DATETIME    NOT NULL DEFAULT (datetime('now')),
    total_capacity      INTEGER     NOT NULL,
    booked_count        INTEGER     NOT NULL DEFAULT 0,
    locked_count        INTEGER     NOT NULL DEFAULT 0,
    occupancy_percent   REAL        NOT NULL DEFAULT 0.0,
    surge_multiplier    REAL        NOT NULL DEFAULT 1.0,
    avg_dynamic_price   REAL,
    requests_per_min    INTEGER     DEFAULT 0,

    FOREIGN KEY (show_id) REFERENCES shows(show_id)
);

CREATE INDEX idx_surge_show ON surge_analytics(show_id);
CREATE INDEX idx_surge_time ON surge_analytics(snapshot_time);

-- ============================================================================
-- 15. REVENUE ANALYTICS (Aggregated daily revenue stats)
--     Used by the admin dashboard for business intelligence.
-- ============================================================================
CREATE TABLE IF NOT EXISTS revenue_analytics (
    revenue_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    report_date         DATE        NOT NULL,
    city_id             INTEGER,
    theater_id          INTEGER,
    movie_id            INTEGER,
    total_bookings      INTEGER     NOT NULL DEFAULT 0,
    total_revenue       REAL        NOT NULL DEFAULT 0.0,
    avg_occupancy       REAL        NOT NULL DEFAULT 0.0,
    avg_surge           REAL        NOT NULL DEFAULT 1.0,
    peak_surge          REAL        NOT NULL DEFAULT 1.0,

    FOREIGN KEY (city_id)    REFERENCES cities(city_id),
    FOREIGN KEY (theater_id) REFERENCES theaters(theater_id),
    FOREIGN KEY (movie_id)   REFERENCES movies(movie_id)
);

CREATE INDEX idx_revenue_date ON revenue_analytics(report_date);

-- ============================================================================
-- END OF SCHEMA — 15 tables, 27 indices, 1 trigger
-- ============================================================================
