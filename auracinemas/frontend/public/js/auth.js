// ============================================================================
// AuraCinemas - Shared Authentication & Navigation Manager
// Strict Role Separation: Customers vs Administrators
// Initial state: Logged Out (Guest). Reliable Logout.
// ============================================================================
(() => {
    'use strict';

    const API_BASE = (window.location.protocol === 'file:' || (window.location.port && window.location.port !== '8080'))
        ? 'http://localhost:8080'
        : '';
    const STORAGE_KEY = 'aura_cinemas_current_user';

    window.AuraAuth = {
        // Initial state: NO ONE is logged in unless explicitly stored in localStorage
        getCurrentUser() {
            try {
                const stored = localStorage.getItem(STORAGE_KEY);
                if (stored) {
                    const parsed = JSON.parse(stored);
                    if (parsed && parsed.email) {
                        return parsed;
                    }
                }
            } catch (e) {}
            return null; // Guest by default!
        },

        setCurrentUser(user) {
            try {
                if (user) {
                    localStorage.setItem(STORAGE_KEY, JSON.stringify(user));
                } else {
                    localStorage.removeItem(STORAGE_KEY);
                }
                this.updateNavbar();
                window.dispatchEvent(new CustomEvent('aura-user-changed', { detail: user }));
            } catch (e) {}
        },

        logout(e) {
            if (e && e.preventDefault) e.preventDefault();
            try {
                localStorage.removeItem(STORAGE_KEY);
                sessionStorage.clear();
            } catch (err) {}
            this.updateNavbar();
            window.dispatchEvent(new CustomEvent('aura-user-changed', { detail: null }));
            if (window.showToast) {
                window.showToast('You have been signed out successfully.', 'info');
            }
            // If on admin console, redirect to public homepage
            if (window.location.pathname.includes('admin.html')) {
                window.location.href = 'index.html';
            } else {
                window.location.reload();
            }
        },

        async login(email, password, role = 'customer') {
            try {
                const res = await fetch(`${API_BASE}/api/auth/login`, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ email, password, role })
                });
                const data = await res.json();
                if (res.ok && data.success) {
                    this.setCurrentUser(data.user);
                    return { success: true, user: data.user };
                }
                return { success: false, error: data.error || 'Invalid credentials' };
            } catch (e) {
                return { success: false, error: 'Network error connecting to auth server' };
            }
        },

        async register(userData) {
            try {
                // Public registration is strictly customer
                userData.role = 'customer';
                const res = await fetch(`${API_BASE}/api/auth/register`, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(userData)
                });
                const data = await res.json();
                if (res.ok && data.success) {
                    this.setCurrentUser(data.user);
                    return { success: true, user: data.user };
                }
                return { success: false, error: data.error || 'Registration failed' };
            } catch (e) {
                return { success: false, error: 'Network error connecting to auth server' };
            }
        },

        openLoginModal() {
            let modal = document.getElementById('auraAuthModal');
            if (!modal) {
                this.injectAuthModal();
                modal = document.getElementById('auraAuthModal');
            }
            this.switchAuthTab('login');
            modal.classList.add('active');
        },

        openRegisterModal() {
            let modal = document.getElementById('auraAuthModal');
            if (!modal) {
                this.injectAuthModal();
                modal = document.getElementById('auraAuthModal');
            }
            this.switchAuthTab('register');
            modal.classList.add('active');
        },

        closeLoginModal() {
            const modal = document.getElementById('auraAuthModal');
            if (modal) modal.classList.remove('active');
        },

        switchAuthTab(tab) {
            document.querySelectorAll('.auth-tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.auth-tab-pane').forEach(p => p.classList.remove('active'));

            const btn = document.getElementById(`tabBtn_${tab}`);
            const pane = document.getElementById(`tabPane_${tab}`);
            if (btn) btn.classList.add('active');
            if (pane) pane.classList.add('active');

            const title = document.getElementById('authModalTitle');
            if (title) {
                title.innerHTML = tab === 'register' ? '✨ Create Customer Account' : '👤 Customer Sign In';
            }
        },

        injectAuthModal() {
            if (document.getElementById('auraAuthModal')) return;

            const modalHtml = `
            <div class="modal-overlay" id="auraAuthModal">
                <div class="modal auth-modal-box">
                    <button class="modal-close-btn" onclick="AuraAuth.closeLoginModal()">&times;</button>
                    <div style="text-align:center; margin-bottom: 1.25rem;">
                        <h2 class="modal__title" id="authModalTitle" style="margin-bottom:0.3rem;">👤 Customer Sign In</h2>
                        <p style="font-size:0.8rem; color:var(--text-secondary);">Sign in to book tickets and receive automated email receipts</p>
                    </div>

                    <div class="auth-tabs">
                        <button class="auth-tab-btn active" id="tabBtn_login" onclick="AuraAuth.switchAuthTab('login')">Sign In</button>
                        <button class="auth-tab-btn" id="tabBtn_register" onclick="AuraAuth.switchAuthTab('register')">Register New Account</button>
                    </div>

                    <!-- Customer Login Form -->
                    <div class="auth-tab-pane active" id="tabPane_login">
                        <div class="auth-quick-actions" style="margin-bottom:1rem;">
                            <button type="button" class="btn btn--outline btn--sm btn--block" onclick="AuraAuth.fillDemoCustomer()" style="font-size:0.75rem; border-color:var(--accent); color:var(--accent);">
                                ⚡ Fill Demo Customer (Arjun Sharma)
                            </button>
                        </div>
                        <form id="customerLoginForm" onsubmit="AuraAuth.handleCustomerSubmit(event)">
                            <div class="form-group">
                                <label class="form-label">Customer Email</label>
                                <input type="email" id="customerEmail" class="form-control" required placeholder="name@example.com">
                            </div>
                            <div class="form-group">
                                <label class="form-label">Password</label>
                                <input type="password" id="customerPassword" class="form-control" required placeholder="••••••••">
                            </div>
                            <div id="customerLoginError" class="auth-error-msg" style="display:none;"></div>
                            <button type="submit" class="btn btn--primary btn--block" style="margin-top:1rem;">Sign In as Customer</button>
                        </form>
                    </div>

                    <!-- Customer Registration Form -->
                    <div class="auth-tab-pane" id="tabPane_register">
                        <form id="registerForm" onsubmit="AuraAuth.handleRegisterSubmit(event)">
                            <div class="form-group">
                                <label class="form-label">Full Name *</label>
                                <input type="text" id="regName" class="form-control" required placeholder="Your full name">
                            </div>
                            <div class="form-group">
                                <label class="form-label">Email (Automated ticket emails will be sent here) *</label>
                                <input type="email" id="regEmail" class="form-control" required placeholder="your.mail@example.com">
                            </div>
                            <div class="form-group">
                                <label class="form-label">City / Region</label>
                                <input type="text" id="regCity" class="form-control" placeholder="e.g. Mumbai, Pune, Delhi" value="Mumbai">
                            </div>
                            <div class="form-group">
                                <label class="form-label">Password *</label>
                                <input type="password" id="regPassword" class="form-control" required placeholder="Minimum 6 characters">
                            </div>
                            <div id="registerError" class="auth-error-msg" style="display:none;"></div>
                            <button type="submit" class="btn btn--primary btn--block" style="margin-top:1rem;">Create Customer Account</button>
                        </form>
                    </div>
                </div>
            </div>
            `;
            document.body.insertAdjacentHTML('beforeend', modalHtml);
            const m = document.getElementById('auraAuthModal');
            if (m) {
                m.addEventListener('click', (e) => {
                    if (e.target.id === 'auraAuthModal') AuraAuth.closeLoginModal();
                });
            }
        },

        fillDemoCustomer() {
            document.getElementById('customerEmail').value = 'arjun@example.com';
            document.getElementById('customerPassword').value = 'user123';
        },

        async handleCustomerSubmit(e) {
            e.preventDefault();
            const errEl = document.getElementById('customerLoginError');
            errEl.style.display = 'none';

            const email = document.getElementById('customerEmail').value.trim();
            const password = document.getElementById('customerPassword').value;

            // Submit specifically as customer
            const res = await this.login(email, password, 'customer');
            if (res.success) {
                this.closeLoginModal();
                if (window.showToast) window.showToast(`Welcome back, ${res.user.full_name}!`, 'success');
                setTimeout(() => window.location.reload(), 150);
            } else {
                errEl.textContent = res.error;
                errEl.style.display = 'block';
            }
        },

        async handleRegisterSubmit(e) {
            e.preventDefault();
            const errEl = document.getElementById('registerError');
            errEl.style.display = 'none';

            const full_name = document.getElementById('regName').value.trim();
            const email = document.getElementById('regEmail').value.trim();
            const city = document.getElementById('regCity').value.trim() || 'Mumbai';
            const password = document.getElementById('regPassword').value;

            const res = await this.register({ full_name, email, city, password });
            if (res.success) {
                this.closeLoginModal();
                if (window.showToast) window.showToast(`Welcome to AuraCinemas, ${full_name}!`, 'success');
                setTimeout(() => window.location.reload(), 150);
            } else {
                errEl.textContent = res.error;
                errEl.style.display = 'block';
            }
        },

        updateNavbar() {
            const user = this.getCurrentUser();
            const navContainer = document.querySelector('.navbar__nav');
            if (!navContainer) return;

            let userWidget = document.getElementById('navbarUserWidget');
            if (!userWidget) {
                userWidget = document.createElement('div');
                userWidget.id = 'navbarUserWidget';
                userWidget.className = 'navbar-user-widget';
                navContainer.appendChild(userWidget);
            }

            if (user) {
                if (user.role === 'admin') {
                    userWidget.innerHTML = `
                        <div class="user-pill user-pill--admin">
                            <span class="user-badge-role">👑 Administrator</span>
                            <span class="user-name">${user.full_name.split(' ')[0]}</span>
                        </div>
                        <a href="admin.html" class="btn btn--outline btn--sm" style="font-size:0.75rem; padding:0.35rem 0.75rem; border-color:var(--accent); color:var(--accent);">Admin Console</a>
                        <button class="btn btn--outline btn--sm" onclick="AuraAuth.logout()" style="font-size:0.75rem; padding:0.35rem 0.6rem; color:var(--text-muted); border-color:#333;">Sign Out</button>
                    `;
                } else {
                    userWidget.innerHTML = `
                        <div class="user-pill">
                            <span class="user-badge-role">👤 Customer</span>
                            <span class="user-name">${user.full_name.split(' ')[0]}</span>
                        </div>
                        <button class="btn btn--outline btn--sm" onclick="AuraAuth.openBookingsModal()" style="font-size:0.75rem; padding:0.35rem 0.75rem;">My Bookings</button>
                        <button class="btn btn--outline btn--sm" onclick="AuraAuth.logout()" style="font-size:0.75rem; padding:0.35rem 0.6rem; color:var(--text-muted); border-color:#333;">Sign Out</button>
                    `;
                }
            } else {
                userWidget.innerHTML = `
                    <button class="btn btn--outline btn--sm" onclick="AuraAuth.openLoginModal()" style="font-size:0.75rem;">Sign In</button>
                    <button class="btn btn--primary btn--sm" onclick="AuraAuth.openRegisterModal()" style="font-size:0.75rem;">Register</button>
                `;
            }
        },

        async openBookingsModal() {
            const user = this.getCurrentUser();
            if (!user) {
                this.openLoginModal();
                return;
            }

            let modal = document.getElementById('auraBookingsModal');
            if (!modal) {
                this.injectBookingsModal();
                modal = document.getElementById('auraBookingsModal');
            }

            const listEl = document.getElementById('userBookingsList');
            listEl.innerHTML = '<p style="text-align:center; color:var(--text-muted); padding:2rem;">Loading bookings...</p>';
            modal.classList.add('active');

            try {
                const res = await fetch(`${API_BASE}/api/user/bookings?user_id=${user.user_id}`);
                const data = await res.json();
                if (data.bookings && data.bookings.length > 0) {
                    listEl.innerHTML = data.bookings.map(b => `
                        <div class="booking-history-card">
                            <div class="bh-header">
                                <div>
                                    <strong style="color:var(--text-primary); font-size:1rem;">${b.title}</strong>
                                    <div style="font-size:0.75rem; color:var(--text-secondary);">${b.theater_name} (${b.screen_name}) | ${b.city_name}</div>
                                </div>
                                <span class="bh-badge bh-badge--${b.booking_status}">${b.booking_status.toUpperCase()}</span>
                            </div>
                            <div class="bh-grid">
                                <div><strong>Date:</strong> ${b.show_date} at ${b.start_time}</div>
                                <div><strong>Tickets:</strong> ${b.num_seats} seats (${b.seats.map(s => s.seat_label).join(', ')})</div>
                                <div><strong>Total Paid:</strong> ₹${b.grand_total.toFixed(0)} (${b.payment_method || 'UPI'})</div>
                                <div><strong>Registered Email:</strong> ${user.email}</div>
                            </div>
                            <div class="bh-footer">
                                <span style="font-size:0.75rem; color:var(--color-green);">
                                    ✉️ Confirmation email sent to ${user.email}
                                </span>
                                <div style="display:flex; gap:0.5rem;">
                                    <button class="btn btn--outline btn--sm" onclick="AuraAuth.previewEmail(${b.booking_id})">View Email Receipt</button>
                                    <button class="btn btn--outline btn--sm" onclick="AuraAuth.resendEmail(${b.booking_id})">Resend Email</button>
                                </div>
                            </div>
                        </div>
                    `).join('');
                } else {
                    listEl.innerHTML = `
                        <div style="text-align:center; padding:3rem 1rem; color:var(--text-muted);">
                            <p style="font-size:1.1rem; margin-bottom:0.5rem;">No bookings yet</p>
                            <p style="font-size:0.8rem;">Browse movies and book tickets to receive automated email receipts.</p>
                            <a href="index.html" class="btn btn--primary btn--sm" style="margin-top:1rem; display:inline-block;">Explore Movies</a>
                        </div>
                    `;
                }
            } catch (e) {
                listEl.innerHTML = '<p style="color:var(--color-red); text-align:center;">Failed to load bookings.</p>';
            }
        },

        injectBookingsModal() {
            const html = `
            <div class="modal-overlay" id="auraBookingsModal">
                <div class="modal bookings-modal-box">
                    <button class="modal-close-btn" onclick="document.getElementById('auraBookingsModal').classList.remove('active')">&times;</button>
                    <h2 class="modal__title" style="text-align:left; margin-bottom:0.5rem;">🎟️ My Confirmed Bookings</h2>
                    <p style="font-size:0.8rem; color:var(--text-secondary); margin-bottom:1.25rem;">
                        View your tickets and automated email receipts sent to your registered email.
                    </p>
                    <div id="userBookingsList" class="bookings-scroll-area"></div>
                </div>
            </div>
            `;
            document.body.insertAdjacentHTML('beforeend', html);
            const bm = document.getElementById('auraBookingsModal');
            if (bm) {
                bm.addEventListener('click', (e) => {
                    if (e.target.id === 'auraBookingsModal') bm.classList.remove('active');
                });
            }
        },

        async previewEmail(bookingId) {
            let modal = document.getElementById('auraEmailModal');
            if (!modal) {
                this.injectEmailModal();
                modal = document.getElementById('auraEmailModal');
            }

            const frame = document.getElementById('emailReceiptFrame');
            frame.src = `${API_BASE}/api/emails/${bookingId}?format=html`;
            modal.classList.add('active');
        },

        injectEmailModal() {
            const html = `
            <div class="modal-overlay" id="auraEmailModal">
                <div class="modal email-modal-box">
                    <button class="modal-close-btn" onclick="document.getElementById('auraEmailModal').classList.remove('active')">&times;</button>
                    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:0.75rem;">
                        <h2 class="modal__title" style="margin:0; font-size:1.1rem;">✉️ Automated Ticket Email Receipt</h2>
                        <a id="emailOpenNewTab" href="#" target="_blank" class="btn btn--outline btn--sm" style="font-size:0.7rem;">Open in New Tab</a>
                    </div>
                    <div class="email-frame-container">
                        <iframe id="emailReceiptFrame" style="width:100%; height:480px; border:none; border-radius:8px; background:#121212;"></iframe>
                    </div>
                </div>
            </div>
            `;
            document.body.insertAdjacentHTML('beforeend', html);
            document.getElementById('auraEmailModal').addEventListener('click', (e) => {
                if (e.target.id === 'auraEmailModal') e.target.classList.remove('active');
            });
        },

        async resendEmail(bookingId) {
            try {
                const res = await fetch(`${API_BASE}/api/emails/resend`, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ booking_id: bookingId })
                });
                const data = await res.json();
                if (res.ok && data.success) {
                    if (window.showToast) window.showToast(data.message, 'success');
                    else alert(data.message);
                } else {
                    alert('Error resending email: ' + (data.error || 'Server error'));
                }
            } catch (e) {
                alert('Network error resending email');
            }
        },

        init() {
            this.injectAuthModal();
            this.injectBookingsModal();
            this.injectEmailModal();
            this.updateNavbar();
        }
    };

    // Auto-initialize
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => AuraAuth.init());
    } else {
        AuraAuth.init();
    }
})();
