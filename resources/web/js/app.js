let backend = null;
let cachedRegions = [];
let activeTicketId = -1;
let currentRole = "";
let lastEditedElement = null;
let lastTicketsJson = "";
let lastMessagesJson = "";

function maskEmail(email) {
    if (!email) return "";
    const parts = email.split('@');
    if (parts.length < 2) return email;

    const name = parts[0];
    const domain = parts[1];

    if (name.length > 5) {
        return name.substring(0, 5) + "..." + "@" + domain;
    }
    return email;
}

document.addEventListener("DOMContentLoaded", function() {
    new QWebChannel(qt.webChannelTransport, function (channel) {
        backend = channel.objects.backend;
        console.log("WebChannel initialized");

        backend.regionsDataReceived.connect(function(list) {
            console.log("Regions received:", list);
            cachedRegions = list;
            
            const select = document.getElementById("reg-region");
            if (select) {
                select.innerHTML = ""; // Очищаем старые опции
                
                if (list.length === 0) {
                    let opt = document.createElement("option");
                    opt.innerText = "Нет доступных регионов";
                    select.appendChild(opt);
                } else {
                    list.forEach(function(region) {
                        let opt = document.createElement("option");
                        opt.value = region.id;
                        opt.innerText = region.name;
                        select.appendChild(opt);
                    });
                }
            }
        });

        backend.loginSuccess.connect(function(role, fio) {
            console.log("Login success:", role, fio);
            currentRole = role;
            
            // Скрываем авторизацию, показываем главный экран
            document.getElementById("auth-container").style.display = "none";
            document.getElementById("main-layout").style.display = "flex";
            
            document.getElementById("welcome-header").innerText = "Здравствуйте, " + fio;
            document.getElementById("user-role-badge").innerText = role.toUpperCase();
            
            if (role === "admin" || role === "manager") {
                document.getElementById("admin-panel").style.display = "block";
                document.getElementById("user-panel").style.display = "none";
                backend.requestDashboardData();
            } else {
                document.getElementById("admin-panel").style.display = "none";
                document.getElementById("user-panel").style.display = "block";
                backend.requestOwnData();
            }
        });

        backend.checkSession();

        backend.dashboardDataReceived.connect(renderAdminTable);
        
        backend.operationResult.connect((success, msg) => {
            console.log("Op result:", success, msg);
            if(success && (msg === "Обновлено" || msg === "Тикет создан")) showToast(msg, "success");
            else if (!success) showToast(msg, "error");
            
            if (success && lastEditedElement) {
                lastEditedElement.classList.add("saved");
                setTimeout(() => lastEditedElement.classList.remove("saved"), 2000);
            }
        });

        // Обновление списка тикетов
        backend.ticketsDataReceived.connect(function(tickets) {
            const currentJson = JSON.stringify(tickets);
            if (currentJson === lastTicketsJson) return; // Избегаем лишних перерисовок
            lastTicketsJson = currentJson;

            const list = document.getElementById("tickets-list");
            const scrollTop = list.scrollTop;
            list.innerHTML = "";
            
            if (!tickets || tickets.length === 0) { 
                list.innerHTML = "<div class='empty-state'></div>"; 
                return; 
            }
            
            const newBtn = document.getElementById("btn-new-ticket-list");
            if(newBtn) newBtn.style.display = (currentRole === 'user') ? "block" : "none";

            tickets.forEach(t => {
                const isActive = (t.id === activeTicketId);
                const el = document.createElement("div");
                el.className = `ticket-item ${isActive ? 'active' : ''}`;
                el.onclick = () => loadTicket(t.id, t.title, t.status);
                
                let statusIcon = t.status === 'open' ? '🟢' : '🔴';
                let unreadBadge = "";
                // Если мы поддержка и есть непрочитанные сообщения от юзера
                if (currentRole !== 'user' && t.unread === true) {
                    unreadBadge = `<span style="color:red; font-weight:bold; margin-right:5px;">●</span>`;
                    el.style.background = "#fff0f0";
                }
                
                if (isActive) updateChatControls(t.status);
                
                // Форматируем дату
                let dateStr = t.created_at ? t.created_at.replace('T', ' ').substring(5, 16) : '';
                
                el.innerHTML = `
                    <span class="t-title">${unreadBadge} #${t.id} ${t.title}</span>
                    <div class="t-meta">
                        <span>${statusIcon} ${t.author}</span>
                        <span>${dateStr}</span>
                    </div>`;
                list.appendChild(el);
            });
            list.scrollTop = scrollTop;
        });

        // Сообщения чата
        backend.ticketMessagesReceived.connect(function(ticketId, msgs) {
            if(activeTicketId !== ticketId) return;
            
            const currentJson = JSON.stringify(msgs);
            if (currentJson === lastMessagesJson) return;
            lastMessagesJson = currentJson;

            const chatBox = document.getElementById("chat-messages");
            // Проверяем, был ли скролл внизу, чтобы автопрокрутить новые сообщения
            const isAtBottom = (chatBox.scrollHeight - chatBox.scrollTop <= chatBox.clientHeight + 100);

            chatBox.innerHTML = "";
            msgs.forEach(m => {
                let isMe = false;
                // Логика "свой-чужой"
                if (currentRole === 'user' && m.role === 'user') isMe = true;
                if (currentRole !== 'user' && m.role !== 'user') isMe = true;
   
                const bubble = document.createElement("div");
                bubble.className = `msg ${isMe ? 'out' : 'in'}`;
                
                // Формат времени
                let timeStr = m.time ? m.time.replace('T', ' ').substring(11, 16) : '';
                
                bubble.innerHTML = `
                    <span class="msg-sender">${m.sender}</span>
                    ${m.text}
                    <span class="msg-time">${timeStr}</span>`;
                chatBox.appendChild(bubble);
            });
            
            if (isAtBottom || msgs.length < 5) chatBox.scrollTop = chatBox.scrollHeight;
        });

        backend.authCodeRequired.connect(email => { 
            showToast("Код отправлен", "success"); 
            toggleAuth('verify-block'); 
            document.getElementById("target-email").innerText = maskEmail(email); 
        });
        
        backend.ownDataReceived.connect((email, bal) => { 
            document.getElementById("my-email").innerText = maskEmail(email); 
            document.getElementById("my-balance").innerText = bal.toFixed(2) + " ₽"; 
        });
        
        backend.registrationSuccess.connect(() => { 
            showToast("Регистрация успешна! Войдите.", "success"); 
            showLogin(); 
        });
        
        backend.registrationFailed.connect(msg => showToast(msg, "error"));
        backend.loginFailed.connect(msg => showToast(msg, "error"));

        console.log("Requesting regions...");
        backend.requestAllRegions();
    });
});

function switchTab(tabName) {
    document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
    document.getElementById('tab-' + tabName).classList.add('active');
    
    document.querySelectorAll('.nav-links li').forEach(el => el.classList.remove('active'));
    document.getElementById('nav-' + tabName).classList.add('active');
    
    if (tabName !== 'tickets') { 
        activeTicketId = -1; 
    } else { 
        backend.requestTickets();
    }
}

function loadTicket(id, title, status) {
    if (activeTicketId === id) { updateChatControls(status); return; }
    
    activeTicketId = id;
    lastMessagesJson = "";
    
    document.getElementById("chat-header").style.display = "flex";
    document.getElementById("chat-input-area").style.display = "flex";
    document.getElementById("chat-title-text").innerText = `#${id} ${title}`;
    
    updateChatControls(status);
    
    document.querySelectorAll('.ticket-item').forEach(el => el.classList.remove('active'));
    
    backend.openTicket(id);
}

function updateChatControls(status) {
    const closeBtn = document.getElementById("btn-close-ticket");
    const inputAreaBtn = document.querySelector("#chat-input-area button");
    const textField = document.getElementById("msg-input");
    
    if (status === 'open') {
        closeBtn.style.display = "block";
        textField.disabled = false; 
        inputAreaBtn.disabled = false; 
        textField.placeholder = "Сообщение...";
    } else {
        closeBtn.style.display = "none";
        textField.disabled = true;
        textField.placeholder = "Тикет закрыт"; 
        inputAreaBtn.disabled = true; 
    }
}

function sendMsg() {
    const txt = document.getElementById("msg-input").value.trim();
    if (!txt || activeTicketId === -1) return;
    backend.sendTicketMessage(activeTicketId, txt);
    document.getElementById("msg-input").value = "";
}

function openCloseConfirmation() {
    document.getElementById("close-confirm-modal").style.display = "flex";
}
function confirmCloseTicket() {
    if(activeTicketId !== -1) backend.closeTicket(activeTicketId);
    document.getElementById("close-confirm-modal").style.display = "none";
}

function openNewTicketModalDirectly() { 
    document.getElementById("new-ticket-modal").style.display = "flex";
    setTimeout(() => document.getElementById("new-ticket-title").focus(), 100); 
}
function showNewTicketModal() { openNewTicketModalDirectly(); }

function closeNewTicketModal() { 
    document.getElementById("new-ticket-modal").style.display = "none";
}

function createTicket() {
    const t = document.getElementById("new-ticket-title").value; 
    const m = document.getElementById("new-ticket-msg").value;
    
    if(!t || !m) { showToast("Заполните все поля", "error"); return; }
    
    backend.createTicket(t, m); 
    closeNewTicketModal();
    
    document.getElementById("new-ticket-title").value=""; 
    document.getElementById("new-ticket-msg").value="";
    switchTab('tickets');
}

function renderAdminTable(users) {
    const tbody = document.getElementById("users-table-body"); 
    tbody.innerHTML = "";
    
    users.forEach(u => {
        let r = u.role ? u.role : 'user';
        const ent = `onkeydown="if(event.key==='Enter') this.blur()"`;
        
        let opts = cachedRegions.map(reg => 
            `<option value="${reg.id}" ${reg.id == u.region_id ? 'selected' : ''}>${reg.name}</option>`
        ).join('');
        
        let regSel = `<select class="admin-select" onchange="updateData(this, ${u.id}, 'region_id', this.value, '${r}')">${opts}</select>`;
        
        let roleSel = (r === 'admin') 
            ? `<span style="color:red;font-weight:bold;">ADMIN</span>` 
            : `<select class="admin-select" onchange="updateData(this, ${u.id}, 'role', this.value, '${r}')">
                 <option value="user" ${r==='user'?'selected':''}>User</option>
                 <option value="manager" ${r==='manager'?'selected':''}>Manager</option>
               </select>`;
     
        let balInp = (r === 'user') 
            ? `<input type="number" class="admin-input" value="${u.balance}" ${ent} onchange="updateData(this, ${u.id}, 'balance', this.value, '${r}')">` 
            : `—`;
            
        tbody.innerHTML += `<tr>
            <td>${u.id}</td>
            <td><input type="text" class="admin-input" value="${u.fio}" ${ent} onchange="updateData(this, ${u.id}, 'fio', this.value, '${r}')"></td>
            <td><input type="text" class="admin-input" value="${u.email}" ${ent} onchange="updateData(this, ${u.id}, 'email', this.value, '${r}')"></td>
            <td>${balInp}</td>
            <td>${roleSel}</td>
            <td>${regSel}</td>
        </tr>`;
    });
    filterTable();
}

function updateData(el, id, f, v, rt) { 
    if(backend) { 
        lastEditedElement = el; 
        backend.updateUserField(id, f, v, rt);
    } 
}

function filterTable() {
    let filter = document.getElementById("searchInput").value.toLowerCase();
    const rows = document.getElementById("users-table-body").getElementsByTagName("tr");
    
    for(let r of rows) {
        let txt = r.innerText.toLowerCase();
        for(let inp of r.getElementsByTagName("input")) txt += inp.value.toLowerCase(); 
        
        r.style.display = (txt.indexOf(filter) > -1) ? "" : "none";
    }
}

function val(id) { return document.getElementById(id).value; }

function attemptLogin() { backend.login(val("login-email"), val("login-pass")); }
function attemptVerify() { backend.verifyCode(val("login-email"), val("verify-code")); }
function attemptRegister() { 
    let regId = document.getElementById("reg-region").value;
    if (!regId) { showToast("Выберите регион", "error"); return; }
    
    backend.registerUser(val("reg-fio"), val("reg-email"), val("reg-pass"), parseInt(regId));
}

function showRegister() { toggleAuth('register-block'); }
function showLogin() { toggleAuth('login-block'); }

function toggleAuth(id) { 
    ['login-block','register-block','verify-block'].forEach(b => document.getElementById(b).style.display = 'none'); 
    document.getElementById(id).style.display = 'flex'; 
}

function logout() { 
    backend.logout(); 
    location.reload();
}

function showToast(msg, type) {
    const d = document.createElement("div"); 
    d.className = `toast ${type}`; 
    d.innerText = msg;
    document.getElementById("toast-container").appendChild(d);
    
    setTimeout(() => d.classList.add("visible"), 10);
    setTimeout(() => { 
        d.classList.remove("visible"); 
        setTimeout(()=>d.remove(), 400); 
    }, 3000);
}

function userTopUp() { 
    document.getElementById("topup-modal").style.display = "flex";
}
function confirmTopUp() { 
    let v = parseFloat(val("topup-amount")); 
    if(v>=60) { 
        backend.initiatePayment(v); 
        document.getElementById("topup-modal").style.display="none"; 
    } else {
        showToast("Минимум 60 RUB", "error");
    }
}