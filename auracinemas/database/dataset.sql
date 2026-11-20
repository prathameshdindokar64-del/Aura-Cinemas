-- ============================================================================
-- AuraCinemas — Seed Data (DML)
-- Indian Market: Bollywood/Hollywood movies, Indian cities & theaters
-- ============================================================================

-- ======================== CITIES ========================
INSERT INTO cities (city_name, state) VALUES
('Mumbai',      'Maharashtra'),
('Pune',        'Maharashtra'),
('Bengaluru',   'Karnataka'),
('Delhi',       'Delhi'),
('Hyderabad',   'Telangana'),
('Chennai',     'Tamil Nadu'),
('Kolkata',     'West Bengal'),
('Ahmedabad',   'Gujarat'),
('Jaipur',      'Rajasthan'),
('Lucknow',     'Uttar Pradesh');

-- ======================== SEAT CATEGORIES ========================
INSERT INTO seat_categories (category_name, base_price, color_hex) VALUES
('Standard',  200.00, '#6366F1'),
('Premium',   300.00, '#A855F7'),
('Recliner',  350.00, '#F59E0B'),
('VIP',       500.00, '#EAB308'),
('Balcony',   250.00, '#8B5CF6');

-- ======================== MOVIES ========================
INSERT INTO movies (title, language, genre, duration_mins, certification, release_date, synopsis, director, cast_list, rating) VALUES
('Pathaan',           'Hindi',   'Action, Thriller',    146, 'U/A', '2026-01-25', 'An Indian spy takes on a deadly mercenary with ties to his past.',                       'Siddharth Anand',    'Shah Rukh Khan, Deepika Padukone, John Abraham',       8.2),
('Jawan',             'Hindi',   'Action, Drama',       169, 'U/A', '2026-09-07', 'A man is driven by a personal vendetta to rectify societal wrongs.',                     'Atlee',              'Shah Rukh Khan, Nayanthara, Vijay Sethupathi',          7.9),
('Animal',            'Hindi',   'Crime, Drama',        201, 'A',   '2026-12-01', 'A son undergoes a transformation as his love for his father is tested.',                 'Sandeep Reddy Vanga','Ranbir Kapoor, Anil Kapoor, Rashmika Mandanna',         7.4),
('Dunki',             'Hindi',   'Comedy, Drama',       161, 'U/A', '2026-12-21', 'Four friends from a village in Punjab attempt to illegally migrate abroad.',             'Rajkumar Hirani',   'Shah Rukh Khan, Taapsee Pannu, Vicky Kaushal',          7.1),
('Fighter',           'Hindi',   'Action, Drama',       166, 'U/A', '2026-01-25', 'Indian Air Force pilots defend the nation against an aerial threat.',                    'Siddharth Anand',   'Hrithik Roshan, Deepika Padukone, Anil Kapoor',         6.8),
('Stree 2',           'Hindi',   'Horror, Comedy',      149, 'U/A', '2026-08-15', 'The haunted town faces a new supernatural threat far more dangerous.',                   'Amar Kaushik',      'Rajkummar Rao, Shraddha Kapoor, Pankaj Tripathi',       8.0),
('Kalki 2898 AD',     'Hindi',   'Sci-Fi, Action',      181, 'U/A', '2026-06-27', 'Set in a dystopian future, a warrior rises to protect the last hope of humanity.',       'Nag Ashwin',        'Prabhas, Amitabh Bachchan, Deepika Padukone',           7.6),
('Oppenheimer',       'English', 'Biography, Drama',    180, 'A',   '2026-07-21', 'The story of the father of the atomic bomb and the consequences of his creation.',       'Christopher Nolan', 'Cillian Murphy, Robert Downey Jr., Emily Blunt',        8.9),
('Dune: Part Two',    'English', 'Sci-Fi, Adventure',   166, 'U/A', '2026-03-01', 'Paul Atreides unites the Fremen to wage war against the Harkonnens.',                    'Denis Villeneuve',  'Timothée Chalamet, Zendaya, Austin Butler',             8.5),
('Pushpa 2',          'Telugu',  'Action, Drama',       175, 'U/A', '2026-12-06', 'Pushpa Raj continues his rise in the smuggling syndicate while facing new enemies.',     'Sukumar',           'Allu Arjun, Rashmika Mandanna, Fahadh Faasil',          8.1),
('Singham Again',     'Hindi',   'Action, Drama',       148, 'U/A', '2026-11-01', 'Bajirao Singham embarks on a mission inspired by the Ramayana to rescue his love.',     'Rohit Shetty',      'Ajay Devgn, Kareena Kapoor, Ranveer Singh',             7.0),
('Bhool Bhulaiyaa 3', 'Hindi',   'Horror, Comedy',      152, 'U/A', '2026-11-01', 'Rooh Baba returns to face an even more terrifying ghostly presence.',                    'Anees Bazmee',      'Kartik Aaryan, Vidya Balan, Triptii Dimri',             6.9);

-- ======================== THEATERS ========================
INSERT INTO theaters (theater_name, city_id, address, total_screens) VALUES
('AuraCinemas Nexus Mall',       1, 'Nexus Seawoods, Navi Mumbai',          4),
('AuraCinemas Phoenix',          1, 'Phoenix Palladium, Lower Parel',       5),
('AuraCinemas Hinjawadi',        2, 'Xion Mall, Hinjawadi Phase 1, Pune',   3),
('AuraCinemas Koregaon',         2, 'Amanora Mall, Koregaon Park, Pune',    4),
('AuraCinemas Orion',            3, 'Orion Mall, Rajajinagar, Bengaluru',   5),
('AuraCinemas Forum',            3, 'Forum Mall, Koramangala, Bengaluru',   3),
('AuraCinemas Select City',      4, 'Select Citywalk, Saket, Delhi',        4),
('AuraCinemas Ambience',         4, 'Ambience Mall, Vasant Kunj, Delhi',    5),
('AuraCinemas Inorbit',          5, 'Inorbit Mall, Hitec City, Hyderabad',  4),
('AuraCinemas Express Avenue',   6, 'Express Avenue, Royapettah, Chennai',  3);

-- ======================== SCREENS ========================
-- Theater 1 — AuraCinemas Nexus Mall, Mumbai (4 screens)
INSERT INTO screens (theater_id, screen_name, screen_type, total_rows, seats_per_row, total_capacity) VALUES
(1, 'Audi 1',       'Dolby Atmos', 12, 16, 192),
(1, 'Audi 2',       'Standard',    10, 14, 140),
(1, 'Audi 3',       'IMAX',        14, 20, 280),
(1, 'Audi 4',       'Standard',    10, 14, 140);

-- Theater 3 — AuraCinemas Hinjawadi, Pune (3 screens)
INSERT INTO screens (theater_id, screen_name, screen_type, total_rows, seats_per_row, total_capacity) VALUES
(3, 'Screen 1',     'Dolby Atmos', 12, 16, 192),
(3, 'Screen 2',     'Standard',    10, 14, 140),
(3, 'Screen 3',     'Standard',     8, 12,  96);

-- Theater 5 — AuraCinemas Orion, Bengaluru (5 screens)
INSERT INTO screens (theater_id, screen_name, screen_type, total_rows, seats_per_row, total_capacity) VALUES
(5, 'Audi 1',       'IMAX',        14, 20, 280),
(5, 'Audi 2',       'Dolby Atmos', 12, 16, 192),
(5, 'Audi 3',       'Standard',    10, 14, 140),
(5, 'Audi 4',       '4DX',         10, 12, 120),
(5, 'Audi 5',       'Standard',     8, 12,  96);

-- Theater 7 — AuraCinemas Select City, Delhi (4 screens)
INSERT INTO screens (theater_id, screen_name, screen_type, total_rows, seats_per_row, total_capacity) VALUES
(7, 'Gold 1',       'Dolby Atmos', 12, 16, 192),
(7, 'Gold 2',       'Standard',    10, 14, 140),
(7, 'Platinum 1',   'IMAX',        14, 20, 280),
(7, 'Platinum 2',   '4DX',         10, 12, 120);

-- Theater 9 — AuraCinemas Inorbit, Hyderabad (4 screens)
INSERT INTO screens (theater_id, screen_name, screen_type, total_rows, seats_per_row, total_capacity) VALUES
(9, 'Audi 1',       'IMAX',        14, 20, 280),
(9, 'Audi 2',       'Dolby Atmos', 12, 16, 192),
(9, 'Audi 3',       'Standard',    10, 14, 140),
(9, 'Audi 4',       'Standard',     8, 12,  96);

-- ======================== SEATS (for Screen 1 — Audi 1, Nexus Mall) ========================
-- Rows A-D: Standard (4×16=64), Rows E-H: Premium (4×16=64), Rows I-L: Recliner (4×16=64)
INSERT INTO seats (screen_id, category_id, row_label, seat_number)
SELECT 1, 1, chr, n FROM (
    SELECT 'A' AS chr UNION SELECT 'B' UNION SELECT 'C' UNION SELECT 'D'
) rows, (
    WITH RECURSIVE cnt(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM cnt WHERE n<16)
    SELECT n FROM cnt
) cols;

INSERT INTO seats (screen_id, category_id, row_label, seat_number)
SELECT 1, 2, chr, n FROM (
    SELECT 'E' AS chr UNION SELECT 'F' UNION SELECT 'G' UNION SELECT 'H'
) rows, (
    WITH RECURSIVE cnt(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM cnt WHERE n<16)
    SELECT n FROM cnt
) cols;

INSERT INTO seats (screen_id, category_id, row_label, seat_number)
SELECT 1, 3, chr, n FROM (
    SELECT 'I' AS chr UNION SELECT 'J' UNION SELECT 'K' UNION SELECT 'L'
) rows, (
    WITH RECURSIVE cnt(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM cnt WHERE n<16)
    SELECT n FROM cnt
) cols;

-- ======================== SEATS (for Screen 5 — Audi 1 IMAX, Hinjawadi Pune) ========================
INSERT INTO seats (screen_id, category_id, row_label, seat_number)
SELECT 5, 1, chr, n FROM (
    SELECT 'A' AS chr UNION SELECT 'B' UNION SELECT 'C' UNION SELECT 'D' UNION SELECT 'E' UNION SELECT 'F'
) rows, (
    WITH RECURSIVE cnt(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM cnt WHERE n<16)
    SELECT n FROM cnt
) cols;

INSERT INTO seats (screen_id, category_id, row_label, seat_number)
SELECT 5, 2, chr, n FROM (
    SELECT 'G' AS chr UNION SELECT 'H' UNION SELECT 'I' UNION SELECT 'J'
) rows, (
    WITH RECURSIVE cnt(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM cnt WHERE n<16)
    SELECT n FROM cnt
) cols;

INSERT INTO seats (screen_id, category_id, row_label, seat_number)
SELECT 5, 3, chr, n FROM (
    SELECT 'K' AS chr UNION SELECT 'L'
) rows, (
    WITH RECURSIVE cnt(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM cnt WHERE n<16)
    SELECT n FROM cnt
) cols;

-- ======================== SHOWS (Upcoming Showtimes) ========================
-- Pathaan — Screen 1, Nexus Mall Mumbai
INSERT INTO shows (movie_id, screen_id, show_date, start_time, end_time) VALUES
(1, 1, '2026-08-25', '09:00', '11:30'),
(1, 1, '2026-08-25', '13:00', '15:30'),
(1, 1, '2026-08-25', '17:00', '19:30'),
(1, 1, '2026-08-25', '20:00', '22:30');

-- Stree 2 — Screen 5, Hinjawadi Pune
INSERT INTO shows (movie_id, screen_id, show_date, start_time, end_time) VALUES
(6, 5, '2026-08-25', '10:00', '12:30'),
(6, 5, '2026-08-25', '14:00', '16:30'),
(6, 5, '2026-08-25', '18:30', '21:00'),
(6, 5, '2026-08-25', '21:30', '00:00');

-- Oppenheimer — Screen 1, Nexus Mall Mumbai (weekend)
INSERT INTO shows (movie_id, screen_id, show_date, start_time, end_time) VALUES
(8, 1, '2026-08-30', '10:00', '13:00'),
(8, 1, '2026-08-30', '15:00', '18:00'),
(8, 1, '2026-08-30', '19:00', '22:00');

-- Kalki 2898 AD — Screen 5, Hinjawadi Pune
INSERT INTO shows (movie_id, screen_id, show_date, start_time, end_time) VALUES
(7, 5, '2026-08-26', '11:00', '14:00'),
(7, 5, '2026-08-26', '16:00', '19:00'),
(7, 5, '2026-08-26', '20:00', '23:00');

-- Pushpa 2 — Screen 1, Nexus Mall
INSERT INTO shows (movie_id, screen_id, show_date, start_time, end_time) VALUES
(10, 1, '2026-08-27', '09:30', '12:30'),
(10, 1, '2026-08-27', '14:00', '17:00'),
(10, 1, '2026-08-27', '19:00', '22:00');

-- Dune: Part Two — Screen 5, Hinjawadi
INSERT INTO shows (movie_id, screen_id, show_date, start_time, end_time) VALUES
(9, 5, '2026-08-28', '10:00', '12:50'),
(9, 5, '2026-08-28', '15:00', '17:50'),
(9, 5, '2026-08-28', '20:30', '23:20');

-- ======================== SHOW_SEATS (auto-generate for Show 1 — all seats of screen 1) ========================
INSERT INTO show_seats (show_id, seat_id, status)
SELECT 1, seat_id, 'available' FROM seats WHERE screen_id = 1;

INSERT INTO show_seats (show_id, seat_id, status)
SELECT 2, seat_id, 'available' FROM seats WHERE screen_id = 1;

INSERT INTO show_seats (show_id, seat_id, status)
SELECT 3, seat_id, 'available' FROM seats WHERE screen_id = 1;

INSERT INTO show_seats (show_id, seat_id, status)
SELECT 4, seat_id, 'available' FROM seats WHERE screen_id = 1;

-- Show seats for Stree 2 shows (screen 5)
INSERT INTO show_seats (show_id, seat_id, status)
SELECT 5, seat_id, 'available' FROM seats WHERE screen_id = 5;

INSERT INTO show_seats (show_id, seat_id, status)
SELECT 6, seat_id, 'available' FROM seats WHERE screen_id = 5;

INSERT INTO show_seats (show_id, seat_id, status)
SELECT 7, seat_id, 'available' FROM seats WHERE screen_id = 5;

INSERT INTO show_seats (show_id, seat_id, status)
SELECT 8, seat_id, 'available' FROM seats WHERE screen_id = 5;

-- ======================== SIMULATE PARTIAL BOOKINGS (for dynamic pricing demo) ========================
-- Book ~55% of seats for Show 3 (Pathaan 5PM) to trigger occupancy surge
UPDATE show_seats SET status = 'booked' WHERE show_id = 3 AND seat_id IN (
    SELECT seat_id FROM seats WHERE screen_id = 1 AND row_label IN ('A','B','C','D','E','F','G')
);

-- Book ~85% of seats for Show 7 (Stree 2 6:30PM) to trigger high surge
UPDATE show_seats SET status = 'booked' WHERE show_id = 7 AND seat_id IN (
    SELECT seat_id FROM seats WHERE screen_id = 5 AND row_label IN ('A','B','C','D','E','F','G','H','I','J')
);

-- ======================== SAMPLE USERS ========================
INSERT INTO users (full_name, email, phone, password_hash, role, city) VALUES
('Arjun Sharma',    'arjun@example.com',    '+919876543210', '$2b$12$examplehash1', 'customer', 'Mumbai'),
('Priya Deshmukh',  'priya@example.com',    '+919876543211', '$2b$12$examplehash2', 'customer', 'Pune'),
('Rahul Verma',     'rahul@example.com',    '+919876543212', '$2b$12$examplehash3', 'customer', 'Delhi'),
('Sneha Iyer',      'sneha@example.com',    '+919876543213', '$2b$12$examplehash4', 'customer', 'Bengaluru'),
('Admin AuraCinema','admin@auracinemas.in',  '+919000000001', '$2b$12$adminhash001', 'admin',    'Mumbai');

-- ======================== SAMPLE BOOKINGS ========================
INSERT INTO bookings (user_id, show_id, num_seats, base_total, surge_total, tax_amount, convenience_fee, grand_total, booking_status) VALUES
(1, 1, 2, 400.00, 400.00,  72.00, 30.00,  502.00, 'confirmed'),
(2, 5, 3, 600.00, 660.00, 118.80, 30.00,  808.80, 'confirmed'),
(3, 3, 4, 800.00, 1000.00, 180.00, 30.00, 1210.00, 'confirmed'),
(4, 7, 2, 700.00, 980.00,  176.40, 30.00, 1186.40, 'confirmed');

-- ======================== SAMPLE PAYMENTS ========================
INSERT INTO payments (booking_id, payment_method, payment_gateway, transaction_ref, amount_paid, payment_status) VALUES
(1, 'UPI',         'PhonePe',   'TXN2026082501A', 502.00,  'success'),
(2, 'RuPay',       'Razorpay',  'TXN2026082502B', 808.80,  'success'),
(3, 'Credit Card', 'Razorpay',  'TXN2026082503C', 1210.00, 'success'),
(4, 'UPI',         'GPay',      'TXN2026082504D', 1186.40, 'success');

-- ======================== PRICING RULES ========================
INSERT INTO pricing_rules (rule_name, rule_type, condition_desc, modifier_percent, priority) VALUES
('Weekend Surge',        'temporal',  'Friday 6PM to Sunday 11:59PM',        20.0,  10),
('Prime Time Surge',     'temporal',  'Showtime between 7:00PM – 10:30PM',   10.0,  20),
('Matinee Discount',     'temporal',  'Showtime before 12:00PM',            -10.0,  30),
('Low Occupancy',        'occupancy', 'Occupancy < 40%',                      0.0,  40),
('Medium Occupancy',     'occupancy', 'Occupancy 40% – 70%',                 10.0,  50),
('High Occupancy',       'occupancy', 'Occupancy 70% – 90%',                 25.0,  60),
('Surge Demand',         'occupancy', 'Occupancy > 90%',                     40.0,  70),
('National Holiday',     'special',   'Republic Day / Independence Day',     15.0,  80),
('Premiere Night',       'special',   'First day first show of new release', 30.0,  90);

-- ======================== SURGE ANALYTICS SNAPSHOTS ========================
INSERT INTO surge_analytics (show_id, total_capacity, booked_count, locked_count, occupancy_percent, surge_multiplier, avg_dynamic_price, requests_per_min) VALUES
(1, 192,  15,  3,   7.8, 1.00, 200.00,  12),
(2, 192,  42,  5,  21.9, 1.00, 200.00,  28),
(3, 192, 112,  8,  58.3, 1.10, 220.00,  85),
(4, 192,  30,  2,  15.6, 1.00, 200.00,  18),
(5, 192,  20,  4,  10.4, 1.00, 200.00,  22),
(7, 192, 160, 10,  83.3, 1.25, 250.00, 142),
(8, 192,  45,  6,  23.4, 1.00, 200.00,  35);

-- ======================== REVENUE ANALYTICS ========================
INSERT INTO revenue_analytics (report_date, city_id, theater_id, movie_id, total_bookings, total_revenue, avg_occupancy, avg_surge, peak_surge) VALUES
('2026-08-25', 1, 1, 1,  45,  28500.00, 62.5, 1.12, 1.25),
('2026-08-25', 2, 3, 6,  62,  41200.00, 78.3, 1.20, 1.40),
('2026-08-25', 1, 1, 8,  38,  32800.00, 55.0, 1.08, 1.15),
('2026-08-26', 2, 3, 7,  51,  38900.00, 71.2, 1.18, 1.35),
('2026-08-27', 1, 1, 10, 55,  35600.00, 68.0, 1.15, 1.30);

-- ============================================================================
-- END OF SEED DATA
-- ============================================================================
