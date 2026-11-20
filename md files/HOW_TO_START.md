# 🎬 How to Start AuraCinemas Platform

A step-by-step guide to run and test the **AuraCinemas** movie ticket booking platform.

---

## 🚀 Quick Start (Recommended)

The platform includes a zero-dependency full-stack server (`server.py`) that serves both the **frontend UI** and all **REST APIs** using Python's standard library and SQLite. No `pip install` or external packages are required!

### 1. Open Terminal
Open **PowerShell** or **Command Prompt** and navigate to the project directory:

```powershell
cd "c:\Users\prath\OneDrive\Desktop\College SY-Sem1\Aura Cinemas\auracinemas"
```

### 2. Start the Server
Run the following command:

```powershell
python server.py
```

You will see:
```text
============================================================
 AuraCinemas Full-Stack Platform running on http://localhost:8080
 Frontend static files served from: ...\frontend\public
 Live API listening on http://localhost:8080/api/...
============================================================
```

### 3. Open in Browser
Visit any of the following URLs in your web browser:

| Page / Endpoint | URL | Description |
| :--- | :--- | :--- |
| **Home / Movies** | [http://localhost:8080](http://localhost:8080) | Browse movies, filter by language/city, view details |
| **Seat Booking** | [http://localhost:8080/booking.html](http://localhost:8080/booking.html) | Interactive seat selection & checkout (switch movie via dropdown or select any movie from Home) |
| **API Health** | [http://localhost:8080/health](http://localhost:8080/health) | Verifies server and API status |

> **To stop the server:** Press `Ctrl + C` in the terminal window.

---

## 📁 Project Structure

```text
auracinemas/
├── HOW_TO_START.md          <-- You are here (Quick start guide)
├── server.py                <-- Python full-stack server (Zero-dependency)
├── auracinemas.db           <-- SQLite database (pre-populated)
├── frontend/
│   └── public/
│       ├── index.html       <-- Homepage / Movie catalog
│       ├── booking.html     <-- Seat layout & checkout UI
│       ├── css/style.css    <-- Styling & design system
│       └── js/              <-- Frontend logic (seat selection, API calls)
├── database/
│   ├── schema.sql           <-- Database schema definitions
│   └── dataset.sql          <-- Initial mock data (movies, cities, shows)
└── backend/                 <-- Optional C++ Crow/CMake backend
    ├── CMakeLists.txt
    └── src/
```

---

## 🔌 Available API Endpoints

When `server.py` is running, the following endpoints are live:

- `GET /health` — Check server status
- `GET /api/movies` — List all active movies (optional: `?language=Hindi`)
- `GET /api/cities` — List supported cities
- `GET /api/cities/:id/theaters` — List theaters in a specific city
- `GET /api/movies/:id/shows` — List showtimes for a movie
- `GET /api/shows/:id/seats` — Get real-time seat layout & booking status
- `GET /api/shows/:id/surge` — Get dynamic surge pricing for a show
- `POST /api/shows/:id/lock` — Temporarily hold/lock selected seats
- `POST /api/bookings` — Create and confirm a ticket booking
- `DELETE /api/bookings/:id` — Cancel an existing booking
- `POST /api/pricing/calculate` — Calculate dynamic breakdown with taxes & discounts
- `GET /api/pricing/rules` — View active pricing rules

---

## 🛠️ Optional: Building the C++ Backend

If your project evaluation specifically requires the C++ backend:

1. Ensure **CMake** (>= 3.20) and a C++20 compiler (**MSVC** or **GCC**) are installed.
2. Build with CMake:
   ```powershell
   cd backend
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
   ```
3. Run the compiled binary:
   ```powershell
   ./Release/auracinemas_server.exe
   ```

---

## ❓ Troubleshooting

* **Port 8080 is already in use:**
  If port `8080` is busy, open `server.py`, change `PORT = 8080` (line 290) to another port like `PORT = 8000` or `PORT = 5000`, and restart.
* **`python` command not recognized:**
  Try running `py server.py` or `python3 server.py`. Ensure Python is added to your system's PATH.
* **Database reset:**
  To reset the database to its fresh original state, delete `auracinemas.db` and re-run `python server.py`. It will automatically re-create the database using `database/schema.sql` and `database/dataset.sql`.
