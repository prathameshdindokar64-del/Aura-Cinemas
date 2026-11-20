# 👥 AuraCinemas — 4-Person OOP Project Work Breakdown & Team Responsibility Matrix

> **A Complete Object-Oriented Task Distribution, Module Ownership & Viva Guide for 4 Team Members**  
> **Target Evaluation:** College SY-Sem1 Object-Oriented Programming (C++) / Software Engineering Project  
> **Design Philosophy:** Strict adherence to **SOLID Principles**, Low Coupling, High Cohesion, and Interface-Driven Collaboration.

---

## 📑 Table of Contents
1. [Project Architectural Division Overview](#1-project-architectural-division-overview)
2. [Interface-First Development (Preventing Merge Conflicts)](#2-interface-first-development-preventing-merge-conflicts)
3. [Individual Team Member Responsibilities](#3-individual-team-member-responsibilities)
   - [Member 1: Database Persistence, RAII & Domain Entity Modeling](#member-1-database-persistence-raii--domain-entity-modeling)
   - [Member 2: Dynamic Pricing Engine & Strategy Pattern Architecture](#member-2-dynamic-pricing-engine--strategy-pattern-architecture)
   - [Member 3: High-Concurrency Booking Service, State Machine & Lock Engine](#member-3-high-concurrency-booking-service-state-machine--lock-engine)
   - [Member 4: REST API Controllers, Authentication & Full-Stack Integration](#member-4-rest-api-controllers-authentication--full-stack-integration)
4. [Component Collaboration & Dependency Matrix](#4-component-collaboration--dependency-matrix)
5. [4-Week Implementation Roadmap & Milestones](#5-4-week-implementation-roadmap--milestones)
6. [Individual Viva Defense & Evaluation Guide (Per-Member Q&A)](#6-individual-viva-defense--evaluation-guide-per-member-qa)

---

## 1. Project Architectural Division Overview

To deliver a scalable C++ project without team members stepping on each other's toes, the system is partitioned into **four decoupled OOP modules**. Each module is owned by one person who acts as the lead architect and developer for that domain.

```mermaid
flowchart TD
    subgraph Member4 ["Member 4: Presentation & Integration Layer"]
        Ctrl["API Controllers (Crow HTTP)\n• BookingController\n• PricingController\n• AuthController / RBAC\n• JSON DTO Serializer"]
    end

    subgraph Member3 ["Member 3: Transaction & Concurrency Layer"]
        LockSvc["BookingService & Lock Engine\n• 2-Phase Seat Lock (8m TTL)\n• ACID Transactions (BEGIN IMMEDIATE)\n• Readers-Writer Lock (shared_mutex)\n• Background Cleaner Thread"]
    end

    subgraph Member2 ["Member 2: Business Logic & Algorithm Layer"]
        PriceEng["PricingEngine & Strategy Architecture\n• IPricingStrategy (Polymorphism)\n• Occupancy / Temporal / Holiday Rules\n• Demand Velocity Surge Bonus\n• Clamped Financial Bounds (0.7x–3.0x)"]
    end

    subgraph Member1 ["Member 1: Persistence & Data Modeling Layer"]
        DataLayer["DbManager & Domain Entities\n• RAII SQLite3 Connection Handle\n• Rule of 5 (Deleted Copying)\n• Domain Models (Movie, Show, Seat, User)\n• Parameterized Query Dispatcher"]
    end

    Ctrl -->|Invokes Services| LockSvc
    Ctrl -->|Invokes Pricing APIs| PriceEng
    LockSvc -->|Calls calculate_price()| PriceEng
    LockSvc -->|Executes SQL & Checks Locks| DataLayer
    PriceEng -->|Reads Rules| DataLayer
```

---

## 2. Interface-First Development (Preventing Merge Conflicts)

In professional software development, teams avoid merge conflicts by agreeing on **Interface Contracts** before writing implementation code. 

- **Contract Freezing (Day 1–3):**
  - Member 1 defines the `DbManager` public API and core database structs.
  - Member 2 defines `ShowContext`, `PricingResult`, and `IPricingStrategy`.
  - Member 3 defines `BookingRequest`, `BookingReceipt`, and `BookingOutcome`.
  - Member 4 defines HTTP routes and input/output JSON schemas.
- Once headers and DTO structs are committed, **all 4 members can write code simultaneously** without blocking each other.

---

## 3. Individual Team Member Responsibilities

---

### Member 1: Database Persistence, RAII & Domain Entity Modeling
> **Role:** *The Data Architecture & Infrastructure Specialist*  
> **Primary Responsibility:** Ensure data integrity, connection lifecycle safety, and clean domain models.

#### 1. Core OOP Focus & Concepts Applied:
- **Encapsulation & RAII (Resource Acquisition Is Initialization):** Encapsulates the native `sqlite3*` C pointer. File descriptors and database connections are opened upon object construction and cleaned up automatically upon destruction without leaks.
- **Rule of Three / Five:** Explicitly deletes copy constructor and copy assignment operators (`DbManager(const DbManager&) = delete;`) to eliminate double-free errors.
- **Domain Modeling & Abstraction:** Creates concrete entity classes/structs representing real-world multiplex objects (`Movie`, `Screen`, `Seat`, `Show`, `Theater`, `User`) rather than passing raw tabular data.
- **Functional Abstraction:** Implements `using RowCallback = std::function<void(sqlite3_stmt*)>` to decouple SQL cursor iteration from business data consumption.

#### 2. Key Code Deliverables:
- `database/db_manager.hpp`: Core class managing SQLite connection in WAL mode (`PRAGMA journal_mode = WAL`).
- `models/domain_entities.hpp`: Strongly typed C++ structs for `Movie`, `Theater`, `ShowSeat`, and `User`.
- `database/schema.sql` & `database/dataset.sql`: Schema definitions and mock seed data for Indian cities and multiplexes.
- Unit tests verifying database creation, table existence checks, and parameterized query execution.

#### 3. Class Design Blueprint:
```cpp
namespace aura {
class DbManager {
public:
    explicit DbManager(const std::string& db_path);
    ~DbManager();

    // Disable copying
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    bool open();
    void close();
    bool exec(const std::string& sql);
    bool execute_file(const std::string& filepath);

    using RowCallback = std::function<void(sqlite3_stmt*)>;
    bool query(const std::string& sql, RowCallback cb);

    sqlite3* handle();
    bool table_exists(const std::string& table_name);

private:
    std::string db_path_;
    sqlite3*    db_ = nullptr;
};
}
```

---

### Member 2: Dynamic Pricing Engine & Strategy Pattern Architecture
> **Role:** *The Algorithm & Yield Optimization Specialist*  
> **Primary Responsibility:** Design the dynamic pricing engine, rule dispatching hierarchy, and financial math models.

#### 1. Core OOP Focus & Concepts Applied:
- **The Strategy Pattern (Runtime Polymorphism):** Defines an abstract base class `IPricingStrategy` with pure virtual functions (`is_applicable()`, `calculate_modifier()`), subclassed by concrete strategies:
  - `OccupancyPricingStrategy`: Adjusts price based on auditorium fill percentage (<40%, 40–70%, 70–90%, >90%).
  - `TemporalPricingStrategy`: Detects weekend screenings, late evening prime-time shows, and morning matinee discounts.
  - `HolidayPricingStrategy`: Injects holiday premiums for national and festive dates.
- **Open-Closed Principle (OCP):** New pricing rules (e.g., IPL match day discount, VIP member perk) can be added simply by creating a new derived class without touching existing pricing code.
- **Thread Safety & Data Hiding:** Encapsulates the rules list inside a mutex-protected container (`mutable std::mutex mu_` with `std::lock_guard`).
- **Mathematical Clamping & Value Invariants:** Uses `std::clamp` to guarantee prices never surge past 3.0× or dip below 0.70×.

#### 2. Key Code Deliverables:
- `services/pricing_engine.hpp`: The core `PricingEngine` class and DTOs (`PricingRule`, `ShowContext`, `PricingResult`, `BulkPricingResult`).
- `services/strategies/`: Strategy interface and derived strategy classes.
- Surge calculation formulas including demand velocity bonus (`velocity_bonus(rpm)`), GST tax breakdown (18%), and platform convenience fee.
- Unit tests validating pricing calculations across various occupancy and time conditions.

#### 3. Class Design Blueprint:
```cpp
namespace aura {

class IPricingStrategy {
public:
    virtual ~IPricingStrategy() = default;
    virtual bool is_applicable(const ShowContext& ctx) const = 0;
    virtual double calculate_modifier(const ShowContext& ctx) const = 0;
    virtual std::string get_name() const = 0;
};

class PricingEngine {
public:
    static constexpr double GST_RATE = 0.18;
    static constexpr double CONVENIENCE_FEE = 30.0;
    static constexpr double MIN_SURGE_MULTIPLIER = 0.70;
    static constexpr double MAX_SURGE_MULTIPLIER = 3.00;

    explicit PricingEngine(std::vector<PricingRule> rules);
    void load_rules(std::vector<PricingRule> rules);

    [[nodiscard]] PricingResult calculate_price(double base_price, const ShowContext& ctx) const;
    [[nodiscard]] BulkPricingResult calculate_bulk(const std::vector<double>& base_prices, const ShowContext& ctx) const;

private:
    mutable std::mutex mu_;
    std::vector<PricingRule> rules_;
    void sort_rules();
};
}
```

---

### Member 3: High-Concurrency Booking Service, State Machine & Lock Engine
> **Role:** *The Transaction, Concurrency & State Specialist*  
> **Primary Responsibility:** Eliminate race conditions during seat selection, manage TTL holds, and orchestrate ACID booking transactions.

#### 1. Core OOP Focus & Concepts Applied:
- **State Pattern & Invariant Preservation:** Models the seat lifecycle through explicit state transitions: `Available` $\rightarrow$ `Locked` (8-minute hold) $\rightarrow$ `Booked` (Payment confirmed) or back to `Available` (Lock expired or cancelled).
- **Concurrency & Readers-Writer Lock:** Uses `std::shared_mutex`:
  - `std::shared_lock`: Allows infinite concurrent users to view the seat map simultaneously (`get_seat_map()`).
  - `std::unique_lock`: Grants exclusive access during state updates (`lock_seats()`, `confirm_booking()`).
- **ACID Transaction Management:** Uses `BEGIN IMMEDIATE TRANSACTION` in SQLite to prevent writer starvation and eliminate double-booking bugs.
- **Active Object / Background Daemon Thread:** Spawns a background worker thread in `main.cpp` that periodically calls `release_expired_locks()` every 60 seconds without freezing the API.
- **Dependency Injection:** Injects dependencies (`sqlite3* db`, `PricingEngine& engine`) via constructor.

#### 2. Key Code Deliverables:
- `services/booking_service.hpp`: Main service class implementing `lock_seats()`, `confirm_booking()`, `cancel_booking()`, and `get_seat_map()`.
- Thread-safe in-memory cache `std::unordered_map<std::string, SeatLock> locks_` protected by `locks_mu_`.
- Automatic transaction rollback routines on error (`ROLLBACK`).
- Concurrency simulation script to prove zero double-booking under race conditions.

#### 3. Class Design Blueprint:
```cpp
namespace aura {
class BookingService {
public:
    static constexpr int LOCK_DURATION_SECONDS = 480; // 8 minutes
    static constexpr int MAX_SEATS_PER_BOOKING = 10;

    explicit BookingService(sqlite3* db, PricingEngine& pricing);

    BookingOutcome lock_seats(int user_id, int show_id, const std::vector<int>& seat_ids);
    BookingOutcome confirm_booking(const BookingRequest& req);
    BookingOutcome cancel_booking(int booking_id, int user_id);
    void release_expired_locks();
    std::vector<SeatView> get_seat_map(int show_id);

private:
    sqlite3* db_;
    PricingEngine& pricing_;
    mutable std::shared_mutex rw_mu_;
    std::mutex locks_mu_;
    std::unordered_map<std::string, SeatLock> locks_;
};
}
```

---

### Member 4: REST API Controllers, Authentication & Full-Stack Integration
> **Role:** *The Interface, Security & Integration Specialist*  
> **Primary Responsibility:** Expose web-accessible Crow HTTP endpoints, handle JSON parsing/serialization, and bridge the C++ engine to the frontend.

#### 1. Core OOP Focus & Concepts Applied:
- **Model-View-Controller (MVC) Architectural Pattern:** Decouples user-facing web requests from business logic. Controllers only handle HTTP parsing, status codes, and JSON response framing.
- **Facade & Adapter Patterns:** Simplifies complex internal service methods into clean HTTP endpoints for client consumption.
- **Role-Based Access Control (RBAC):** Models user credentials and privileges (`User` class with derived roles or polymorphic permissions for `Customer` vs `Admin`).
- **Data Transfer Object (DTO) Serialization:** Leverages `nlohmann::json` to map JSON request bodies into strongly typed C++ DTOs (`BookingRequest`, `SeatSelection`) and serialize output receipts.

#### 2. Key Code Deliverables:
- `controllers/booking_controller.hpp`: Endpoints for seat layout (`/api/shows/:id/seats`), seat locking (`/api/shows/:id/lock`), booking confirmation (`/api/bookings`), and cancellation.
- `controllers/pricing_controller.hpp`: Endpoints for dynamic pricing breakdown (`/api/pricing/calculate`) and active rule queries (`/api/pricing/rules`).
- `main.cpp` Server Setup: Initializes Crow HTTP server, configures CORS middleware, registers route groups, and starts listener on port 8080.
- Frontend Wiring: Connects the interactive seat map (`booking.html` / `js/booking.js`) and movie catalog (`index.html`) to the C++ Crow API.

#### 3. Class / Module Blueprint:
```cpp
namespace aura {
// Functional Controller Route Registration
void register_booking_routes(crow::SimpleApp& app, BookingService& booking);
void register_pricing_routes(crow::SimpleApp& app, PricingEngine& engine, DbManager& db);
void register_data_routes(crow::SimpleApp& app, DbManager& db);
}
```

---

## 4. Component Collaboration & Dependency Matrix

The following table summarizes who depends on whom, and what contracts link them:

| Team Member | What They Build | What They Depend On | Contract / Shared Header |
| :--- | :--- | :--- | :--- |
| **Member 1** (Data) | `DbManager`, Database Schema, Entity Structs | SQLite3 C Library | `db_manager.hpp`, `schema.sql` |
| **Member 2** (Pricing) | `PricingEngine`, Strategy Classes, Price Formulas | Member 1's `PricingRule` SQL table | `pricing_engine.hpp`, `ShowContext` |
| **Member 3** (Booking) | `BookingService`, Seat Locks, Transactions | Member 1's `sqlite3*`, Member 2's `PricingEngine&` | `booking_service.hpp`, `BookingOutcome` |
| **Member 4** (API/Web) | Crow Controllers, JSON Serialization, UI Integration | Member 1 (`DbManager`), Member 2 (`PricingEngine`), Member 3 (`BookingService`) | `booking_controller.hpp`, `pricing_controller.hpp`, `main.cpp` |

---

## 5. 4-Week Implementation Roadmap & Milestones

```mermaid
gantt
    title 4-Week Team Implementation Roadmap
    dateFormat  YYYY-MM-DD
    section Week 1: Interface & Architecture
    Freeze Header Contracts & DTOs         :done,    des1, 2026-09-01, 2d
    Member 1: Setup SQLite & DbManager    :active,  m1_1, 2026-09-03, 5d
    Member 2: PricingEngine Skeleton       :active,  m2_1, 2026-09-03, 5d
    Member 3: In-Memory Lock Mockup        :active,  m3_1, 2026-09-03, 5d
    Member 4: Crow Server & CORS Setup     :active,  m4_1, 2026-09-03, 5d

    section Week 2: Core Module Coding
    Member 1: Schema Seed & Queries        :         m1_2, 2026-09-08, 6d
    Member 2: Strategy Pattern Rules       :         m2_2, 2026-09-08, 6d
    Member 3: ACID Transactions & Hold TTL :         m3_2, 2026-09-08, 6d
    Member 4: REST Route Handlers          :         m4_2, 2026-09-08, 6d

    section Week 3: Integration
    Wire Member 3 with Member 1 & 2        :         int1, 2026-09-14, 4d
    Wire Member 4 Controllers with Services:         int2, 2026-09-16, 4d
    Frontend UI Integration                :         int3, 2026-09-18, 3d

    section Week 4: Testing & Viva Prep
    Concurrency & Stress Testing           :         tst1, 2026-09-21, 3d
    Bug Fixes & Code Cleanup               :         tst2, 2026-09-24, 2d
    Viva Q&A Practice & Presentation Slides:         viva, 2026-09-26, 3d
```

---

## 6. Individual Viva Defense & Evaluation Guide (Per-Member Q&A)

When the project examiner or professor asks: *"What was your specific individual contribution, and where are your OOP concepts?"*, each member should present these specific points:

### Member 1 (Data & RAII Specialist)
- **Contribution Statement:** *"I designed the persistence layer, engineered the `DbManager` class using modern C++ RAII to manage database connections safely, and established the schema with WAL mode for concurrent multi-threading."*
- **Key Code to Show the Examiner:**
  - `DbManager::~DbManager()` — Show that closing `sqlite3_close()` is guaranteed upon object destruction.
  - `DbManager(const DbManager&) = delete;` — Explain why copying database connections is prohibited under the Rule of Five.
  - Parameterized `query()` method using `std::function` callback.
- **Examiner's Favorite Question:** *"Why not just use global SQLite variables instead of a class?"*
  - **Answer:** *"A global variable violates encapsulation, creates hidden coupling, and causes race conditions when multiple threads initialize or terminate the connection. Encapsulating the connection inside `DbManager` guarantees controlled initialization, centralized error logging, and deterministic cleanup through RAII."*

---

### Member 2 (Pricing Engine & Strategy Specialist)
- **Contribution Statement:** *"I authored the dynamic pricing engine and structured the surge rules using the Strategy Pattern to demonstrate runtime polymorphism, ensuring mathematical bounds and thread-safe rule evaluation."*
- **Key Code to Show the Examiner:**
  - The `IPricingStrategy` abstract base class with pure virtual methods.
  - `PricingEngine::calculate_price()` with `std::clamp` preventing surge pricing exploitation.
  - Mutex synchronization (`std::lock_guard<std::mutex> lock(mu_)`) protecting the rules catalog.
- **Examiner's Favorite Question:** *"How did you apply the Open-Closed Principle (OCP) in your module?"*
  - **Answer:** *"The pricing engine is open for extension but closed for modification. If management wants to add a new pricing factor—such as loyalty tier discounts or weather-based surge—we do not modify the core calculation loop. We simply create a new subclass inheriting from `IPricingStrategy` and push it to the rules list."*

---

### Member 3 (Concurrency & Booking Service Specialist)
- **Contribution Statement:** *"I implemented the high-concurrency `BookingService`, managing the 2-phase seat reservation protocol (temporary 8-minute TTL hold $\rightarrow$ confirmation) using reader-writer locks and atomic SQLite transactions to prevent double-booking."*
- **Key Code to Show the Examiner:**
  - `std::shared_mutex rw_mu_` showing read-write separation (`std::shared_lock` for viewing seats vs `std::unique_lock` for booking).
  - SQLite `BEGIN IMMEDIATE TRANSACTION` in `lock_seats()` and `confirm_booking()`.
  - The background thread in `main.cpp` executing `release_expired_locks()` every 60 seconds.
- **Examiner's Favorite Question:** *"What happens if two users click the same VIP seat at the exact same millisecond?"*
  - **Answer:** *"Both request threads enter `lock_seats()`. One acquires the exclusive `unique_lock` and initiates `BEGIN IMMEDIATE`. The database checks `status == 'available'`. The first user locks the seat and commits. When the second transaction runs, it sees `status == 'locked'` and immediately rolls back, returning an HTTP 409 Conflict. Zero race conditions occur."*

---

### Member 4 (API, Security & Integration Specialist)
- **Contribution Statement:** *"I built the Presentation and Controller tier using Crow HTTP, implementing MVC architecture to decouple HTTP handling from business logic, serializing data via DTOs, and integrating the live UI."*
- **Key Code to Show the Examiner:**
  - `register_booking_routes()` and `register_pricing_routes()` demonstrating clean separation of concerns.
  - JSON input parsing, error status code mapping (400, 404, 409, 410, 500), and response generation using `nlohmann::json`.
  - Integration with the frontend seat map SVG renderer and dynamic checkout summary.
- **Examiner's Favorite Question:** *"Why shouldn't your controllers execute SQL queries directly?"*
  - **Answer:** *"Direct SQL execution in controllers violates the Single Responsibility Principle and creates tight coupling between the presentation layer and the database schema. If the database schema changes, all controllers break. By delegating to `BookingService` and `PricingEngine`, the controllers only care about HTTP request/response formatting, while business rules remain reusable and testable."*
