/* ═══════════════════════════════════════════════════════════════════════
 * VECTORD OS - Web Management Interface
 * HTTP server with embedded HTML dashboard
 * ═══════════════════════════════════════════════════════════════════════ */

#include "../include/vecord.h"

/* ═══════════════════════════════════════════════════════════════════════
 * HTTP SERVER STATE
 * ═══════════════════════════════════════════════════════════════════════ */

#define HTTP_PORT 80
#define HTTP_BUFFER_SIZE 8192
#define HTTP_MAX_REQUESTS 100

static int httpd_running = 0;
static int httpd_socket = -1;
static u32 httpd_request_count = 0;
static u32 httpd_bytes_served = 0;

/* Request structure */
typedef struct {
    char method[8];
    char path[256];
    char version[16];
    char headers[1024];
    char body[2048];
    int content_length;
} http_request_t;

/* Response structure */
typedef struct {
    int status_code;
    char status_text[32];
    char content_type[64];
    int content_length;
} http_response_t;

/* ═══════════════════════════════════════════════════════════════════════
 * HTML TEMPLATES - Embedded as C strings
 * ═══════════════════════════════════════════════════════════════════════ */

/* CSS Styles - Modern Dark Theme */
static const char *HTML_CSS =
    "<style>"
    "* { margin: 0; padding: 0; box-sizing: border-box; }"
    "body {"
        "font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;"
        "background: linear-gradient(135deg, #0d1117 0%, #161b22 100%);"
        "color: #e6edf3;"
        "min-height: 100vh;"
    "}"
    ".container { max-width: 1400px; margin: 0 auto; padding: 20px; }"
    "header {"
        "background: rgba(22, 27, 34, 0.95);"
        "border-bottom: 1px solid #30363d;"
        "padding: 16px 24px;"
        "display: flex;"
        "justify-content: space-between;"
        "align-items: center;"
        "backdrop-filter: blur(10px);"
    "}"
    ".logo {"
        "display: flex;"
        "align-items: center;"
        "gap: 12px;"
    "}"
    ".logo h1 {"
        "font-size: 1.5rem;"
        "background: linear-gradient(90deg, #58a6ff, #bc8cff);"
        "-webkit-background-clip: text;"
        "-webkit-text-fill-color: transparent;"
        "background-clip: text;"
    "}"
    ".logo-icon {"
        "width: 32px;"
        "height: 32px;"
        "background: linear-gradient(135deg, #58a6ff, #bc8cff);"
        "border-radius: 8px;"
        "display: flex;"
        "align-items: center;"
        "justify-content: center;"
        "font-weight: bold;"
        "font-size: 18px;"
    "}"
    "nav { display: flex; gap: 8px; }"
    "nav a {"
        "color: #8b949e;"
        "text-decoration: none;"
        "padding: 8px 16px;"
        "border-radius: 6px;"
        "transition: all 0.2s;"
        "font-weight: 500;"
    "}"
    "nav a:hover, nav a.active {"
        "color: #e6edf3;"
        "background: rgba(88, 166, 255, 0.1);"
    "}"
    ".status-badge {"
        "display: flex;"
        "align-items: center;"
        "gap: 8px;"
        "padding: 8px 16px;"
        "background: rgba(63, 185, 80, 0.1);"
        "border: 1px solid rgba(63, 185, 80, 0.3);"
        "border-radius: 20px;"
        "font-size: 0.875rem;"
    "}"
    ".status-dot {"
        "width: 8px;"
        "height: 8px;"
        "background: #3fb950;"
        "border-radius: 50%;"
        "animation: pulse 2s infinite;"
    "}"
    "@keyframes pulse {"
        "0%, 100% { opacity: 1; }"
        "50% { opacity: 0.5; }"
    "}"
    ".dashboard {"
        "display: grid;"
        "grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));"
        "gap: 20px;"
        "margin-top: 24px;"
    "}"
    ".card {"
        "background: rgba(22, 27, 34, 0.8);"
        "border: 1px solid #30363d;"
        "border-radius: 12px;"
        "padding: 20px;"
        "transition: transform 0.2s, box-shadow 0.2s;"
    "}"
    ".card:hover {"
        "transform: translateY(-2px);"
        "box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);"
    "}"
    ".card-header {"
        "display: flex;"
        "justify-content: space-between;"
        "align-items: center;"
        "margin-bottom: 16px;"
    "}"
    ".card-title {"
        "font-size: 0.875rem;"
        "color: #8b949e;"
        "text-transform: uppercase;"
        "letter-spacing: 0.5px;"
    "}"
    ".card-icon {"
        "width: 40px;"
        "height: 40px;"
        "border-radius: 10px;"
        "display: flex;"
        "align-items: center;"
        "justify-content: center;"
        "font-size: 20px;"
    "}"
    ".card-value {"
        "font-size: 2rem;"
        "font-weight: 600;"
        "margin-bottom: 8px;"
    "}"
    ".card-subtitle {"
        "color: #8b949e;"
        "font-size: 0.875rem;"
    "}"
    ".progress-bar {"
        "height: 8px;"
        "background: #21262d;"
        "border-radius: 4px;"
        "overflow: hidden;"
        "margin-top: 12px;"
    "}"
    ".progress-fill {"
        "height: 100%;"
        "border-radius: 4px;"
        "transition: width 0.5s ease;"
    "}"
    ".cpu-fill { background: linear-gradient(90deg, #58a6ff, #388bfd); }"
    ".mem-fill { background: linear-gradient(90deg, #3fb950, #2ea043); }"
    ".disk-fill { background: linear-gradient(90deg, #f0883e, #db6d28); }"
    ".icon-cpu { background: rgba(88, 166, 255, 0.15); color: #58a6ff; }"
    ".icon-mem { background: rgba(63, 185, 80, 0.15); color: #3fb950; }"
    ".icon-disk { background: rgba(240, 136, 62, 0.15); color: #f0883e; }"
    ".icon-net { background: rgba(188, 140, 255, 0.15); color: #bc8cff; }"
    ".icon-proc { background: rgba(255, 123, 114, 0.15); color: #ff7b72; }"
    "table {"
        "width: 100%;"
        "border-collapse: collapse;"
        "margin-top: 12px;"
    "}"
    "th, td {"
        "padding: 12px;"
        "text-align: left;"
        "border-bottom: 1px solid #21262d;"
    "}"
    "th {"
        "color: #8b949e;"
        "font-weight: 500;"
        "font-size: 0.875rem;"
        "text-transform: uppercase;"
    "}"
    "tr:hover { background: rgba(88, 166, 255, 0.05); }"
    ".btn {"
        "padding: 10px 20px;"
        "border: none;"
        "border-radius: 8px;"
        "cursor: pointer;"
        "font-weight: 500;"
        "transition: all 0.2s;"
        "display: inline-flex;"
        "align-items: center;"
        "gap: 8px;"
    "}"
    ".btn-primary {"
        "background: linear-gradient(135deg, #238636, #2ea043);"
        "color: white;"
    "}"
    ".btn-primary:hover { background: #2ea043; }"
    ".btn-danger {"
        "background: linear-gradient(135deg, #da3633, #f85149);"
        "color: white;"
    "}"
    ".btn-danger:hover { background: #f85149; }"
    ".btn-secondary {"
        "background: #21262d;"
        "color: #e6edf3;"
        "border: 1px solid #30363d;"
    "}"
    ".btn-secondary:hover { background: #30363d; }"
    ".actions {"
        "display: flex;"
        "gap: 12px;"
        "flex-wrap: wrap;"
        "margin-top: 16px;"
    "}"
    ".log-container {"
        "background: #0d1117;"
        "border: 1px solid #30363d;"
        "border-radius: 8px;"
        "padding: 16px;"
        "font-family: 'Cascadia Code', 'Fira Code', monospace;"
        "font-size: 0.875rem;"
        "max-height: 300px;"
        "overflow-y: auto;"
    "}"
    ".log-entry { padding: 4px 0; }"
    ".log-time { color: #8b949e; }"
    ".log-info { color: #58a6ff; }"
    ".log-warn { color: #d29922; }"
    ".log-error { color: #f85149; }"
    ".chart-container {"
        "position: relative;"
        "height: 200px;"
        "margin-top: 16px;"
    "}"
    "svg { width: 100%; height: 100%; }"
    ".chart-line {"
        "fill: none;"
        "stroke-width: 2;"
        "stroke-linecap: round;"
        "stroke-linejoin: round;"
    "}"
    ".chart-area { opacity: 0.2; }"
    ".grid-line { stroke: #21262d; stroke-width: 1; }"
    ".chart-label { fill: #8b949e; font-size: 10px; }"
    "footer {"
        "margin-top: 40px;"
        "padding: 20px;"
        "text-align: center;"
        "color: #8b949e;"
        "border-top: 1px solid #21262d;"
    "}"
    ".quick-actions {"
        "display: grid;"
        "grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));"
        "gap: 12px;"
    "}"
    ".quick-action {"
        "display: flex;"
        "flex-direction: column;"
        "align-items: center;"
        "padding: 20px;"
        "background: rgba(22, 27, 34, 0.6);"
        "border: 1px solid #30363d;"
        "border-radius: 12px;"
        "cursor: pointer;"
        "transition: all 0.2s;"
        "text-decoration: none;"
        "color: inherit;"
    "}"
    ".quick-action:hover {"
        "background: rgba(88, 166, 255, 0.1);"
        "border-color: #58a6ff;"
    "}"
    ".quick-action-icon { font-size: 24px; margin-bottom: 8px; }"
    ".quick-action-label { font-size: 0.875rem; color: #8b949e; }"
    ".full-width { grid-column: 1 / -1; }"
    "</style>";

/* Navigation Header */
static const char *HTML_HEADER =
    "<header>"
        "<div class='logo'>"
            "<div class='logo-icon'>V</div>"
            "<h1>VECTORD OS</h1>"
        "</div>"
        "<nav>"
            "<a href='/' class='active'>Dashboard</a>"
            "<a href='/system'>System</a>"
            "<a href='/network'>Network</a>"
            "<a href='/files'>Files</a>"
            "<a href='/settings'>Settings</a>"
        "</nav>"
        "<div class='status-badge'>"
            "<div class='status-dot'></div>"
            "<span>System Online</span>"
        "</div>"
    "</header>";

/* Dashboard Page Template */
static const char *HTML_DASHBOARD =
    "<div class='container'>"
        "%HEADER%"
        "<div class='dashboard'>"
            "<!-- CPU Card -->"
            "<div class='card'>"
                "<div class='card-header'>"
                    "<span class='card-title'>CPU Usage</span>"
                    "<div class='card-icon icon-cpu'>%</div>"
                "</div>"
                "<div class='card-value' id='cpu-value'>%CPU%%</div>"
                "<div class='card-subtitle'>%CPU_NAME%</div>"
                "<div class='progress-bar'>"
                    "<div class='progress-fill cpu-fill' id='cpu-bar' style='width: %CPU%%'></div>"
                "</div>"
            "</div>"
            "<!-- Memory Card -->"
            "<div class='card'>"
                "<div class='card-header'>"
                    "<span class='card-title'>Memory Usage</span>"
                    "<div class='card-icon icon-mem'>M</div>"
                "</div>"
                "<div class='card-value' id='mem-value'>%MEM_USED% MB</div>"
                "<div class='card-subtitle'>of %MEM_TOTAL% MB total</div>"
                "<div class='progress-bar'>"
                    "<div class='progress-fill mem-fill' id='mem-bar' style='width: %MEM%%'></div>"
                "</div>"
            "</div>"
            "<!-- Disk Card -->"
            "<div class='card'>"
                "<div class='card-header'>"
                    "<span class='card-title'>Disk Usage</span>"
                    "<div class='card-icon icon-disk'>D</div>"
                "</div>"
                "<div class='card-value' id='disk-value'>%DISK_USED% MB</div>"
                "<div class='card-subtitle'>of %DISK_TOTAL% MB total</div>"
                "<div class='progress-bar'>"
                    "<div class='progress-fill disk-fill' id='disk-bar' style='width: %DISK%%'></div>"
                "</div>"
            "</div>"
            "<!-- Network Card -->"
            "<div class='card'>"
                "<div class='card-header'>"
                    "<span class='card-title'>Network</span>"
                    "<div class='card-icon icon-net'>N</div>"
                "</div>"
                "<div class='card-value'>%NET_IF%</div>"
                "<div class='card-subtitle'>IP: %NET_IP%</div>"
                "<div style='margin-top: 12px; display: flex; gap: 16px;'>"
                    "<span><small>TX:</small> %NET_TX% KB</span>"
                    "<span><small>RX:</small> %NET_RX% KB</span>"
                "</div>"
            "</div>"
            "<!-- CPU History Chart -->"
            "<div class='card'>"
                "<div class='card-header'>"
                    "<span class='card-title'>CPU History</span>"
                "</div>"
                "<div class='chart-container'>"
                    "<svg viewBox='0 0 400 200'>"
                        "<line class='grid-line' x1='0' y1='50' x2='400' y2='50'/>"
                        "<line class='grid-line' x1='0' y1='100' x2='400' y2='100'/>"
                        "<line class='grid-line' x1='0' y1='150' x2='400' y2='150'/>"
                        "<text class='chart-label' x='5' y='55'>75%</text>"
                        "<text class='chart-label' x='5' y='105'>50%</text>"
                        "<text class='chart-label' x='5' y='155'>25%</text>"
                        "<path class='chart-area' d='M0,180 L40,160 L80,140 L120,150 L160,130 L200,145 L240,120 L280,140 L320,110 L360,125 L400,100 L400,200 L0,200 Z' fill='%2358a6ff'/>"
                        "<path class='chart-line' d='M0,180 L40,160 L80,140 L120,150 L160,130 L200,145 L240,120 L280,140 L320,110 L360,125 L400,100' stroke='%2358a6ff'/>"
                    "</svg>"
                "</div>"
            "</div>"
            "<!-- Processes Card -->"
            "<div class='card'>"
                "<div class='card-header'>"
                    "<span class='card-title'>Running Processes</span>"
                    "<div class='card-icon icon-proc'>P</div>"
                "</div>"
                "<table>"
                    "<thead><tr><th>PID</th><th>Name</th><th>State</th><th>CPU%</th></tr></thead>"
                    "<tbody>"
                        "<tr><td>1</td><td>kernel</td><td>Running</td><td>2%</td></tr>"
                        "<tr><td>2</td><td>init</td><td>Running</td><td>1%</td></tr>"
                        "<tr><td>3</td><td>shell</td><td>Running</td><td>5%</td></tr>"
                        "<tr><td>4</td><td>webserver</td><td>Running</td><td>3%</td></tr>"
                        "<tr><td>5</td><td>network</td><td>Sleeping</td><td>0%</td></tr>"
                        "<tr><td>6</td><td>idle</td><td>Running</td><td>89%</td></tr>"
                    "</tbody>"
                "</table>"
            "</div>"
            "<!-- Quick Actions -->"
            "<div class='card full-width'>"
                "<div class='card-header'>"
                    "<span class='card-title'>Quick Actions</span>"
                "</div>"
                "<div class='quick-actions'>"
                    "<a href='/api/command?cmd=reboot' class='quick-action'>"
                        "<span class='quick-action-icon'>&#x21bb;</span>"
                        "<span class='quick-action-label'>Reboot</span>"
                    "</a>"
                    "<a href='/api/command?cmd=shutdown' class='quick-action'>"
                        "<span class='quick-action-icon'>&#x23FB;</span>"
                        "<span class='quick-action-label'>Shutdown</span>"
                    "</a>"
                    "<a href='/api/command?cmd=clear_logs' class='quick-action'>"
                        "<span class='quick-action-icon'>&#x1F5D1;</span>"
                        "<span class='quick-action-label'>Clear Logs</span>"
                    "</a>"
                    "<a href='/system' class='quick-action'>"
                        "<span class='quick-action-icon'>&#x2139;</span>"
                        "<span class='quick-action-label'>System Info</span>"
                    "</a>"
                    "<a href='/files' class='quick-action'>"
                        "<span class='quick-action-icon'>&#x1F4C1;</span>"
                        "<span class='quick-action-label'>File Browser</span>"
                    "</a>"
                    "<a href='/settings' class='quick-action'>"
                        "<span class='quick-action-icon'>&#x2699;</span>"
                        "<span class='quick-action-label'>Settings</span>"
                    "</a>"
                "</div>"
            "</div>"
            "<!-- System Logs -->"
            "<div class='card full-width'>"
                "<div class='card-header'>"
                    "<span class='card-title'>System Logs</span>"
                "</div>"
                "<div class='log-container' id='logs'>"
                    "<div class='log-entry'><span class='log-time'>[00:00:01]</span> <span class='log-info'>[BOOT]</span> Kernel initialized</div>"
                    "<div class='log-entry'><span class='log-time'>[00:00:02]</span> <span class='log-info'>[BOOT]</span> Memory manager started</div>"
                    "<div class='log-entry'><span class='log-time'>[00:00:03]</span> <span class='log-info'>[BOOT]</span> Network stack initialized</div>"
                    "<div class='log-entry'><span class='log-time'>[00:00:04]</span> <span class='log-info'>[BOOT]</span> Web server started on port 80</div>"
                    "<div class='log-entry'><span class='log-time'>[00:00:05]</span> <span class='log-info'>[INFO]</span> System ready</div>"
                "</div>"
            "</div>"
        "</div>"
        "<footer>"
            "<p>VECTORD OS %VERSION% | Uptime: %UPTIME% | Web Server Active</p>"
        "</footer>"
    "</div>"
    "<script>"
        "function refreshStatus() {"
            "fetch('/api/status')"
                ".then(r => r.json())"
                ".then(data => {"
                    "document.getElementById('cpu-value').textContent = data.cpu + '%';"
                    "document.getElementById('cpu-bar').style.width = data.cpu + '%';"
                    "document.getElementById('mem-value').textContent = data.mem_used + ' MB';"
                    "document.getElementById('mem-bar').style.width = data.mem_percent + '%';"
                "})"
                ".catch(e => console.log('Status update failed'));"
        "}"
        "setInterval(refreshStatus, 5000);"
    "</script>";

/* System Info Page */
static const char *HTML_SYSTEM =
    "<div class='container'>"
        "%HEADER%"
        "<div class='dashboard'>"
            "<div class='card full-width'>"
                "<div class='card-header'><span class='card-title'>Operating System</span></div>"
                "<table>"
                    "<tr><td><strong>Name</strong></td><td>VECTORD OS</td></tr>"
                    "<tr><td><strong>Version</strong></td><td>%VERSION%</td></tr>"
                    "<tr><td><strong>Codename</strong></td><td>The Thinking OS</td></tr>"
                    "<tr><td><strong>Architecture</strong></td><td>i386 (x86)</td></tr>"
                    "<tr><td><strong>Kernel Type</strong></td><td>Standalone Microkernel</td></tr>"
                "</table>"
            "</div>"
            "<div class='card'>"
                "<div class='card-header'><span class='card-title'>CPU Information</span></div>"
                "<table>"
                    "<tr><td><strong>Vendor</strong></td><td>%CPU_VENDOR%</td></tr>"
                    "<tr><td><strong>Brand</strong></td><td>%CPU_NAME%</td></tr>"
                    "<tr><td><strong>Family</strong></td><td>%CPU_FAMILY%</td></tr>"
                    "<tr><td><strong>Model</strong></td><td>%CPU_MODEL%</td></tr>"
                    "<tr><td><strong>Speed</strong></td><td>%CPU_SPEED% MHz</td></tr>"
                "</table>"
            "</div>"
            "<div class='card'>"
                "<div class='card-header'><span class='card-title'>Memory Information</span></div>"
                "<table>"
                    "<tr><td><strong>Total RAM</strong></td><td>%MEM_TOTAL% MB</td></tr>"
                    "<tr><td><strong>Used</strong></td><td>%MEM_USED% MB</td></tr>"
                    "<tr><td><strong>Free</strong></td><td>%MEM_FREE% MB</td></tr>"
                    "<tr><td><strong>Heap Blocks</strong></td><td>%MEM_BLOCKS%</td></tr>"
                "</table>"
            "</div>"
            "<div class='card'>"
                "<div class='card-header'><span class='card-title'>Storage</span></div>"
                "<table>"
                    "<tr><td><strong>ATA Devices</strong></td><td>%ATA_COUNT%</td></tr>"
                    "%ATA_LIST%"
                    "<tr><td><strong>AHCI Controllers</strong></td><td>%AHCI_COUNT%</td></tr>"
                "</table>"
            "</div>"
            "<div class='card'>"
                "<div class='card-header'><span class='card-title'>Hardware</span></div>"
                "<table>"
                    "<tr><td><strong>PCI Devices</strong></td><td>%PCI_COUNT%</td></tr>"
                    "<tr><td><strong>USB Controllers</strong></td><td>%USB_COUNT%</td></tr>"
                    "<tr><td><strong>Framebuffer</strong></td><td>%FB_STATUS%</td></tr>"
                "</table>"
            "</div>"
        "</div>"
    "</div>";

/* Network Page */
static const char *HTML_NETWORK =
    "<div class='container'>"
        "%HEADER%"
        "<div class='dashboard'>"
            "<div class='card full-width'>"
                "<div class='card-header'><span class='card-title'>Network Interfaces</span></div>"
                "<table>"
                    "<thead><tr><th>Interface</th><th>Status</th><th>IP Address</th><th>Netmask</th><th>Gateway</th><th>MAC</th></tr></thead>"
                    "<tbody>"
                        "<tr><td>eth0</td><td><span style='color: #3fb950;'>UP</span></td><td>%ETH0_IP%</td><td>%ETH0_NETMASK%</td><td>%ETH0_GW%</td><td>%ETH0_MAC%</td></tr>"
                        "<tr><td>lo</td><td><span style='color: #3fb950;'>UP</span></td><td>127.0.0.1</td><td>255.0.0.0</td><td>-</td><td>00:00:00:00:00:00</td></tr>"
                    "</tbody>"
                "</table>"
            "</div>"
            "<div class='card'>"
                "<div class='card-header'><span class='card-title'>Traffic Statistics</span></div>"
                "<table>"
                    "<tr><td><strong>Packets TX</strong></td><td>%PKT_TX%</td></tr>"
                    "<tr><td><strong>Packets RX</strong></td><td>%PKT_RX%</td></tr>"
                    "<tr><td><strong>Bytes TX</strong></td><td>%BYTES_TX% KB</td></tr>"
                    "<tr><td><strong>Bytes RX</strong></td><td>%BYTES_RX% KB</td></tr>"
                    "<tr><td><strong>Errors</strong></td><td>%NET_ERRORS%</td></tr>"
                "</table>"
            "</div>"
            "<div class='card'>"
                "<div class='card-header'><span class='card-title'>Connections</span></div>"
                "<table>"
                    "<thead><tr><th>Protocol</th><th>Local</th><th>Remote</th><th>State</th></tr></thead>"
                    "<tbody>"
                        "<tr><td>TCP</td><td>0.0.0.0:80</td><td>0.0.0.0:0</td><td>LISTEN</td></tr>"
                        "<tr><td>UDP</td><td>0.0.0.0:53</td><td>0.0.0.0:0</td><td>-</td></tr>"
                    "</tbody>"
                "</table>"
            "</div>"
        "</div>"
    "</div>";

/* Files Page */
static const char *HTML_FILES =
    "<div class='container'>"
        "%HEADER%"
        "<div class='dashboard'>"
            "<div class='card full-width'>"
                "<div class='card-header'><span class='card-title'>File Browser</span></div>"
                "<div style='margin-bottom: 16px;'>"
                    "<span style='color: #8b949e;'>Path: </span>"
                    "<span id='current-path'>/</span>"
                "</div>"
                "<table>"
                    "<thead><tr><th>Type</th><th>Name</th><th>Size</th><th>Modified</th><th>Permissions</th></tr></thead>"
                    "<tbody id='file-list'>"
                        "<tr><td>&#x1F4C1;</td><td><a href='/files?path=/boot' style='color: #58a6ff;'>boot</a></td><td>-</td><td>2026-01-15</td><td>drwxr-xr-x</td></tr>"
                        "<tr><td>&#x1F4C1;</td><td><a href='/files?path=/etc' style='color: #58a6ff;'>etc</a></td><td>-</td><td>2026-01-15</td><td>drwxr-xr-x</td></tr>"
                        "<tr><td>&#x1F4C1;</td><td><a href='/files?path=/home' style='color: #58a6ff;'>home</a></td><td>-</td><td>2026-01-15</td><td>drwxr-xr-x</td></tr>"
                        "<tr><td>&#x1F4C1;</td><td><a href='/files?path=/bin' style='color: #58a6ff;'>bin</a></td><td>-</td><td>2026-01-15</td><td>drwxr-xr-x</td></tr>"
                        "<tr><td>&#x1F4C1;</td><td><a href='/files?path=/var' style='color: #58a6ff;'>var</a></td><td>-</td><td>2026-01-15</td><td>drwxr-xr-x</td></tr>"
                        "<tr><td>&#x1F4C4;</td><td>readme.txt</td><td>256 B</td><td>2026-01-15</td><td>-rw-r--r--</td></tr>"
                        "<tr><td>&#x1F4C4;</td><td>system.cfg</td><td>128 B</td><td>2026-01-15</td><td>-rw-r--r--</td></tr>"
                    "</tbody>"
                "</table>"
            "</div>"
        "</div>"
    "</div>";

/* Settings Page */
static const char *HTML_SETTINGS =
    "<div class='container'>"
        "%HEADER%"
        "<div class='dashboard'>"
            "<div class='card'>"
                "<div class='card-header'><span class='card-title'>System Settings</span></div>"
                "<div style='margin-bottom: 16px;'>"
                    "<label style='display: block; margin-bottom: 8px; color: #8b949e;'>Hostname</label>"
                    "<input type='text' value='vectord-os' style='width: 100%; padding: 10px; background: #21262d; border: 1px solid #30363d; border-radius: 6px; color: #e6edf3;'>"
                "</div>"
                "<div style='margin-bottom: 16px;'>"
                    "<label style='display: block; margin-bottom: 8px; color: #8b949e;'>Timezone</label>"
                    "<select style='width: 100%; padding: 10px; background: #21262d; border: 1px solid #30363d; border-radius: 6px; color: #e6edf3;'>"
                        "<option>UTC</option>"
                        "<option>America/New_York</option>"
                        "<option>Europe/London</option>"
                        "<option>Asia/Tokyo</option>"
                    "</select>"
                "</div>"
                "<div class='actions'>"
                    "<button class='btn btn-primary'>Save Settings</button>"
                "</div>"
            "</div>"
            "<div class='card'>"
                "<div class='card-header'><span class='card-title'>Network Settings</span></div>"
                "<div style='margin-bottom: 16px;'>"
                    "<label style='display: block; margin-bottom: 8px; color: #8b949e;'>DHCP</label>"
                    "<select style='width: 100%; padding: 10px; background: #21262d; border: 1px solid #30363d; border-radius: 6px; color: #e6edf3;'>"
                        "<option>Enabled</option>"
                        "<option>Disabled</option>"
                    "</select>"
                "</div>"
                "<div style='margin-bottom: 16px;'>"
                    "<label style='display: block; margin-bottom: 8px; color: #8b949e;'>IP Address</label>"
                    "<input type='text' value='%ETH0_IP%' style='width: 100%; padding: 10px; background: #21262d; border: 1px solid #30363d; border-radius: 6px; color: #e6edf3;'>"
                "</div>"
                "<div class='actions'>"
                    "<button class='btn btn-primary'>Apply</button>"
                    "<button class='btn btn-secondary'>Reset</button>"
                "</div>"
            "</div>"
            "<div class='card full-width'>"
                "<div class='card-header'><span class='card-title'>Shell Command Executor</span></div>"
                "<div style='margin-bottom: 16px;'>"
                    "<input type='text' id='shell-cmd' placeholder='Enter command...' style='width: 100%; padding: 10px; background: #21262d; border: 1px solid #30363d; border-radius: 6px; color: #e6edf3;'>"
                "</div>"
                "<div class='log-container' id='shell-output' style='min-height: 100px;'>Command output will appear here...</div>"
                "<div class='actions' style='margin-top: 12px;'>"
                    "<button class='btn btn-primary' onclick='execCommand()'>Execute</button>"
                    "<button class='btn btn-secondary' onclick='clearOutput()'>Clear</button>"
                "</div>"
            "</div>"
        "</div>"
    "</div>"
    "<script>"
        "function execCommand() {"
            "var cmd = document.getElementById('shell-cmd').value;"
            "if (!cmd) return;"
            "fetch('/api/command', {"
                "method: 'POST',"
                "headers: {'Content-Type': 'application/json'},"
                "body: JSON.stringify({command: cmd})"
            "})"
            ".then(r => r.text())"
            ".then(out => {"
                "document.getElementById('shell-output').innerHTML = '<div class=\"log-entry\">' + out + '</div>';"
            "});"
        "}"
        "function clearOutput() {"
            "document.getElementById('shell-output').innerHTML = '';"
            "document.getElementById('shell-cmd').value = '';"
        "}"
    "</script>";

/* JSON Status API Response */
static const char *JSON_STATUS_TEMPLATE =
    "{"
        "\"status\": \"running\","
        "\"uptime\": %UPTIME%,"
        "\"cpu\": %CPU%,"
        "\"cpu_name\": \"%CPU_NAME%\","
        "\"mem_total\": %MEM_TOTAL%,"
        "\"mem_used\": %MEM_USED%,"
        "\"mem_free\": %MEM_FREE%,"
        "\"mem_percent\": %MEM%,"
        "\"disk_total\": %DISK_TOTAL%,"
        "\"disk_used\": %DISK_USED%,"
        "\"disk_percent\": %DISK%,"
        "\"net_ip\": \"%NET_IP%\","
        "\"net_if\": \"%NET_IF%\","
        "\"processes\": %PROC_COUNT%,"
        "\"requests\": %REQUESTS%,"
        "\"version\": \"%VERSION%\""
    "}";

/* ═══════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

static void httpd_itoa(int n, char *buf) {
    int i = 0;
    int neg = 0;
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    if (n < 0) { neg = 1; n = -n; }
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    if (neg) buf[i++] = '-';
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
    buf[i] = '\0';
}

static void httpd_strcpy(char *dest, const char *src) {
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

static int httpd_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

static int httpd_strncmp(const char *a, const char *b, int n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return *(unsigned char*)a - *(unsigned char*)b;
}

static char *httpd_strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char*)haystack;
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
        haystack++;
    }
    return NULL;
}

static void httpd_send_str(int sockfd, const char *str) {
    int len = 0;
    while (str[len]) len++;
    socket_send(sockfd, (u8*)str, len);
}

static int httpd_strlen(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void httpd_send_status(int sockfd, int code, const char *text) {
    char buf[128];
    httpd_strcpy(buf, "HTTP/1.1 ");
    char num[12];
    httpd_itoa(code, num);
    httpd_strcpy(buf + 9, num);
    httpd_strcpy(buf + 9 + httpd_strlen(num), " ");
    httpd_strcpy(buf + 10 + httpd_strlen(num), text);
    httpd_strcpy(buf + 10 + httpd_strlen(num) + httpd_strlen(text), "\r\n");
    httpd_send_str(sockfd, buf);
}

static void httpd_send_header(int sockfd, const char *name, const char *value) {
    httpd_send_str(sockfd, name);
    httpd_send_str(sockfd, ": ");
    httpd_send_str(sockfd, value);
    httpd_send_str(sockfd, "\r\n");
}

static void httpd_send_content_type(int sockfd, const char *type) {
    httpd_send_header(sockfd, "Content-Type", type);
}

static void httpd_send_content_length(int sockfd, int len) {
    char num[12];
    httpd_itoa(len, num);
    httpd_send_header(sockfd, "Content-Length", num);
}

static void httpd_finish_headers(int sockfd) {
    httpd_send_str(sockfd, "\r\n");
}

static void httpd_replace_placeholder(char *buf, int bufsize, const char *placeholder, const char *value) {
    char *pos = httpd_strstr(buf, placeholder);
    if (!pos) return;
    
    int ph_len = 0;
    while (placeholder[ph_len]) ph_len++;
    
    int val_len = 0;
    while (value[val_len]) val_len++;
    
    int remaining = httpd_strlen(pos + ph_len);
    int new_len = httpd_strlen(buf) - ph_len + val_len;
    
    if (new_len >= bufsize) return; /* Would overflow */
    
    /* Shift remaining content */
    char *src = pos + ph_len;
    char *dst = pos + val_len;
    while (*src) *dst++ = *src++;
    *dst = '\0';
    
    /* Copy new value */
    for (int i = 0; i < val_len; i++) {
        pos[i] = value[i];
    }
}

static void httpd_generate_status_json(char *buf, int bufsize) {
    char num[16];
    
    /* Simple simulated values */
    int cpu = 42;
    int mem_total = hw_info.ram_total_mb ? hw_info.ram_total_mb : 64;
    int mem_used = mem_get_used() / (1024 * 1024);
    int mem_free = mem_get_free() / (1024 * 1024);
    int mem_percent = mem_total > 0 ? (mem_used * 100 / mem_total) : 15;
    
    httpd_strcpy(buf, JSON_STATUS_TEMPLATE);
    
    httpd_itoa(httpd_request_count, num);
    httpd_replace_placeholder(buf, bufsize, "%REQUESTS%", num);
    
    httpd_itoa(cpu, num);
    httpd_replace_placeholder(buf, bufsize, "%CPU%", num);
    
    httpd_itoa(mem_total, num);
    httpd_replace_placeholder(buf, bufsize, "%MEM_TOTAL%", num);
    
    httpd_itoa(mem_used, num);
    httpd_replace_placeholder(buf, bufsize, "%MEM_USED%", num);
    
    httpd_itoa(mem_free, num);
    httpd_replace_placeholder(buf, bufsize, "%MEM_FREE%", num);
    
    httpd_itoa(mem_percent, num);
    httpd_replace_placeholder(buf, bufsize, "%MEM%", num);
    
    httpd_itoa(512, num);  /* disk total */
    httpd_replace_placeholder(buf, bufsize, "%DISK_TOTAL%", num);
    
    httpd_itoa(128, num);  /* disk used */
    httpd_replace_placeholder(buf, bufsize, "%DISK_USED%", num);
    
    httpd_itoa(25, num);   /* disk percent */
    httpd_replace_placeholder(buf, bufsize, "%DISK%", num);
    
    httpd_itoa(6, num);    /* processes */
    httpd_replace_placeholder(buf, bufsize, "%PROC_COUNT%", num);
    
    httpd_itoa(0, num);    /* uptime */
    httpd_replace_placeholder(buf, bufsize, "%UPTIME%", num);
    
    httpd_replace_placeholder(buf, bufsize, "%CPU_NAME%", hw_info.cpu_brand[0] ? hw_info.cpu_brand : "VECTORD CPU");
    httpd_replace_placeholder(buf, bufsize, "%NET_IP%", "192.168.1.100");
    httpd_replace_placeholder(buf, bufsize, "%NET_IF%", "eth0");
    httpd_replace_placeholder(buf, bufsize, "%VERSION%", OS_VERSION);
}

/* ═══════════════════════════════════════════════════════════════════════
 * REQUEST PARSING
 * ═══════════════════════════════════════════════════════════════════════ */

static int httpd_parse_request(const char *raw, http_request_t *req) {
    int i = 0;
    
    /* Parse method */
    int method_len = 0;
    while (raw[i] && raw[i] != ' ' && method_len < 7) {
        req->method[method_len++] = raw[i++];
    }
    req->method[method_len] = '\0';
    
    /* Skip space */
    if (raw[i] == ' ') i++;
    
    /* Parse path */
    int path_len = 0;
    while (raw[i] && raw[i] != ' ' && path_len < 255) {
        req->path[path_len++] = raw[i++];
    }
    req->path[path_len] = '\0';
    
    /* Skip space */
    if (raw[i] == ' ') i++;
    
    /* Parse version */
    int version_len = 0;
    while (raw[i] && raw[i] != '\r' && raw[i] != '\n' && version_len < 15) {
        req->version[version_len++] = raw[i++];
    }
    req->version[version_len] = '\0';
    
    /* Find end of headers */
    const char *body_start = httpd_strstr(raw, "\r\n\r\n");
    if (body_start) {
        int headers_len = body_start - raw;
        if (headers_len < 1024) {
            for (int j = 0; j < headers_len; j++) {
                req->headers[j] = raw[j];
            }
            req->headers[headers_len] = '\0';
        }
        body_start += 4;
        httpd_strcpy(req->body, body_start);
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * ROUTE HANDLERS
 * ═══════════════════════════════════════════════════════════════════════ */

static void httpd_handle_dashboard(int sockfd) {
    char page[16384];
    char num[16];

    int mem_total = hw_info.ram_total_mb ? hw_info.ram_total_mb : 64;
    int mem_used = mem_get_used() / (1024 * 1024);
    int mem_percent = mem_total > 0 ? (mem_used * 100 / mem_total) : 15;
    int cpu = 42;
    
    /* Build page */
    httpd_strcpy(page, "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>VECTORD OS - Dashboard</title>");
    httpd_strcpy(page + httpd_strlen(page), HTML_CSS);
    httpd_strcpy(page + httpd_strlen(page), "</head><body>");
    httpd_strcpy(page + httpd_strlen(page), HTML_DASHBOARD);
    httpd_strcpy(page + httpd_strlen(page), "</body></html>");
    
    /* Replace placeholders */
    httpd_replace_placeholder(page, sizeof(page), "%HEADER%", HTML_HEADER);
    
    httpd_itoa(cpu, num);
    httpd_replace_placeholder(page, sizeof(page), "%CPU%", num);
    httpd_replace_placeholder(page, sizeof(page), "%CPU_NAME%", hw_info.cpu_brand[0] ? hw_info.cpu_brand : "VECTORD CPU");
    
    httpd_itoa(mem_used, num);
    httpd_replace_placeholder(page, sizeof(page), "%MEM_USED%", num);
    
    httpd_itoa(mem_total, num);
    httpd_replace_placeholder(page, sizeof(page), "%MEM_TOTAL%", num);
    
    httpd_itoa(mem_percent, num);
    httpd_replace_placeholder(page, sizeof(page), "%MEM%", num);
    
    httpd_itoa(128, num);
    httpd_replace_placeholder(page, sizeof(page), "%DISK_USED%", num);
    
    httpd_itoa(512, num);
    httpd_replace_placeholder(page, sizeof(page), "%DISK_TOTAL%", num);
    
    httpd_itoa(25, num);
    httpd_replace_placeholder(page, sizeof(page), "%DISK%", num);
    
    httpd_replace_placeholder(page, sizeof(page), "%NET_IF%", "eth0");
    httpd_replace_placeholder(page, sizeof(page), "%NET_IP%", "192.168.1.100");
    httpd_itoa(1024, num);
    httpd_replace_placeholder(page, sizeof(page), "%NET_TX%", num);
    httpd_itoa(2048, num);
    httpd_replace_placeholder(page, sizeof(page), "%NET_RX%", num);
    httpd_replace_placeholder(page, sizeof(page), "%VERSION%", OS_VERSION);
    httpd_itoa(0, num);
    httpd_replace_placeholder(page, sizeof(page), "%UPTIME%", num);
    
    /* Send response */
    int page_len = httpd_strlen(page);
    httpd_send_status(sockfd, 200, "OK");
    httpd_send_content_type(sockfd, "text/html; charset=utf-8");
    httpd_send_content_length(sockfd, page_len);
    httpd_finish_headers(sockfd);
    httpd_send_str(sockfd, page);
}

static void httpd_handle_system(int sockfd) {
    char page[16384];
    char num[16];
    
    httpd_strcpy(page, "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>VECTORD OS - System</title>");
    httpd_strcpy(page + httpd_strlen(page), HTML_CSS);
    httpd_strcpy(page + httpd_strlen(page), "</head><body>");
    httpd_strcpy(page + httpd_strlen(page), HTML_SYSTEM);
    httpd_strcpy(page + httpd_strlen(page), "</body></html>");
    
    httpd_replace_placeholder(page, sizeof(page), "%HEADER%", HTML_HEADER);
    httpd_replace_placeholder(page, sizeof(page), "%VERSION%", OS_VERSION);
    httpd_replace_placeholder(page, sizeof(page), "%CPU_VENDOR%", hw_info.cpu_vendor[0] ? hw_info.cpu_vendor : "VECTORD");
    httpd_replace_placeholder(page, sizeof(page), "%CPU_NAME%", hw_info.cpu_brand[0] ? hw_info.cpu_brand : "VECTORD CPU");
    httpd_itoa(hw_info.cpu_family, num);
    httpd_replace_placeholder(page, sizeof(page), "%CPU_FAMILY%", num);
    httpd_itoa(hw_info.cpu_model, num);
    httpd_replace_placeholder(page, sizeof(page), "%CPU_MODEL%", num);
    httpd_itoa(hw_info.cpu_speed_mhz ? hw_info.cpu_speed_mhz : 2000, num);
    httpd_replace_placeholder(page, sizeof(page), "%CPU_SPEED%", num);
    
    int mem_total = hw_info.ram_total_mb ? hw_info.ram_total_mb : 64;
    int mem_used = mem_get_used() / (1024 * 1024);
    int mem_free = mem_get_free() / (1024 * 1024);
    httpd_itoa(mem_total, num);
    httpd_replace_placeholder(page, sizeof(page), "%MEM_TOTAL%", num);
    httpd_itoa(mem_used, num);
    httpd_replace_placeholder(page, sizeof(page), "%MEM_USED%", num);
    httpd_itoa(mem_free, num);
    httpd_replace_placeholder(page, sizeof(page), "%MEM_FREE%", num);
    httpd_itoa(mem_get_free_blocks(), num);
    httpd_replace_placeholder(page, sizeof(page), "%MEM_BLOCKS%", num);
    
    httpd_itoa(ata_device_count, num);
    httpd_replace_placeholder(page, sizeof(page), "%ATA_COUNT%", num);
    httpd_replace_placeholder(page, sizeof(page), "%ATA_LIST%", "");
    httpd_itoa(ahci_controller_count, num);
    httpd_replace_placeholder(page, sizeof(page), "%AHCI_COUNT%", num);
    httpd_itoa(pci_device_count, num);
    httpd_replace_placeholder(page, sizeof(page), "%PCI_COUNT%", num);
    httpd_itoa(usb_controller_count, num);
    httpd_replace_placeholder(page, sizeof(page), "%USB_COUNT%", num);
    httpd_replace_placeholder(page, sizeof(page), "%FB_STATUS%", vbe_info.present ? "Available" : "Not initialized");
    
    int page_len = httpd_strlen(page);
    httpd_send_status(sockfd, 200, "OK");
    httpd_send_content_type(sockfd, "text/html; charset=utf-8");
    httpd_send_content_length(sockfd, page_len);
    httpd_finish_headers(sockfd);
    httpd_send_str(sockfd, page);
}

static void httpd_handle_network(int sockfd) {
    char page[16384];
    
    httpd_strcpy(page, "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>VECTORD OS - Network</title>");
    httpd_strcpy(page + httpd_strlen(page), HTML_CSS);
    httpd_strcpy(page + httpd_strlen(page), "</head><body>");
    httpd_strcpy(page + httpd_strlen(page), HTML_NETWORK);
    httpd_strcpy(page + httpd_strlen(page), "</body></html>");
    
    httpd_replace_placeholder(page, sizeof(page), "%HEADER%", HTML_HEADER);
    httpd_replace_placeholder(page, sizeof(page), "%ETH0_IP%", "192.168.1.100");
    httpd_replace_placeholder(page, sizeof(page), "%ETH0_NETMASK%", "255.255.255.0");
    httpd_replace_placeholder(page, sizeof(page), "%ETH0_GW%", "192.168.1.1");
    httpd_replace_placeholder(page, sizeof(page), "%ETH0_MAC%", "AA:BB:CC:DD:EE:FF");
    httpd_replace_placeholder(page, sizeof(page), "%PKT_TX%", "1234");
    httpd_replace_placeholder(page, sizeof(page), "%PKT_RX%", "5678");
    httpd_replace_placeholder(page, sizeof(page), "%BYTES_TX%", "1024");
    httpd_replace_placeholder(page, sizeof(page), "%BYTES_RX%", "2048");
    httpd_replace_placeholder(page, sizeof(page), "%NET_ERRORS%", "0");
    
    int page_len = httpd_strlen(page);
    httpd_send_status(sockfd, 200, "OK");
    httpd_send_content_type(sockfd, "text/html; charset=utf-8");
    httpd_send_content_length(sockfd, page_len);
    httpd_finish_headers(sockfd);
    httpd_send_str(sockfd, page);
}

static void httpd_handle_files(int sockfd) {
    char page[16384];
    
    httpd_strcpy(page, "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>VECTORD OS - Files</title>");
    httpd_strcpy(page + httpd_strlen(page), HTML_CSS);
    httpd_strcpy(page + httpd_strlen(page), "</head><body>");
    httpd_strcpy(page + httpd_strlen(page), HTML_FILES);
    httpd_strcpy(page + httpd_strlen(page), "</body></html>");
    
    httpd_replace_placeholder(page, sizeof(page), "%HEADER%", HTML_HEADER);
    
    int page_len = httpd_strlen(page);
    httpd_send_status(sockfd, 200, "OK");
    httpd_send_content_type(sockfd, "text/html; charset=utf-8");
    httpd_send_content_length(sockfd, page_len);
    httpd_finish_headers(sockfd);
    httpd_send_str(sockfd, page);
}

static void httpd_handle_settings(int sockfd) {
    char page[16384];
    
    httpd_strcpy(page, "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>VECTORD OS - Settings</title>");
    httpd_strcpy(page + httpd_strlen(page), HTML_CSS);
    httpd_strcpy(page + httpd_strlen(page), "</head><body>");
    httpd_strcpy(page + httpd_strlen(page), HTML_SETTINGS);
    httpd_strcpy(page + httpd_strlen(page), "</body></html>");
    
    httpd_replace_placeholder(page, sizeof(page), "%HEADER%", HTML_HEADER);
    httpd_replace_placeholder(page, sizeof(page), "%ETH0_IP%", "192.168.1.100");
    
    int page_len = httpd_strlen(page);
    httpd_send_status(sockfd, 200, "OK");
    httpd_send_content_type(sockfd, "text/html; charset=utf-8");
    httpd_send_content_length(sockfd, page_len);
    httpd_finish_headers(sockfd);
    httpd_send_str(sockfd, page);
}

static void httpd_handle_api_status(int sockfd) {
    char json[2048];
    httpd_generate_status_json(json, sizeof(json));
    
    int json_len = httpd_strlen(json);
    httpd_send_status(sockfd, 200, "OK");
    httpd_send_content_type(sockfd, "application/json");
    httpd_send_content_length(sockfd, json_len);
    httpd_finish_headers(sockfd);
    httpd_send_str(sockfd, json);
}

static void httpd_handle_api_command(int sockfd, const http_request_t *req) {
    char result[1024];
    char cmd[256];
    cmd[0] = '\0';
    
    /* Extract command from query string or body */
    const char *q = httpd_strstr(req->path, "cmd=");
    if (q) {
        q += 4;
        int i = 0;
        while (q[i] && q[i] != ' ' && q[i] != '&' && i < 255) {
            cmd[i] = q[i];
            i++;
        }
        cmd[i] = '\0';
    }
    
    /* Execute command */
    if (httpd_strcmp(cmd, "reboot") == 0) {
        httpd_strcpy(result, "System reboot initiated...");
    } else if (httpd_strcmp(cmd, "shutdown") == 0) {
        httpd_strcpy(result, "System shutdown initiated...");
    } else if (httpd_strcmp(cmd, "clear_logs") == 0) {
        httpd_strcpy(result, "Logs cleared.");
    } else {
        httpd_strcpy(result, "Command executed: ");
        httpd_strcpy(result + httpd_strlen(result), cmd);
    }
    
    int result_len = httpd_strlen(result);
    httpd_send_status(sockfd, 200, "OK");
    httpd_send_content_type(sockfd, "text/plain");
    httpd_send_content_length(sockfd, result_len);
    httpd_finish_headers(sockfd);
    httpd_send_str(sockfd, result);
}

/* ═══════════════════════════════════════════════════════════════════════
 * HTTP SERVER MAIN HANDLER
 * ═══════════════════════════════════════════════════════════════════════ */

static void httpd_handle_request(int client_sock, http_request_t *req) {
    httpd_request_count++;
    
    serial_puts("[HTTP] ");
    serial_puts(req->method);
    serial_puts(" ");
    serial_puts(req->path);
    serial_puts("\n");
    
    /* Route handling */
    if (httpd_strcmp(req->path, "/") == 0) {
        httpd_handle_dashboard(client_sock);
    } else if (httpd_strcmp(req->path, "/system") == 0) {
        httpd_handle_system(client_sock);
    } else if (httpd_strcmp(req->path, "/network") == 0) {
        httpd_handle_network(client_sock);
    } else if (httpd_strncmp(req->path, "/files", 6) == 0) {
        httpd_handle_files(client_sock);
    } else if (httpd_strcmp(req->path, "/settings") == 0) {
        httpd_handle_settings(client_sock);
    } else if (httpd_strcmp(req->path, "/api/status") == 0) {
        httpd_handle_api_status(client_sock);
    } else if (httpd_strncmp(req->path, "/api/command", 12) == 0) {
        httpd_handle_api_command(client_sock, req);
    } else {
        /* 404 Not Found */
        const char *not_found = "<html><body style='background:#0d1117;color:#e6edf3;padding:40px;'><h1>404 - Not Found</h1><p><a href='/'>Return to Dashboard</a></p></body></html>";
        int len = httpd_strlen(not_found);
        httpd_send_status(client_sock, 404, "Not Found");
        httpd_send_content_type(client_sock, "text/html");
        httpd_send_content_length(client_sock, len);
        httpd_finish_headers(client_sock);
        httpd_send_str(client_sock, not_found);
    }
    
    httpd_bytes_served += 1024; /* Approximate */
}

/* ═══════════════════════════════════════════════════════════════════════
 * HTTP SERVER API
 * ═══════════════════════════════════════════════════════════════════════ */

void httpd_init(void) {
    httpd_running = 0;
    httpd_socket = -1;
    httpd_request_count = 0;
    httpd_bytes_served = 0;
    
    serial_puts("[HTTPD] Web server module initialized\n");
}

void httpd_start(void) {
    if (httpd_running) {
        serial_puts("[HTTPD] Server already running\n");
        return;
    }
    
    /* Create TCP socket */
    httpd_socket = socket_create(SOCKET_TCP);
    if (httpd_socket < 0) {
        serial_puts("[HTTPD] Failed to create socket\n");
        return;
    }
    
    /* Bind to port 80 */
    if (socket_bind(httpd_socket, HTTP_PORT) != 0) {
        serial_puts("[HTTPD] Failed to bind to port ");
        serial_putdec(HTTP_PORT);
        serial_puts("\n");
        socket_close(httpd_socket);
        return;
    }
    
    /* Listen for connections */
    if (socket_listen(httpd_socket) != 0) {
        serial_puts("[HTTPD] Failed to listen\n");
        socket_close(httpd_socket);
        return;
    }
    
    httpd_running = 1;
    serial_puts("[HTTPD] Web server started on port ");
    serial_putdec(HTTP_PORT);
    serial_puts("\n");
    serial_puts("[HTTPD] Access at: http://localhost/\n");
}

void httpd_stop(void) {
    if (!httpd_running) {
        serial_puts("[HTTPD] Server not running\n");
        return;
    }
    
    if (httpd_socket >= 0) {
        socket_close(httpd_socket);
        httpd_socket = -1;
    }
    
    httpd_running = 0;
    serial_puts("[HTTPD] Web server stopped\n");
}

void httpd_handler(void) {
    if (!httpd_running || httpd_socket < 0) {
        return;
    }
    
    /* Accept connection (non-blocking check) */
    int client = socket_accept(httpd_socket);
    if (client < 0) {
        return; /* No pending connections */
    }
    
    /* Receive request */
    char buffer[HTTP_BUFFER_SIZE];
    u16 buflen = sizeof(buffer) - 1;
    int received = socket_receive(client, (u8*)buffer, &buflen);
    
    if (received > 0) {
        buffer[received] = '\0';
        
        /* Parse request */
        http_request_t req;
        httpd_parse_request(buffer, &req);
        
        /* Handle request */
        httpd_handle_request(client, &req);
    }
    
    /* Close client connection */
    socket_close(client);
}

void httpd_status(void) {
    serial_puts("Web Server Status:\n");
    serial_puts("  State:    ");
    serial_puts(httpd_running ? "Running" : "Stopped");
    serial_puts("\n");
    serial_puts("  Port:     ");
    serial_putdec(HTTP_PORT);
    serial_puts("\n");
    serial_puts("  Requests: ");
    serial_putdec(httpd_request_count);
    serial_puts("\n");
    serial_puts("  URL:      http://localhost/\n");
}

int httpd_is_running(void) {
    return httpd_running;
}

/* ═══════════════════════════════════════════════════════════════════════
 * SHELL COMMAND: webserver
 * ═══════════════════════════════════════════════════════════════════════ */

void cmd_webserver(const char *arg) {
    if (!arg || arg[0] == '\0') {
        httpd_status();
        return;
    }
    
    if (httpd_strcmp(arg, "start") == 0) {
        httpd_start();
    } else if (httpd_strcmp(arg, "stop") == 0) {
        httpd_stop();
    } else if (httpd_strcmp(arg, "status") == 0) {
        httpd_status();
    } else {
        serial_puts("Usage: webserver [start|stop|status]\n");
    }
}
