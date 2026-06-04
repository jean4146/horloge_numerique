#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <Wire.h>          
#include <RtcDS3231.h>     
#include <U8g2lib.h> 

// ==========================================
// CONFIGURATION DU RESEAU WIFI (Point d'Accès)
// ==========================================
const char* ap_ssid = "ClockDesk_AP";
const char* ap_password = "12345678"; 
WebServer server(80);

// ==========================================
// CONFIGURATION DU MATERIEL 
// ==========================================
#define DHTPIN 4     
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define BTN_MODE 32
#define BTN_MINUS 33
#define BTN_PLUS 26

RtcDS3231<TwoWire> Rtc(Wire);

U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R1, /* clock=*/ 18, /* data=*/ 23, /* CS=*/ 5, /* reset=*/ 14);
Hide quoted text

#define BUZZER_PIN 15
#define BUZZER_ON LOW
#define BUZZER_OFF HIGH

// ==========================================
// VARIABLES GLOBALES
// ==========================================
float temperature = 0.0;
unsigned long lastDHTRead = 0;
unsigned long lastDisplayUpdate = 0;
String currentLang = "FR";
bool is12hFormat = false; 
int dateFormat = 0; 

bool hourBeeped = false;
unsigned long buzzerTimer = 0;
bool isBuzzing = false;

enum MenuState { NORMAL, SET_HOUR, SET_MIN, SET_DAY, SET_MONTH, SET_YEAR };
MenuState currentMenu = NORMAL;
int editHour, editMin, editDay, editMonth, editYear;
bool lastBtnMode = HIGH;
bool lastBtnMin = HIGH;
bool lastBtnPlus = HIGH;

int getMaxDays(int year, int month);
int calculateDayOfWeek(int d, int m, int y);
String getDayStr(int dow, String lang);
bool checkButton(int pin, bool &lastState);
void gererBoutons();
void afficherEcran();

// ==========================================
// PAGE WEB HTML
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>ClockDesk</title>
    <style>
        :root { --brand: #f59e0b; --brand-dark: #d97706; --dark: #1a1c29; --bg: #f3f4f6; --text: #1f2937; --text-muted: #9ca3af; --card: #ffffff; }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: system-ui, -apple-system, sans-serif; }
        body { background-color: var(--bg); color: var(--text); display: flex; flex-direction: column; min-height: 100vh; }
        header { background: rgba(255, 255, 255, 0.9); border-bottom: 1px solid #e5e7eb; padding: 15px 20px; display: flex; justify-content: space-between; align-items: center; position: sticky; top: 0; z-index: 50; }
        .logo { font-size: 1.2rem; font-weight: 800; display: flex; align-items: center; gap: 8px;}
        .logo span { color: var(--brand); }
        .nav-tabs { display: flex; gap: 20px; }
        .tab-btn { background: none; border: none; font-size: 1rem; font-weight: 600; color: var(--text-muted); cursor: pointer; padding-bottom: 5px; border-bottom: 2px solid transparent; transition: 0.2s; }
        .tab-btn.active { color: var(--brand); border-bottom-color: var(--brand); }
        .status { padding: 4px 12px; border-radius: 20px; font-size: 0.8rem; font-weight: bold; display: flex; align-items: center; gap: 8px; }
        .status.searching { background: #fef3c7; color: #d97706; border: 1px solid #fde68a; }
        .status.connected { background: #d1fae5; color: #059669; border: 1px solid #a7f3d0; }
        .status.error { background: #fee2e2; color: #dc2626; border: 1px solid #fecaca; }
        .dot { width: 8px; height: 8px; border-radius: 50%; background: currentcolor; animation: pulse 2s infinite; }
        @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.5; } 100% { opacity: 1; } }
        main { flex: 1; padding: 20px; max-width: 900px; margin: 0 auto; width: 100%; padding-bottom: 80px; }
        .tab-content { display: none; animation: fadeIn 0.3s ease; }
        .tab-content.active { display: block; }
        @keyframes fadeIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }
        .section-title { font-size: 0.8rem; font-weight: 700; color: var(--text-muted); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 15px; }
        h2.title { font-size: 1.8rem; font-weight: 800; margin-bottom: 20px; }
        .card-dark { background: var(--dark); color: #fff; border-radius: 24px; padding: 40px 20px; text-align: center; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.1); display: flex; flex-direction: column; justify-content: center; min-height: 300px; }
        .card-light { background: var(--card); border-radius: 16px; padding: 25px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05); margin-bottom: 20px; display: flex; flex-direction: column; }
        .grid-2 { display: grid; grid-template-columns: 1fr; gap: 20px; }
        @media(min-width: 768px) { .grid-2 { grid-template-columns: 1fr 1fr; } }
        .time-display { font-size: 3.5rem; font-family: monospace; font-weight: 700; letter-spacing: 2px; display: flex; justify-content: center; align-items: baseline; }
        @media(min-width: 768px) { .time-display { font-size: 6rem; } }
        .time-display .colon { opacity: 0.5; margin: 0 5px; }
        .time-display .ampm { font-size: 1.5rem; color: var(--brand); margin-left: 10px; }
        .date-display { color: var(--text-muted); font-size: 1.1rem; font-weight: 500; margin-top: 10px; text-transform: capitalize; }
        .temp-footer { margin-top: 30px; border-top: 1px solid #374151; padding-top: 20px; display: flex; justify-content: center; align-items: center; gap: 10px; }
        .temp-footer .val { font-size: 1.5rem; font-weight: bold; }
        .temp-footer .lbl { font-size: 0.8rem; color: var(--text-muted); }
        .temp-large { font-size: 5rem; font-weight: bold; display: flex; align-items: start; justify-content: center; margin-bottom: 30px; }
        .temp-large span { color: var(--brand); font-size: 2rem; margin-top: 10px; }
        .card-header { display: flex; align-items: center; gap: 10px; border-bottom: 1px solid #f3f4f6; padding-bottom: 15px; margin-bottom: 20px; font-weight: 700; font-size: 1.2rem; }
        label { display: block; font-size: 0.75rem; font-weight: 700; color: var(--text-muted); text-transform: uppercase; margin-bottom: 8px; text-align: center; }
        .input-group { display: flex; justify-content: center; align-items: center; gap: 10px; margin-bottom: 25px; }
        .input-group span { font-size: 1.5rem; color: #d1d5db; font-weight: bold; }
        input[type="number"], select { width: 100%; padding: 12px; border-radius: 12px; border: 1px solid #e5e7eb; background: #f9fafb; font-size: 1rem; font-weight: 600; color: var(--text); text-align: center; outline: none; }
        input[type="number"]:focus, select:focus { border-color: var(--brand); }
        .input-group input { width: 60px; font-size: 1.2rem; }
        .btn { width: 100%; padding: 14px; border-radius: 12px; border: none; font-size: 1rem; font-weight: 700; cursor: pointer; display: flex; justify-content: center; align-items: center; gap: 8px; transition: 0.2s; margin-top: auto; }
        .btn-outline { background: #fff; border: 1px solid #e5e7eb; color: #4b5563; }
        .btn-brand { background: var(--brand); color: #fff; }
        .toast { position: fixed; top: 20px; left: 50%; transform: translate(-50%, -100px); background: #10b981; color: white; padding: 12px 24px; border-radius: 30px; font-weight: 600; transition: 0.3s cubic-bezier(0.68, -0.55, 0.265, 1.55); z-index: 100; box-shadow: 0 4px 12px rgba(0,0,0,0.15); display: flex; align-items: center; gap: 8px; }
        .toast.show { transform: translate(-50%, 0); }
        .toast.error { background: #ef4444; }
        .mobile-nav { display: none; position: fixed; bottom: 0; width: 100%; background: #fff; border-top: 1px solid #e5e7eb; padding: 10px 0; justify-content: space-around; z-index: 50; }
        .mob-btn { background: none; border: none; display: flex; flex-direction: column; align-items: center; color: var(--text-muted); font-size: 1.5rem; }
        .mob-btn span { font-size: 0.65rem; font-weight: 700; margin-top: 4px; }
        .mob-btn.active { color: var(--brand); }
        @media(max-width: 767px) { header .nav-tabs { display: none; } .mobile-nav { display: flex; } .status span { display: none; } }
    </style>
</head>
<body>
    <header>
        <div class="logo">🕒 Clock<span>Desk</span></div>
        <nav class="nav-tabs">
            <button class="tab-btn active" data-i18n="tab_overview" data-target="apercu">Aperçu</button>
            <button class="tab-btn" data-i18n="tab_settings" data-target="reglages">Réglages</button>
            <button class="tab-btn" data-i18n="tab_temp" data-target="temperature">Temp.</button>
        </nav>
        <div id="status-container" class="status searching">
            <div class="dot"></div>
            <span id="conn-status" data-i18n="status_searching">Recherche...</span>
        </div>
    </header>

    <main>
        <section id="view-apercu" class="tab-content active">
            <div class="section-title" data-i18n="live_display">Affichage en direct</div>
            <div class="card-dark">
                <div>
                    <div class="time-display">
                        <span id="preview-h">--</span><span class="colon">:</span>
                        <span id="preview-m">--</span><span class="colon">:</span>
                        <span id="preview-s">--</span>
                        <span id="preview-ampm" class="ampm"></span>
                    </div>
                    <div class="date-display" id="preview-date">Synchronisation...</div>
                </div>
                <div class="temp-footer">
                    <div>🌡️</div>
                    <div>
                        <div class="val"><span id="preview-temp">--</span> °C</div>
                        <div class="lbl" data-i18n="ambient_temp">Température ambiante</div>
                    </div>
                </div>
            </div>
        </section>

        <section id="view-reglages" class="tab-content">
            <h2 class="title" data-i18n="settings_title">Réglages</h2>
            <div class="grid-2">
                <div class="card-light">
                    <div class="card-header">⚙️ <span data-i18n="time_set">Heure & Date</span></div>
                    
                    <label>Heure (H : M : S)</label>
                    <div class="input-group">
                        <input type="number" id="input_heure" placeholder="HH"> <span>:</span>
                        <input type="number" id="input_minute" placeholder="MM"> <span>:</span>
                        <input type="number" id="input_seconde" placeholder="SS">
                    </div>
                    
                    <label>Date (JJ / MM / AAAA)</label>
                    <div class="input-group">
                        <input type="number" id="input_jour" placeholder="JJ"> <span>/</span>
                        <input type="number" id="input_mois" placeholder="MM"> <span>/</span>
                        <input type="number" id="input_annee" placeholder="AAAA" style="width: 80px;">
                    </div>
                    <button id="btn-sync" class="btn btn-outline">🔄 <span data-i18n="sync_pc">Synchroniser</span></button>
                </div>

                <div class="card-light">
                    <div class="card-header">🌍 <span data-i18n="localization">Format & Langue</span></div>
                    <label data-i18n="time_format">Format de l'Heure</label>
                    <div class="input-group">
                        <select id="input_format"><option value="24">24 Heures</option><option value="12">12 Heures (AM/PM)</option></select>
                    </div>
                    <label data-i18n="date_format_label">Format d'Affichage LCD</label>
                    <div class="input-group">
                        <select id="input_date_format"><option value="0">JJ/MM/AA</option><option value="1">MM/JJ/AA</option><option value="2">AA/MM/JJ</option></select>
                    </div>
                    <label data-i18n="interface_lang">Langue du système</label>
                    <div class="input-group">
                        <select id="input_lang"><option value="FR">Français (FR)</option><option value="EN">English (EN)</option><option value="ES">Español (ES)</option><option value="DE">Deutsch (DE)</option></select>
                    </div>
                    <button id="btn-save" class="btn btn-brand">💾 <span data-i18n="save_btn">Sauvegarder tout</span></button>
                </div>
            </div>
        </section>

        <section id="view-temperature" class="tab-content">
            <h2 class="title" data-i18n="temp_sensor">Capteur de Température</h2>
            <div class="card-dark">
                <div class="section-title" data-i18n="reading_live">Lecture en direct</div>
                <div class="temp-large"><span id="temp-large">--</span><span>°C</span></div>
                <div style="max-width: 300px; margin: 0 auto; width: 100%;">
                    <button id="btn-force" class="btn btn-brand">🔄 <span data-i18n="force_read">Actualiser</span></button>
                </div>
            </div>
        </section>
    </main>

    <div id="toastMsg" class="toast"><span></span></div>
    <div class="mobile-nav">
        <button class="mob-btn active" data-target="apercu">🕒 <span data-i18n="tab_overview">Aperçu</span></button>
        <button class="mob-btn" data-target="reglages">⚙️ <span data-i18n="tab_settings">Réglages</span></button>
        <button class="mob-btn" data-target="temperature">🌡️ <span data-i18n="tab_temp">Temp.</span></button>
    </div>

    <script>
        // TRADUCTIONS ET NAVIGATION 
        const i18n = {
            'FR': { 'tab_overview': 'Aperçu', 'tab_settings': 'Réglages', 'tab_temp': 'Temp.', 'status_searching': 'Recherche...', 'status_connected': 'Connecté', 'status_error': 'Déconnecté', 'live_display': 'Affichage en direct', 'ambient_temp': 'Température ambiante', 'settings_title': 'Réglages', 'time_set': 'Heure & Date', 'sync_pc': 'Synchroniser l\'appareil', 'localization': 'Format & Langue', 'time_format': 'Format de l\'Heure', 'date_format_label': 'Format Date (LCD)', 'interface_lang': 'Langue', 'save_btn': 'Sauvegarder tout', 'temp_sensor': 'Capteur de Temp.', 'reading_live': 'Lecture en direct', 'force_read': 'Actualiser' },
            'EN': { 'tab_overview': 'Overview', 'tab_settings': 'Settings', 'tab_temp': 'Temp.', 'status_searching': 'Searching...', 'status_connected': 'Connected', 'status_error': 'Unreachable', 'live_display': 'Live Display', 'ambient_temp': 'Ambient Temp.', 'settings_title': 'Settings', 'time_set': 'Time & Date', 'sync_pc': 'Sync Device', 'localization': 'Format & Language', 'time_format': 'Time Format', 'date_format_label': 'LCD Date Format', 'interface_lang': 'Language', 'save_btn': 'Save All', 'temp_sensor': 'Temperature', 'reading_live': 'Live Reading', 'force_read': 'Refresh' },
            'ES': { 'tab_overview': 'Resumen', 'tab_settings': 'Ajustes', 'tab_temp': 'Temp.', 'status_searching': 'Buscando...', 'status_connected': 'Conectado', 'status_error': 'Inaccesible', 'live_display': 'Pantalla en vivo', 'ambient_temp': 'Temp. ambiente', 'settings_title': 'Ajustes', 'time_set': 'Hora y Fecha', 'sync_pc': 'Sincronizar disp.', 'localization': 'Formato y Idioma', 'time_format': 'Formato de hora', 'date_format_label': 'Formato Fecha LCD', 'interface_lang': 'Idioma', 'save_btn': 'Guardar todo', 'temp_sensor': 'Sensor de Temp.', 'reading_live': 'Lectura en vivo', 'force_read': 'Actualizar' },
            'DE': { 'tab_overview': 'Übersicht', 'tab_settings': 'Einstellen', 'tab_temp': 'Temp.', 'status_searching': 'Suchen...', 'status_connected': 'Verbunden', 'status_error': 'Getrennt', 'live_display': 'Live-Anzeige', 'ambient_temp': 'Raumtemperatur', 'settings_title': 'Einstellungen', 'time_set': 'Uhrzeit & Datum', 'sync_pc': 'Gerät sync.', 'localization': 'Format & Sprache', 'time_format': 'Zeitformat', 'date_format_label': 'LCD Datumsformat', 'interface_lang': 'Sprache', 'save_btn': 'Speichern', 'temp_sensor': 'Temperatur', 'reading_live': 'Live-Lesung', 'force_read': 'Aktualisieren' }
        };
        let currentLang = 'FR';

        const applyTranslations = (lang) => {
            currentLang = lang; document.getElementById('input_lang').value = lang;
            document.querySelectorAll('[data-i18n]').forEach(el => {
                const key = el.getAttribute('data-i18n'); if (i18n[lang] && i18n[lang][key]) el.innerText = i18n[lang][key];
            });
        };

        document.querySelectorAll('.tab-btn, .mob-btn').forEach(btn => {
            btn.addEventListener('click', (e) => {
                const tabId = e.currentTarget.dataset.target;
                document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
                document.getElementById('view-' + tabId).classList.add('active');
                document.querySelectorAll('.tab-btn, .mob-btn').forEach(b => b.dataset.target === tabId ? b.classList.add('active') : b.classList.remove('active'));
            });
        });

        // ==========================================
        // GESTION DU REBOUCLAGE DES INPUTS (WRAP-AROUND)
        // ==========================================
        const wrapInput = (id, min, getMaxCallback) => {
            document.getElementById(id).addEventListener('change', function() {
                let v = parseInt(this.value);
                let max = typeof getMaxCallback === 'function' ? getMaxCallback() : getMaxCallback;
                if (isNaN(v)) return;
                
                if (v > max) this.value = String(min).padStart(2, '0');
                else if (v < min) this.value = String(max).padStart(2, '0');
                else this.value = String(v).padStart(2, '0');
            });
        };

        // Application du wrap sur Heure, Minute, Seconde et Mois
        wrapInput('input_heure', 0, 23);
        wrapInput('input_minute', 0, 59);
        wrapInput('input_seconde', 0, 59);
        wrapInput('input_mois', 1, 12);
        
        // Application du wrap sur le Jour (Logique dynamique selon le mois et l'année)
        wrapInput('input_jour', 1, () => {
            let m = parseInt(document.getElementById('input_mois').value) || 1;
            let y = parseInt(document.getElementById('input_annee').value) || new Date().getFullYear();
            return new Date(y, m, 0).getDate(); // Renvoie 28, 29, 30 ou 31 dynamiquement
        });
        
        // Recalculer le jour max si le mois change (ex: de Janvier 31 à Février -> ajuste le jour)
        document.getElementById('input_mois').addEventListener('change', function() {
            let jInput = document.getElementById('input_jour');
            let j = parseInt(jInput.value);
            let m = parseInt(this.value);
            let y = parseInt(document.getElementById('input_annee').value) || new Date().getFullYear();
            let max = new Date(y, m, 0).getDate();
            if (j > max) jInput.value = String(max).padStart(2, '0');
        });

        // ==========================================
        // COMMUNICATION AVEC L'ESP32
        // ==========================================
        setInterval(() => {
            fetch('/data').then(res => res.json()).then(data => {
                document.getElementById('status-container').className = "status connected";
                document.getElementById('conn-status').innerText = i18n[currentLang]['status_connected'];
                document.getElementById('preview-temp').innerText = parseFloat(data.temp).toFixed(1);
                document.getElementById('temp-large').innerText = parseFloat(data.temp).toFixed(1);
                
                let hDisp = data.heure, ampmStr = "";
                if (data.format12h) { ampmStr = (hDisp >= 12) ? "PM" : "AM"; hDisp = hDisp % 12; if(hDisp === 0) hDisp = 12; }
                
                document.getElementById('preview-h').innerText = String(hDisp).padStart(2, '0');
                document.getElementById('preview-m').innerText = String(data.minute).padStart(2, '0');
                document.getElementById('preview-s').innerText = String(data.seconde).padStart(2, '0');
                document.getElementById('preview-ampm').innerText = ampmStr;

                let d = new Date(data.annee, data.mois - 1, data.jour);
                const locale = currentLang === 'EN' ? 'en-US' : currentLang === 'ES' ? 'es-ES' : currentLang === 'DE' ? 'de-DE' : 'fr-FR';
                document.getElementById('preview-date').innerText = d.toLocaleDateString(locale, { weekday: 'long', year: 'numeric', month: 'long', day: 'numeric' });
                
                if(data.lang && data.lang !== currentLang) applyTranslations(data.lang);
                if(document.activeElement.id !== "input_format") document.getElementById('input_format').value = data.format12h ? "12" : "24";
                if(document.activeElement.id !== "input_date_format") document.getElementById('input_date_format').value = data.dateFmt;
            }).catch(() => {
                document.getElementById('status-container').className = "status error";
                document.getElementById('conn-status').innerText = i18n[currentLang]['status_error'];
            });
        }, 2000);

        const envoyerReglages = () => {
            let h = parseInt(document.getElementById('input_heure').value);
            let m = parseInt(document.getElementById('input_minute').value);
            let s = parseInt(document.getElementById('input_seconde').value);
            let jour = parseInt(document.getElementById('input_jour').value);
            let mois = parseInt(document.getElementById('input_mois').value);
            let annee = parseInt(document.getElementById('input_annee').value);
            let fmt = document.getElementById('input_format').value;
            let dFmt = document.getElementById('input_date_format').value;
            let l = document.getElementById('input_lang').value;
            
            if(isNaN(h) || isNaN(m) || isNaN(s) || isNaN(jour) || isNaN(mois) || isNaN(annee)) {
                showToast("Veuillez remplir toutes les cases !", true); return;
            }
            applyTranslations(l);
            fetch(`/set?heure=${h}&minute=${m}&seconde=${s}&jour=${jour}&mois=${mois}&annee=${annee}&format=${fmt}&dateFmt=${dFmt}&lang=${l}`)
                .then(() => showToast("Horloge mise à jour !"))
                .catch(() => showToast("Erreur de connexion", true));
        };

        const syncLocalTime = () => {
            let now = new Date();
            document.getElementById('input_heure').value = String(now.getHours()).padStart(2, '0');
            document.getElementById('input_minute').value = String(now.getMinutes()).padStart(2, '0');
            document.getElementById('input_seconde').value = String(now.getSeconds()).padStart(2, '0');
            document.getElementById('input_jour').value = String(now.getDate()).padStart(2, '0');
            document.getElementById('input_mois').value = String(now.getMonth() + 1).padStart(2, '0');
            document.getElementById('input_annee').value = now.getFullYear();
        };

        const showToast = (message, isError = false) => {
            const toast = document.getElementById('toastMsg');
            toast.querySelector('span').innerText = (isError ? '❌ ' : '✅ ') + message;
            toast.className = isError ? 'toast error show' : 'toast show';
            setTimeout(() => toast.className = 'toast', 3000);
        };

        document.getElementById('btn-sync').onclick = syncLocalTime;
        document.getElementById('btn-save').onclick = envoyerReglages;
        document.getElementById('btn-force').onclick = () => showToast("Actualisé !");

        applyTranslations('FR');
        syncLocalTime();
    </script>
</body>
</html>
)rawliteral";

// ==========================================
// FONCTIONS UTILITAIRES
// ==========================================
int getMaxDays(int year, int month) {
  if (month == 2) {
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) return 29;
    return 28;
  }
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

int calculateDayOfWeek(int d, int m, int y) {
    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    y -= m < 3;
    return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

String getDayStr(int dow, String lang) {
    const char* fr[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};
    const char* en[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    const char* es[] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
    const char* de[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
    if (lang == "EN") return en[dow];
    if (lang == "ES") return es[dow];
    if (lang == "DE") return de[dow];
    return fr[dow];
}

bool checkButton(int pin, bool &lastState) {
    bool currentState = digitalRead(pin);
    if (currentState == LOW && lastState == HIGH) {
        delay(30); 
        if (digitalRead(pin) == LOW) {
            lastState = LOW;
            return true;
        }
    } else if (currentState == HIGH && lastState == LOW) {
        lastState = HIGH;
    }
    return false;
}

void gererBoutons() {
    if (checkButton(BTN_MODE, lastBtnMode)) {
        if (currentMenu == NORMAL) {
            RtcDateTime now = Rtc.GetDateTime();
            editHour = now.Hour(); editMin = now.Minute(); editDay = now.Day(); editMonth = now.Month(); editYear = now.Year();
            currentMenu = SET_HOUR;
        } else if (currentMenu == SET_YEAR) {
            RtcDateTime newTime = RtcDateTime(editYear, editMonth, editDay, editHour, editMin, 0);
            Rtc.SetDateTime(newTime);
            currentMenu = NORMAL;
        } else {
            currentMenu = (MenuState)((int)currentMenu + 1);
        }
    }

    if (currentMenu != NORMAL) {
        if (checkButton(BTN_PLUS, lastBtnPlus)) {
            if (currentMenu == SET_HOUR) editHour = (editHour + 1) % 24;
            else if (currentMenu == SET_MIN) editMin = (editMin + 1) % 60;
            else if (currentMenu == SET_DAY) { editDay++; if (editDay > getMaxDays(editYear, editMonth)) editDay = 1; }
            else if (currentMenu == SET_MONTH) { editMonth++; if (editMonth > 12) editMonth = 1; }
            else if (currentMenu == SET_YEAR) { editYear++; if (editYear > 2099) editYear = 2024; }
        }
        if (checkButton(BTN_MINUS, lastBtnMin)) {
            if (currentMenu == SET_HOUR) editHour = (editHour - 1 < 0) ? 23 : editHour - 1;
            else if (currentMenu == SET_MIN) editMin = (editMin - 1 < 0) ? 59 : editMin - 1;
            else if (currentMenu == SET_DAY) { editDay--; if (editDay < 1) editDay = getMaxDays(editYear, editMonth); }
            else if (currentMenu == SET_MONTH) { editMonth--; if (editMonth < 1) editMonth = 12; }
            else if (currentMenu == SET_YEAR) { editYear--; if (editYear < 2024) editYear = 2099; }
        }
    }
}

void afficherEcran() {
  bool blink = (millis() / 500) % 2 == 0;
  RtcDateTime now = Rtc.GetDateTime();
  int dH = (currentMenu != NORMAL) ? editHour : now.Hour();
  int dM = (currentMenu != NORMAL) ? editMin : now.Minute();
  int dS = now.Second();
  int dDay = (currentMenu != NORMAL) ? editDay : now.Day();
  int dMonth = (currentMenu != NORMAL) ? editMonth : now.Month();
  int dYear = (currentMenu != NORMAL) ? editYear : now.Year();

  if (!now.IsValid() && currentMenu == NORMAL) {
    dH = 0; dM = 0; dS = 0; dDay = 1; dMonth = 1; dYear = 2024;
  }

  int dayOfWeek = calculateDayOfWeek(dDay, dMonth, dYear);
  String dayStr = getDayStr(dayOfWeek, currentLang);
  String ampm = ""; int dispH = dH;
  if (is12hFormat) {
    ampm = (dH >= 12) ? "PM" : "AM";
    if (dispH > 12) dispH -= 12;
    if (dispH == 0) dispH = 12;
  }

  char hStr[3], mStr[3], dStr[3], moStr[3], yStr[5];
  sprintf(hStr, "%02d", dispH); sprintf(mStr, "%02d", dM);
  sprintf(dStr, "%02d", dDay); sprintf(moStr, "%02d", dMonth); sprintf(yStr, "%02d", dYear % 100);

  if (currentMenu == SET_HOUR && blink) strcpy(hStr, " ");
  if (currentMenu == SET_MIN && blink) strcpy(mStr, " ");
  if (currentMenu == SET_DAY && blink) strcpy(dStr, " ");
  if (currentMenu == SET_MONTH && blink) strcpy(moStr, " ");
  if (currentMenu == SET_YEAR && blink) strcpy(yStr, " ");

  char dateFmtStr[12];
  if (dateFormat == 0) sprintf(dateFmtStr, "%s/%s/%s", dStr, moStr, yStr);
  else if (dateFormat == 1) sprintf(dateFmtStr, "%s/%s/%s", moStr, dStr, yStr);
  else sprintf(dateFmtStr, "%s/%s/%s", yStr, moStr, dStr);

  u8g2.clearBuffer();
  if (currentMenu == NORMAL) {
    u8g2.setFont(u8g2_font_helvB10_tr);
    u8g2.drawStr(2, 14, dayStr.c_str());
    char timeStr[6]; sprintf(timeStr, "%s:%s", hStr, mStr);
    u8g2.setFont(u8g2_font_logisoso16_tr); 
    u8g2.drawStr(5, 45, timeStr);
    char secStr[8];
    if (is12hFormat) sprintf(secStr, ":%02d %s", dS, ampm.c_str());
    else sprintf(secStr, ":%02d", dS);
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.drawStr(25, 60, secStr); 
    u8g2.drawHLine(5, 68, 54);
    u8g2.setFont(u8g2_font_helvB10_tr);
    u8g2.drawStr(2, 88, dateFmtStr);
    char tempStr[12];
    if (isnan(temperature)) strcpy(tempStr, "T: --.- C");
    else sprintf(tempStr, "T: %4.1f C", temperature);
    u8g2.drawStr(2, 115, tempStr);
  } else {
    u8g2.setFont(u8g2_font_helvB10_tr); u8g2.drawStr(2, 14, "REGLAGE");
    char timeStr[6]; sprintf(timeStr, "%s:%s", hStr, mStr);
    u8g2.setFont(u8g2_font_logisoso16_tr); u8g2.drawStr(5, 45, timeStr);
    u8g2.setFont(u8g2_font_helvB10_tr); u8g2.drawStr(2, 88, dateFmtStr);
    String step = "";
    if(currentMenu == SET_HOUR) step = "-> Heure";
    else if(currentMenu == SET_MIN) step = "-> Minute";
    else if(currentMenu == SET_DAY) step = "-> Jour";
    else if(currentMenu == SET_MONTH) step = "-> Mois";
    else if(currentMenu == SET_YEAR) step = "-> Annee";
    u8g2.setFont(u8g2_font_helvB08_tr); u8g2.drawStr(2, 115, step.c_str());
  }
  u8g2.sendBuffer(); 
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  pinMode(BTN_MODE, INPUT_PULLUP); pinMode(BTN_MINUS, INPUT_PULLUP); pinMode(BTN_PLUS, INPUT_PULLUP);

  // Initialisation du buzzer avec l'état de repos
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, BUZZER_OFF); 

  dht.begin();
  delay(1000); // Laisse le capteur DHT11 démarrer proprement
  
  Wire.begin(21, 22);
  Rtc.Begin();
  if (!Rtc.GetIsRunning()) Rtc.SetIsRunning(true);
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone); 

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB10_tr);
  u8g2.drawStr(0, 30, "Init AP WiFi");
  u8g2.sendBuffer();

  WiFi.softAP(ap_ssid, ap_password);
  IPAddress myIP = WiFi.softAPIP();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_helvB08_tr);
  u8g2.drawStr(0, 20, "AP Cree !");
  u8g2.drawStr(0, 40, "IP Locale:");
  u8g2.drawStr(0, 60, myIP.toString().c_str());
  u8g2.sendBuffer();
  delay(3000); 
  
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", index_html); });
  server.on("/data", HTTP_GET, []() {
    RtcDateTime now = Rtc.GetDateTime();
    String json = "{";
    json += "\"temp\":" + (isnan(temperature) ? "0.0" : String(temperature)) + ",";
    json += "\"heure\":" + String(now.Hour()) + ",\"minute\":" + String(now.Minute()) + ",\"seconde\":" + String(now.Second()) + ",";
    json += "\"jour\":" + String(now.Day()) + ",\"mois\":" + String(now.Month()) + ",\"annee\":" + String(now.Year()) + ",";
    json += "\"format12h\":" + String(is12hFormat ? "true" : "false") + ",\"dateFmt\":" + String(dateFormat) + ",\"lang\":\"" + currentLang + "\"}";
    server.send(200, "application/json", json);
  });
  server.on("/set", HTTP_GET, []() {
    if (server.hasArg("heure") && server.hasArg("minute") && server.hasArg("jour")) {
      int h = server.arg("heure").toInt(); int m = server.arg("minute").toInt(); int s = server.hasArg("seconde") ? server.arg("seconde").toInt() : 0;
      int d = server.arg("jour").toInt(); int mo = server.arg("mois").toInt(); int y = server.arg("annee").toInt();
      if(server.hasArg("lang")) currentLang = server.arg("lang");
      if(server.hasArg("format")) is12hFormat = (server.arg("format") == "12");
      if(server.hasArg("dateFmt")) dateFormat = server.arg("dateFmt").toInt();
      h = constrain(h, 0, 23); m = constrain(m, 0, 59); s = constrain(s, 0, 59); mo = constrain(mo, 1, 12); y = constrain(y, 2024, 2099); d = constrain(d, 1, getMaxDays(y, mo));
      RtcDateTime newTime = RtcDateTime(y, mo, d, h, m, s);
      Rtc.SetDateTime(newTime);
      server.send(200, "text/plain", "OK");
    } else server.send(400, "text/plain", "Parametres manquants");
  });
  server.begin();
}

// ==========================================
// BOUCLE PRINCIPALE
// ==========================================
void loop() {
  server.handleClient(); 
  gererBoutons();

  if (millis() - lastDHTRead > 2000) {
    temperature = dht.readTemperature();
    lastDHTRead = millis();
  }

  if (millis() - lastDisplayUpdate > 150) {
    afficherEcran();
    lastDisplayUpdate = millis();
  }

  if (currentMenu == NORMAL) {
    RtcDateTime now = Rtc.GetDateTime();
    if (now.IsValid() && now.Minute() == 0 && now.Second() == 0 && !hourBeeped) {
      digitalWrite(BUZZER_PIN, BUZZER_ON);
      buzzerTimer = millis();
      isBuzzing = true;
      hourBeeped = true;
    }
    if (isBuzzing && (millis() - buzzerTimer >= 500)) {
      digitalWrite(BUZZER_PIN, BUZZER_OFF);
      isBuzzing = false;
    }
    if (now.Second() > 0) hourBeeped = false;
  }
}
