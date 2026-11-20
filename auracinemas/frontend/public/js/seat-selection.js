// ============================================================================
// AuraCinemas - Seat Selection & Traffic-Based Adaptive Dynamic Pricing Engine
// ============================================================================
(() => {
    'use strict';

    const API = (window.location.protocol === 'file:' || (window.location.port && window.location.port !== '8080'))
        ? 'http://localhost:8080'
        : '';
    const MAX_SEATS = 10;
    const GST_RATE  = 0.18;
    const CONV_FEE  = 30;

    // ---- State ----
    let selectedSeats = [];
    let seatDataMap   = {};   // seat_id -> seat object
    let surgeMultiplier = 1.0;
    let adaptiveData = null;
    let currentShowId = 0;
    let movieId = 0;
    let currentMovie = null;
    let allMovieShows = [];
    let allActiveCatalogMovies = [];
    let lockTimerInterval = null;

    // ---- Parse URL params ----
    const params  = new URLSearchParams(window.location.search);
    currentShowId = parseInt(params.get('show_id')) || 0;
    movieId       = parseInt(params.get('movie_id')) || 0;

    // ============================================================================
    // INIT
    // ============================================================================
    async function init() {
        checkUserRole();
        window.addEventListener('aura-user-changed', () => checkUserRole());

        await loadCatalogMovies();
        await loadShowInfo();
        await loadAdaptiveData();
        await loadSeatMap();
        renderMovieQuickSelect();
    }

    async function loadCatalogMovies() {
        try {
            const res = await fetch(`${API}/api/movies`);
            if (res.ok) {
                const data = await res.json();
                allActiveCatalogMovies = data.movies || [];
                renderMovieQuickSelect();
            }
        } catch (e) {}
    }

    function renderMovieQuickSelect() {
        const sel = document.getElementById('movieQuickSelect');
        if (!sel || allActiveCatalogMovies.length === 0) return;

        sel.innerHTML = allActiveCatalogMovies.map(m => `
            <option value="${m.movie_id}" ${m.movie_id === movieId ? 'selected' : ''}>
                ${m.title} (${m.language})
            </option>
        `).join('');
    }

    async function switchMovie(newMovieId) {
        newMovieId = parseInt(newMovieId);
        if (!newMovieId || newMovieId === movieId) return;
        movieId = newMovieId;
        currentShowId = 0;
        selectedSeats = [];
        const newUrl = `booking.html?movie_id=${movieId}`;
        window.history.replaceState(null, '', newUrl);

        await loadShowInfo();
        await loadAdaptiveData();
        await loadSeatMap();
        updatePricingPanel();
        renderMovieQuickSelect();
    }

    // ============================================================================
    // ROLE RESTRICTION & USER ENFORCEMENT
    // Strictly separated: Customers vs Administrators vs Guests
    // ============================================================================
    function checkUserRole() {
        const user = window.AuraAuth ? AuraAuth.getCurrentUser() : null;
        const banner = document.getElementById('roleAlertBanner');
        const btnConfirm = document.getElementById('btnConfirm');
        const emailNotice = document.getElementById('registeredEmailNotice');

        if (!user) {
            // GUEST MODE: Browsing allowed, must sign in to confirm
            if (banner) {
                banner.className = 'role-alert-banner role-alert-banner--guest';
                banner.innerHTML = `
                    <div>
                        <strong style="color:var(--accent);">👋 Browsing as Guest:</strong>
                        <span>Select your seats freely. You will need to sign in with a Customer Account to finalize booking and receive your email receipt.</span>
                    </div>
                    <div style="display:flex; gap:0.5rem;">
                        <button class="btn btn--outline btn--sm" onclick="AuraAuth.openLoginModal('customer')" style="font-size:0.75rem;">Sign In</button>
                        <button class="btn btn--primary btn--sm" onclick="AuraAuth.openRegisterModal()" style="font-size:0.75rem;">Register</button>
                    </div>
                `;
            }
            if (emailNotice) {
                emailNotice.textContent = 'Sign in to register email';
            }
            if (btnConfirm) {
                btnConfirm.disabled = selectedSeats.length === 0;
                btnConfirm.textContent = selectedSeats.length === 0 ? 'Select Seats to Continue' : `Sign In to Book (${selectedSeats.length} Seats — ₹${calculateGrandTotal()})`;
                btnConfirm.style.background = '';
                btnConfirm.style.color = '';
                btnConfirm.style.cursor = selectedSeats.length === 0 ? 'not-allowed' : 'pointer';
            }
            return;
        }

        if (user.role === 'admin') {
            // ADMIN MODE: Strict block on booking tickets
            if (emailNotice) {
                emailNotice.textContent = user.email;
            }
            if (banner) {
                banner.className = 'role-alert-banner role-alert-banner--admin';
                banner.innerHTML = `
                    <div>
                        <strong style="color:#ef4444;">👑 Administrator Mode Detected:</strong>
                        <span>Admins manage cinema houses, movies, and adaptive pricing. <strong>Admins cannot book tickets.</strong></span>
                    </div>
                    <div style="display:flex; gap:0.5rem;">
                        <a href="admin.html" class="btn btn--primary btn--sm" style="font-size:0.75rem;">Open Admin Console</a>
                        <button class="btn btn--outline btn--sm" onclick="AuraAuth.logout()" style="font-size:0.75rem; border-color:#555; color:var(--text-muted);">Sign Out</button>
                    </div>
                `;
            }
            if (btnConfirm) {
                btnConfirm.disabled = true;
                btnConfirm.textContent = '🔒 Admin Account — Cannot Book Tickets';
                btnConfirm.style.background = '#2a2a2a';
                btnConfirm.style.color = '#777';
                btnConfirm.style.cursor = 'not-allowed';
            }
        } else {
            // CUSTOMER MODE: Can book tickets and receives automated email
            if (emailNotice) {
                emailNotice.textContent = user.email;
            }
            if (banner) {
                banner.className = 'role-alert-banner role-alert-banner--customer';
                banner.innerHTML = `
                    <div>
                        <strong style="color:var(--color-green);">👤 Signed in as Customer:</strong>
                        <span>${user.full_name} (<em>${user.email}</em>) — Your tickets and automated email receipt will be sent here upon confirmation.</span>
                    </div>
                    <button class="btn btn--outline btn--sm" onclick="AuraAuth.logout()" style="font-size:0.75rem; border-color:#444; color:var(--text-muted);">Sign Out</button>
                `;
            }
            if (btnConfirm) {
                btnConfirm.disabled = selectedSeats.length === 0;
                btnConfirm.textContent = selectedSeats.length === 0 ? 'Select Seats to Continue' : `Proceed to Pay (₹${calculateGrandTotal()})`;
                btnConfirm.style.background = '';
                btnConfirm.style.color = '';
                btnConfirm.style.cursor = selectedSeats.length === 0 ? 'not-allowed' : 'pointer';
            }
        }
    }

    // ============================================================================
    // LOAD SHOW & ADAPTIVE DATA
    // ============================================================================
    async function loadShowInfo() {
        let movieData = null;

        // 0. If no movieId provided, default to first active catalog movie
        if (movieId <= 0 && allActiveCatalogMovies.length > 0) {
            movieId = allActiveCatalogMovies[0].movie_id;
        }

        // 1. If movieId is provided, fetch movie info first
        if (movieId > 0) {
            try {
                const res = await fetch(`${API}/api/movies/${movieId}`);
                if (res.ok) {
                    const data = await res.json();
                    if (data.movie) {
                        movieData = data.movie;
                    }
                }
            } catch (e) {}
        }

        // 2. Fetch shows for this movie
        if (movieId > 0) {
            try {
                const res = await fetch(`${API}/api/movies/${movieId}/shows`);
                if (res.ok) {
                    const data = await res.json();
                    allMovieShows = data.shows || [];
                }
            } catch (e) {}
        }

        // 3. Resolve active show
        let activeShow = null;
        if (currentShowId > 0) {
            activeShow = allMovieShows.find(s => s.show_id === currentShowId);
            if (!activeShow) {
                try {
                    const res = await fetch(`${API}/api/shows/${currentShowId}`);
                    if (res.ok) {
                        const data = await res.json();
                        if (data.show) {
                            activeShow = data.show;
                            movieId = activeShow.movie_id || movieId;
                            if (!movieData) movieData = activeShow;
                        }
                    }
                } catch (e) {}
            }
        }

        // If no activeShow chosen yet, pick the first show of the movie
        if (!activeShow && allMovieShows.length > 0) {
            activeShow = allMovieShows[0];
            currentShowId = activeShow.show_id;
        }

        if (activeShow) {
            currentShowId = activeShow.show_id;
            movieId = activeShow.movie_id || movieId;
            currentMovie = { ...(movieData || {}), ...activeShow };
            renderShowHeader(currentMovie);
            renderShowtimePills(allMovieShows);
            return;
        }

        // If movie was found but had no shows
        if (movieData) {
            currentMovie = movieData;
            renderShowHeader({
                title: movieData.title,
                theater_name: 'AuraCinemas Multiplex',
                city_name: 'Mumbai',
                show_date: new Date().toISOString().split('T')[0],
                start_time: '19:00',
                screen_name: 'Audi 1',
                screen_type: 'Dolby Atmos',
                poster_url: movieData.poster_url
            });
            return;
        }

        // Default fallback if no params provided at all
        movieId = allActiveCatalogMovies.length > 0 ? allActiveCatalogMovies[0].movie_id : 1;
        currentShowId = 1;
        const fallbackTitle = allActiveCatalogMovies.length > 0 ? allActiveCatalogMovies[0].title : 'Pathaan';
        renderShowHeader({
            title: fallbackTitle,
            theater_name: 'AuraCinemas Nexus Mall',
            city_name: 'Mumbai',
            show_date: '2026-10-06',
            start_time: '19:00',
            screen_name: 'Audi 1',
            screen_type: 'Dolby Atmos'
        });
    }

    function renderShowHeader(info) {
        document.getElementById('movieTitle').textContent = info.title || 'Movie Selection';
        document.getElementById('showTheater').textContent = `${info.theater_name || 'AuraCinemas'} (${info.city_name || 'Mumbai'})`;
        document.getElementById('showDate').textContent = formatDate(info.show_date || new Date().toISOString());
        document.getElementById('showTime').textContent = info.start_time || '18:30';
        document.getElementById('showScreen').textContent = `${info.screen_name || 'Screen 1'} (${info.screen_type || 'Standard'})`;
        document.getElementById('panelMovieName').textContent = `${info.title} / ${info.start_time || ''}`;

        const initialsEl = document.getElementById('posterInitials');
        if (initialsEl) {
            if (info.poster_url) {
                initialsEl.innerHTML = `<img src="${info.poster_url}" alt="${info.title}" style="width:100%; height:100%; object-fit:cover; border-radius:inherit;">`;
            } else if (info.title) {
                initialsEl.textContent = info.title.substring(0, 2).toUpperCase();
            }
        }
    }

    function renderShowtimePills(shows) {
        const wrapper = document.getElementById('showtimeSelectorWrapper');
        const container = document.getElementById('showtimePills');
        if (!wrapper || !container) return;

        if (!shows || shows.length <= 1) {
            wrapper.style.display = 'none';
            return;
        }

        wrapper.style.display = 'flex';
        container.innerHTML = shows.map(s => {
            const isSelected = s.show_id === currentShowId;
            return `
                <button type="button" class="btn btn--sm ${isSelected ? 'btn--primary' : 'btn--outline'}"
                        style="font-size:0.75rem; padding:0.25rem 0.65rem;"
                        onclick="window.AuraBooking && window.AuraBooking.switchShow(${s.show_id})">
                    ${s.start_time} (${s.screen_name})
                </button>
            `;
        }).join('');
    }

    async function switchShow(newShowId) {
        if (newShowId === currentShowId) return;
        currentShowId = newShowId;
        selectedSeats = [];
        const newUrl = `booking.html?movie_id=${movieId}&show_id=${newShowId}`;
        window.history.replaceState(null, '', newUrl);

        await loadShowInfo();
        await loadAdaptiveData();
        await loadSeatMap();
        updatePricingPanel();
    }
    window.AuraBooking = { switchShow, switchMovie };

    async function loadAdaptiveData() {
        try {
            const res = await fetch(`${API}/api/pricing/adaptive?movie_id=${movieId}&show_id=${currentShowId}`);
            if (res.ok) {
                const data = await res.json();
                if (data.success) {
                    adaptiveData = data;
                    surgeMultiplier = data.adaptive_multiplier || 1.0;
                    updateAdaptivePricingBadge(data);
                    return;
                }
            }
        } catch (e) {}

        // Fallback
        surgeMultiplier = 1.0;
        updateAdaptivePricingBadge({
            adaptive_multiplier: 1.0,
            price_change_percent: "+0.0%",
            traffic_level: "Normal Traffic",
            traffic_badge_class: "surge-normal"
        });
    }

    function updateAdaptivePricingBadge(data) {
        const badge = document.getElementById('surgeBadge');
        const text = document.getElementById('pricingStatusText');
        if (!badge || !text) return;

        const m = data.adaptive_multiplier || 1.0;
        const pct = data.price_change_percent || "+0.0%";
        const lvl = data.traffic_level || "Normal Demand";
        const loc = data.location_label || (data.theater_name ? `${data.theater_name}, ${data.city_name || ''}` : '');
        const occ = (data.occupancy_percent !== undefined && data.occupancy_percent > 0) ? ` • ${data.occupancy_percent}% Occupancy` : '';

        badge.className = 'surge-badge';
        if (m >= 1.25) {
            badge.classList.add('surge-badge--high');
            badge.textContent = `🔥 ${lvl} (${pct} / ${m.toFixed(2)}x)`;
            text.innerHTML = `High Demand Traffic Surge ${loc ? `at ${loc}` : ''}: <span style="color:var(--color-amber); font-weight:700;">${pct}</span>${occ}`;
        } else if (m > 1.00) {
            badge.classList.add('surge-badge--medium');
            badge.textContent = `⚡ Moderate Traffic (${pct} / ${m.toFixed(2)}x)`;
            text.innerHTML = `Traffic Surge ${loc ? `at ${loc}` : ''}: <span style="color:var(--color-amber); font-weight:700;">${pct}</span>${occ}`;
        } else if (m < 1.00) {
            badge.classList.add('surge-badge--low');
            badge.textContent = `🏷️ ${lvl} (${pct} / ${m.toFixed(2)}x)`;
            text.innerHTML = `Off-Peak Demand ${loc ? `at ${loc}` : ''}: <span style="color:var(--color-green); font-weight:700;">${pct}</span> discount (floor -20%)${occ}`;
        } else {
            badge.classList.add('surge-badge--normal');
            badge.textContent = `🟢 Baseline Normal (1.00x)`;
            text.innerHTML = `Standard Ticket Pricing ${loc ? `at ${loc}` : ''}${occ ? ` (${occ.replace(' • ', '')})` : ' (Original base rate)'}`;
        }
    }

    // ============================================================================
    // LOAD & RENDER SEATS
    // ============================================================================
    async function loadSeatMap() {
        let seatMap = null;
        try {
            const res = await fetch(`${API}/api/shows/${currentShowId}/seats`);
            if (res.ok) {
                const data = await res.json();
                seatMap = data.seat_map;
            }
        } catch (e) {}

        if (!seatMap) {
            seatMap = generateFallbackSeats();
        }

        renderSeatGrid(seatMap);
    }

    function renderSeatGrid(seatMap) {
        const grid = document.getElementById('seatGrid');
        grid.innerHTML = '';
        seatDataMap = {};

        const rowLabels = Object.keys(seatMap).sort();
        let currentCategory = '';

        rowLabels.forEach(rowLabel => {
            const seats = seatMap[rowLabel];
            if (!seats || seats.length === 0) return;

            const cat = seats[0].category;
            if (cat !== currentCategory) {
                currentCategory = cat;
                const divider = document.createElement('div');
                divider.className = 'category-divider';
                const adjustedPrice = Math.round(seats[0].base_price * surgeMultiplier);
                divider.innerHTML = `
                    <div class="category-divider__line"></div>
                    <span class="category-divider__label">${cat} - ₹${adjustedPrice}</span>
                    <div class="category-divider__line"></div>
                `;
                grid.appendChild(divider);
            }

            const rowEl = document.createElement('div');
            rowEl.className = 'seat-row';

            const lblEl = document.createElement('span');
            lblEl.className = 'seat-row__label';
            lblEl.textContent = rowLabel;
            rowEl.appendChild(lblEl);

            seats.forEach(s => {
                seatDataMap[s.seat_id] = s;

                const sEl = document.createElement('button');
                sEl.className = 'seat';
                sEl.dataset.seatId = s.seat_id;
                sEl.title = `${s.label} (${s.category}) - ₹${Math.round(s.base_price * surgeMultiplier)}`;

                if (s.category === 'Premium') sEl.classList.add('seat--premium');
                if (s.category === 'Recliner') sEl.classList.add('seat--recliner');

                if (s.status === 'booked') {
                    sEl.classList.add('seat--booked');
                    sEl.disabled = true;
                } else if (s.status === 'locked') {
                    sEl.classList.add('seat--booked');
                    sEl.title += ' (Temporarily Held)';
                    sEl.disabled = true;
                } else {
                    sEl.addEventListener('click', () => toggleSeat(s.seat_id));
                }

                rowEl.appendChild(sEl);
            });

            grid.appendChild(rowEl);
        });
    }

    function toggleSeat(seatId) {
        const seat = seatDataMap[seatId];
        if (!seat || seat.status !== 'available') return;

        const idx = selectedSeats.findIndex(s => s.seat_id === seatId);
        const el = document.querySelector(`[data-seat-id="${seatId}"]`);

        if (idx !== -1) {
            selectedSeats.splice(idx, 1);
            if (el) el.classList.remove('seat--selected');
        } else {
            if (selectedSeats.length >= MAX_SEATS) {
                showToast(`You can select at most ${MAX_SEATS} seats per booking`, 'info');
                return;
            }
            selectedSeats.push(seat);
            if (el) el.classList.add('seat--selected');

            // Record click hit for adaptive traffic
            try {
                fetch(`${API}/api/pricing/traffic-hit`, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ movie_id: movieId, show_id: currentShowId, action: 'seat_click' })
                });
            } catch (e) {}
        }

        updatePricingPanel();
    }

    // ============================================================================
    // PRICING PANEL & DYNAMIC CALCULATION
    // ============================================================================
    function calculateGrandTotal() {
        const baseTotal = selectedSeats.reduce((sum, s) => sum + s.base_price, 0);
        const surgeTotal = selectedSeats.reduce((sum, s) => sum + Math.round(s.base_price * surgeMultiplier), 0);
        const tax = Math.round(surgeTotal * GST_RATE * 100) / 100;
        const conv = selectedSeats.length * CONV_FEE;
        return Math.round(surgeTotal + tax + conv);
    }

    function updatePricingPanel() {
        const panelSeats = document.getElementById('panelSeats');
        const breakdown = document.getElementById('pricingBreakdown');
        const btnConfirm = document.getElementById('btnConfirm');

        const user = window.AuraAuth ? AuraAuth.getCurrentUser() : null;
        const isAdmin = user && user.role === 'admin';

        if (selectedSeats.length === 0) {
            panelSeats.innerHTML = '<p style="color: var(--text-muted); font-size: 0.8rem; text-align: center; padding: 1.25rem 0;">Click on seats to select them</p>';
            breakdown.style.display = 'none';
            btnConfirm.disabled = true;
            btnConfirm.textContent = isAdmin ? '🔒 Admin Account - Cannot Book Tickets' : 'Select Seats to Continue';
            return;
        }

        breakdown.style.display = 'block';

        panelSeats.innerHTML = selectedSeats.map(s => {
            const dynPrice = Math.round(s.base_price * surgeMultiplier);
            return `
                <div class="pricing-panel__seat-item">
                    <span>${s.label} (${s.category})</span>
                    <span style="font-weight:600;">₹${dynPrice}</span>
                </div>
            `;
        }).join('');

        const baseTotal = selectedSeats.reduce((sum, s) => sum + s.base_price, 0);
        const surgeTotal = selectedSeats.reduce((sum, s) => sum + Math.round(s.base_price * surgeMultiplier), 0);
        const diff = surgeTotal - baseTotal;
        const tax = Math.round(surgeTotal * GST_RATE * 100) / 100;
        const conv = selectedSeats.length * CONV_FEE;
        const total = Math.round(surgeTotal + tax + conv);

        document.getElementById('priceBase').textContent = `₹${baseTotal.toFixed(0)}`;
        
        const diffEl = document.getElementById('priceSurge');
        if (diff > 0) {
            diffEl.textContent = `+₹${diff.toFixed(0)} (${adaptiveData ? adaptiveData.price_change_percent : 'Surge'})`;
            diffEl.className = 'pricing-row__val--surge';
        } else if (diff < 0) {
            diffEl.textContent = `-₹${Math.abs(diff).toFixed(0)} (Off-Peak)`;
            diffEl.className = 'pricing-row__val--discount';
            diffEl.style.color = 'var(--color-green)';
        } else {
            diffEl.textContent = `₹0 (Base Price)`;
            diffEl.className = '';
        }

        document.getElementById('priceTax').textContent = `₹${tax.toFixed(0)}`;
        document.getElementById('priceConvenience').textContent = `₹${conv.toFixed(0)}`;
        document.getElementById('priceTotal').textContent = `₹${total.toFixed(0)}`;

        if (isAdmin) {
            btnConfirm.disabled = true;
            btnConfirm.textContent = '🔒 Admin Account - Cannot Book Tickets';
        } else {
            btnConfirm.disabled = false;
            btnConfirm.textContent = `Confirm & Pay ₹${total.toFixed(0)}`;
        }
    }

    // ============================================================================
    // BOOKING SUBMISSION & AUTOMATED EMAIL TRIGGER
    // ============================================================================
    async function handleBookingConfirm() {
        const user = window.AuraAuth ? AuraAuth.getCurrentUser() : null;
        if (!user) {
            showToast('Please sign in to your Customer account to finalize tickets.', 'info');
            AuraAuth.openLoginModal('customer');
            return;
        }

        // STRICT ENFORCEMENT: Admin cannot book tickets
        if (user.role === 'admin') {
            showToast('Admins cannot book tickets. Ticket booking is reserved for customers.', 'error');
            return;
        }

        if (selectedSeats.length === 0) return;

        const btn = document.getElementById('btnConfirm');
        btn.disabled = true;
        btn.textContent = 'Processing Booking & Dispatching Email...';

        try {
            // Lock seats
            await fetch(`${API}/api/shows/${currentShowId}/lock`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ seat_ids: selectedSeats.map(s => s.seat_id) })
            });

            // Confirm booking
            const bookRes = await fetch(`${API}/api/bookings`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    user_id: user.user_id,
                    show_id: currentShowId,
                    seats: selectedSeats.map(s => ({
                        seat_id: s.seat_id,
                        base_price: s.base_price,
                        seat_label: s.label,
                        category_name: s.category
                    })),
                    payment_method: 'UPI'
                })
            });

            const data = await bookRes.json();
            if (bookRes.ok && data.success) {
                showConfirmationModal(data.booking, data.email_notification);
                showToast(`Tickets booked! Automated email dispatched to ${user.email}`, 'success');
                markSeatsBooked();
                return;
            } else {
                showToast(data.error || 'Booking failed', 'error');
            }
        } catch (e) {
            showToast('Network error processing booking', 'error');
        } finally {
            checkUserRole();
        }
    }

    function markSeatsBooked() {
        selectedSeats.forEach(seat => {
            const el = document.querySelector(`[data-seat-id="${seat.seat_id}"]`);
            if (el) {
                el.classList.remove('seat--selected');
                el.classList.add('seat--booked');
                el.style.pointerEvents = 'none';
            }
            if (seatDataMap[seat.seat_id]) {
                seatDataMap[seat.seat_id].status = 'booked';
            }
        });
        selectedSeats = [];
        updatePricingPanel();
    }

    function showConfirmationModal(booking, emailInfo) {
        const content = document.getElementById('modalContent');
        const user = window.AuraAuth ? AuraAuth.getCurrentUser() : { email: 'your registered email' };
        const recipient = (emailInfo && emailInfo.recipient) || user.email;

        content.innerHTML = `
            <div style="text-align:center; color: var(--text-secondary); font-size: 0.9rem;">
                <p style="margin-bottom: 1rem; color:var(--text-primary); font-size:1.05rem;">
                    🎉 Congratulations! Your tickets have been confirmed.
                </p>

                <!-- Automated Email Notification Box -->
                <div style="background: rgba(74, 222, 128, 0.08); border: 1px solid rgba(74, 222, 128, 0.25); border-radius: var(--radius-md); padding: 1rem; margin-bottom: 1.25rem; text-align: left;">
                    <div style="display:flex; align-items:center; gap:0.5rem; margin-bottom:0.4rem;">
                        <span style="font-size:1.2rem;">✉️</span>
                        <strong style="color:var(--color-green); font-size:0.95rem;">Automated Email Dispatched</strong>
                    </div>
                    <div style="font-size:0.8rem; color:var(--text-secondary); line-height:1.5;">
                        A detailed ticket confirmation receipt with QR barcode and pricing breakdown has been sent to your registered email:
                        <br><strong style="color:var(--text-primary); font-size:0.85rem;">${recipient}</strong>
                    </div>
                    <div style="margin-top:0.75rem; display:flex; gap:0.5rem; flex-wrap:wrap;">
                        <button class="btn btn--outline btn--sm" onclick="AuraAuth.previewEmail(${booking.booking_id})" style="border-color:var(--color-green); color:var(--color-green); font-size:0.75rem;">
                            📄 View Sent Email Receipt
                        </button>
                        <button class="btn btn--outline btn--sm" onclick="AuraAuth.resendEmail(${booking.booking_id})" style="font-size:0.75rem;">
                            🔄 Resend Email
                        </button>
                    </div>
                </div>

                <!-- Booking Summary Card -->
                <div style="background: var(--bg-card); border-radius: var(--radius-md); padding: 1rem; text-align: left; border: 1px solid var(--border);">
                    <div class="pricing-row"><span>Booking ID</span><span style="font-weight:700; color: var(--accent);">#${booking.booking_id}</span></div>
                    <div class="pricing-row"><span>Transaction Ref</span><span style="font-size:0.75rem;">${booking.transaction_ref}</span></div>
                    <div class="pricing-row"><span>Amount Paid</span><span style="font-weight:700; color: var(--color-green);">₹${booking.grand_total.toFixed(0)}</span></div>
                    <div class="pricing-row"><span>Payment Status</span><span style="color: var(--color-green); font-weight:600;">Success (UPI)</span></div>
                </div>
            </div>
        `;
        document.getElementById('confirmModal').classList.add('active');
    }

    // ============================================================================
    // UTILS & FALLBACK DATA
    // ============================================================================
    function formatDate(dateStr) {
        if (!dateStr) return '';
        try {
            const d = new Date(dateStr);
            return d.toLocaleDateString('en-IN', { weekday: 'short', month: 'short', day: 'numeric' });
        } catch (e) {
            return dateStr;
        }
    }

    function showToast(msg, type = 'info') {
        const c = document.getElementById('toastContainer');
        if (!c) return;
        const t = document.createElement('div');
        t.className = `toast toast--${type}`;
        t.textContent = msg;
        c.appendChild(t);
        setTimeout(() => {
            t.classList.add('toast--exit');
            setTimeout(() => t.remove(), 200);
        }, 3000);
    }

    function generateFallbackSeats() {
        const categories = [
            { rows: ['A','B','C','D'], name: 'Standard', price: 200, cols: 14 },
            { rows: ['E','F','G','H'], name: 'Premium',  price: 300, cols: 14 },
            { rows: ['I','J'],         name: 'Recliner', price: 350, cols: 14 },
        ];
        const seats = {};
        let sid = 1;
        categories.forEach(cat => {
            cat.rows.forEach(r => {
                seats[r] = [];
                for (let n = 1; n <= cat.cols; n++) {
                    seats[r].push({
                        seat_id: sid,
                        show_seat_id: sid,
                        row: r,
                        number: n,
                        label: `${r}-${n}`,
                        category: cat.name,
                        status: 'available',
                        base_price: cat.price
                    });
                    sid++;
                }
            });
        });
        return seats;
    }

    // Bind confirm button
    document.addEventListener('DOMContentLoaded', () => {
        const btn = document.getElementById('btnConfirm');
        if (btn) {
            btn.addEventListener('click', handleBookingConfirm);
        }
        init();
    });
})();
