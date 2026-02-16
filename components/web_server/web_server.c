#include "web_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "wifi_manager.h"
#include "ota_manager.h"
#include "sntp_sync.h"
#include "led_indicator.h"
#include <string.h>

static const char *TAG = "WEB_SERVER";

// HTTP server handle
static httpd_handle_t server = NULL;

// ============================================================================
// HTML PAGES
// ============================================================================

/**
 * HTML page for WiFi provisioning
 */
static const char *provisioning_html = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32-C6 Setup</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}"
".container{background:#fff;border-radius:16px;box-shadow:0 10px 40px rgba(0,0,0,0.2);max-width:420px;width:100%;padding:32px;animation:slideUp 0.4s ease}"
"@keyframes slideUp{from{opacity:0;transform:translateY(20px)}to{opacity:1;transform:translateY(0)}}"
"h1{color:#2d3748;font-size:28px;font-weight:700;text-align:center;margin-bottom:8px}"
".subtitle{color:#718096;text-align:center;font-size:14px;margin-bottom:24px}"

// Button styles
".btn{display:block;width:100%;padding:14px;border:none;border-radius:10px;font-size:15px;font-weight:600;cursor:pointer;transition:all 0.3s;text-decoration:none;text-align:center;margin-bottom:16px}"
".btn-primary{background:#667eea;color:#fff}"
".btn-primary:hover{background:#5568d3;transform:translateY(-2px);box-shadow:0 6px 20px rgba(102,126,234,0.4)}"
".btn-success{background:#48bb78;color:#fff}"
".btn-success:hover{background:#38a169;transform:translateY(-2px);box-shadow:0 6px 20px rgba(72,187,120,0.4)}"
".btn-secondary{background:#4299e1;color:#fff;font-size:14px;padding:10px}"
".btn-secondary:hover{background:#3182ce}"
".btn:disabled{background:#cbd5e0;cursor:not-allowed;transform:none}"

// Card styles
".card{background:#f7fafc;border-left:4px solid #4299e1;border-radius:8px;padding:16px;margin-bottom:20px}"
".card.success{background:#f0fff4;border-left-color:#48bb78}"
".card.warning{background:#fffaf0;border-left-color:#ed8936}"
".card-title{font-size:14px;font-weight:600;color:#2d3748;margin-bottom:12px}"

// Time display
".time-display{text-align:center;padding:4px 0}"
".time-value{font-size:32px;font-weight:700;color:#4299e1;font-family:'Courier New',monospace;letter-spacing:2px}"
".date-value{font-size:14px;color:#718096;margin-top:4px}"
".sync-badge{display:inline-block;margin-top:8px;padding:4px 12px;border-radius:12px;font-size:11px;font-weight:600}"
".sync-badge.synced{background:#c6f6d5;color:#22543d}"
".sync-badge.waiting{background:#feebc8;color:#7c2d12}"

// Info rows
".info-row{display:flex;justify-content:space-between;align-items:center;padding:8px 0;border-bottom:1px solid #e2e8f0;font-size:13px}"
".info-row:last-child{border-bottom:none}"
".info-label{color:#718096;font-weight:500}"
".info-value{color:#2d3748;font-family:'Courier New',monospace;font-weight:600;font-size:12px}"

// Form styles
".form-group{margin-bottom:20px}"
".form-label{display:block;color:#4a5568;font-size:13px;font-weight:600;margin-bottom:8px}"
".form-input{width:100%;padding:12px;border:2px solid #e2e8f0;border-radius:8px;font-size:14px;transition:all 0.3s}"
".form-input:focus{outline:none;border-color:#667eea;box-shadow:0 0 0 3px rgba(102,126,234,0.1)}"

// Status message
".status-msg{margin-top:16px;padding:12px;border-radius:8px;text-align:center;font-size:14px;font-weight:500;display:none}"
".status-msg.show{display:block;animation:slideDown 0.3s ease}"
".status-msg.success{background:#c6f6d5;color:#22543d}"
".status-msg.error{background:#fed7d7;color:#742a2a}"
"@keyframes slideDown{from{opacity:0;transform:translateY(-10px)}to{opacity:1;transform:translateY(0)}}"

// Divider
".divider{height:1px;background:linear-gradient(to right,transparent,#e2e8f0,transparent);margin:24px 0}"

"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>ESP32-C6 Setup</h1>"
"<p class='subtitle'>WiFi Configuration & Status</p>"

// OTA Update Button
"<a href='/ota' class='btn btn-primary'>🔄 OTA Firmware Update</a>"

// Time Card
"<div class='card'>"
"<div class='card-title'>Current Time (WIB)</div>"
"<div class='time-display'>"
"<div class='time-value' id='timeDisplay'>--:--:--</div>"
"<div class='date-value' id='dateDisplay'>--.--.----</div>"
"<span class='sync-badge waiting' id='syncBadge'>⏳ Syncing...</span>"
"</div>"
"</div>"

// Connection Status Card
"<div class='card' id='statusCard'>"
"<div class='card-title'>Connection Status</div>"
"<div id='statusContent'>Loading...</div>"
"</div>"

"<div class='divider'></div>"

// WiFi Form
"<form id='wifiForm'>"
"<div class='form-group'>"
"<label class='form-label'>WiFi SSID</label>"
"<input type='text' class='form-input' id='ssid' placeholder='Enter network name' required>"
"</div>"
"<div class='form-group'>"
"<label class='form-label'>WiFi Password</label>"
"<input type='password' class='form-input' id='password' placeholder='Enter password (optional)'>"
"</div>"
"<button type='submit' class='btn btn-success'>Connect to WiFi</button>"
"</form>"

"<button class='btn btn-secondary' onclick='refreshAll()'>🔄 Refresh Status</button>"

"<div class='status-msg' id='statusMsg'></div>"

"</div>"

"<script>"
// Update time
"function updateTime(){"
"fetch('/api/time').then(r=>r.json()).then(d=>{"
"const td=document.getElementById('timeDisplay');"
"const dd=document.getElementById('dateDisplay');"
"const sb=document.getElementById('syncBadge');"
"if(d.synced){"
"const p=d.time.split(' ');"
"if(p.length===2){td.textContent=p[1];dd.textContent=p[0];}"
"sb.textContent='✓ Synchronized';"
"sb.className='sync-badge synced';"
"}else{"
"td.textContent='--:--:--';"
"dd.textContent='Not synced';"
"sb.textContent='⏳ Syncing...';"
"sb.className='sync-badge waiting';"
"}}).catch(e=>console.error(e));}"

// Update status
"function updateStatus(){"
"fetch('/api/status').then(r=>r.json()).then(d=>{"
"const card=document.getElementById('statusCard');"
"const content=document.getElementById('statusContent');"
"if(d.connected){"
"card.className='card success';"
"content.innerHTML="
"'<div class=\"info-row\"><span class=\"info-label\">SSID</span><span class=\"info-value\">'+d.ssid+'</span></div>'"
"+'<div class=\"info-row\"><span class=\"info-label\">IP Address</span><span class=\"info-value\">'+d.ip+'</span></div>'"
"+'<div class=\"info-row\"><span class=\"info-label\">Gateway</span><span class=\"info-value\">'+d.gateway+'</span></div>'"
"+'<div class=\"info-row\"><span class=\"info-label\">AP IP</span><span class=\"info-value\">'+d.ap_ip+'</span></div>';"
"}else{"
"card.className='card warning';"
"content.innerHTML="
"'<div class=\"info-row\"><span class=\"info-label\">Mode</span><span class=\"info-value\">AP Only</span></div>'"
"+'<div class=\"info-row\"><span class=\"info-label\">AP IP</span><span class=\"info-value\">'+d.ap_ip+'</span></div>'"
"+'<div style=\"margin-top:12px;color:#744210;font-size:12px;text-align:center\">Configure WiFi below to connect</div>';"
"}}).catch(e=>console.error(e));}"

// Refresh all
"function refreshAll(){updateTime();updateStatus();}"

// Auto update
"setInterval(updateTime,1000);"
"refreshAll();"

// Form submit
"document.getElementById('wifiForm').addEventListener('submit',function(e){"
"e.preventDefault();"
"const msg=document.getElementById('statusMsg');"
"const btn=e.target.querySelector('button');"
"const ssid=document.getElementById('ssid').value;"
"const pwd=document.getElementById('password').value;"
"msg.className='status-msg';"
"msg.textContent='Connecting to '+ssid+'...';"
"msg.classList.add('show');"
"btn.disabled=true;"
"fetch('/api/wifi/save',{"
"method:'POST',"
"headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({ssid:ssid,password:pwd})"
"})"
".then(r=>r.json())"
".then(d=>{"
"if(d.success){"
"msg.className='status-msg success show';"
"msg.textContent='✓ WiFi saved! Device restarting in 3 seconds...';"
"setTimeout(()=>location.reload(),3000);"
"}else{"
"msg.className='status-msg error show';"
"msg.textContent='✗ Failed: '+(d.message||'Unknown error');"
"btn.disabled=false;"
"}"
"})"
".catch(e=>{"
"msg.className='status-msg error show';"
"msg.textContent='✗ Connection error: '+e;"
"btn.disabled=false;"
"});"
"});"
"</script>"
"</body>"
"</html>";

/**
 * HTML page for OTA update
 */
static const char *ota_html = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32-C6 OTA Update</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}"
".container{background:#fff;border-radius:16px;box-shadow:0 10px 40px rgba(0,0,0,0.2);max-width:520px;width:100%;padding:32px;animation:slideUp 0.4s ease}"
"@keyframes slideUp{from{opacity:0;transform:translateY(20px)}to{opacity:1;transform:translateY(0)}}"
"h1{color:#2d3748;font-size:28px;font-weight:700;text-align:center;margin-bottom:8px}"
".subtitle{color:#718096;text-align:center;font-size:14px;margin-bottom:24px}"

// Button
".btn{display:block;width:100%;padding:14px;border:none;border-radius:10px;font-size:15px;font-weight:600;cursor:pointer;transition:all 0.3s;text-decoration:none;text-align:center;margin-bottom:20px}"
".btn-back{background:#4299e1;color:#fff}"
".btn-back:hover{background:#3182ce;transform:translateY(-2px)}"
".btn-upload{background:#48bb78;color:#fff}"
".btn-upload:hover{background:#38a169;transform:translateY(-2px)}"
".btn:disabled{background:#cbd5e0;cursor:not-allowed;transform:none}"

// Info box
".info-box{background:#edf2f7;border-radius:10px;padding:20px;margin-bottom:24px}"
".info-row{display:flex;justify-content:space-between;padding:10px 0;border-bottom:1px solid #cbd5e0;font-size:14px}"
".info-row:last-child{border-bottom:none}"
".info-label{color:#4a5568;font-weight:500}"
".info-value{color:#2d3748;font-family:'Courier New',monospace;font-weight:700}"

// Upload area
".upload-area{border:3px dashed #cbd5e0;border-radius:12px;padding:48px 24px;text-align:center;cursor:pointer;transition:all 0.3s;margin:24px 0;background:#f7fafc}"
".upload-area:hover{border-color:#667eea;background:#edf2f7}"
".upload-area.drag-over{border-color:#48bb78;background:#f0fff4}"
".upload-icon{font-size:56px;margin-bottom:16px}"
".upload-text{color:#718096;font-size:15px;margin-bottom:8px}"
".upload-hint{color:#a0aec0;font-size:13px}"
".file-name{margin-top:16px;padding:12px;background:#fff;border-radius:8px;color:#2d3748;font-weight:600;font-size:14px}"
".file-input{display:none}"

// Progress
".progress-container{margin-top:24px;display:none}"
".progress-bar{width:100%;height:36px;background:#edf2f7;border-radius:18px;overflow:hidden;position:relative;margin-bottom:16px}"
".progress-fill{height:100%;background:linear-gradient(90deg,#48bb78,#38a169);transition:width 0.3s;position:relative}"
".progress-text{position:absolute;top:0;left:0;width:100%;height:100%;display:flex;align-items:center;justify-content:center;font-weight:700;color:#2d3748;font-size:15px}"
".progress-msg{padding:14px;border-radius:8px;text-align:center;font-weight:600;font-size:14px}"
".progress-msg.success{background:#c6f6d5;color:#22543d}"
".progress-msg.error{background:#fed7d7;color:#742a2a}"
".progress-msg.info{background:#bee3f8;color:#2c5282}"

// Warning
".warning-box{background:#fffaf0;border-left:4px solid #ed8936;border-radius:8px;padding:16px;margin-top:24px}"
".warning-title{color:#7c2d12;font-weight:700;font-size:14px;margin-bottom:8px}"
".warning-text{color:#744210;font-size:13px;margin:4px 0}"

"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>🔄 OTA Firmware Update</h1>"
"<p class='subtitle'>Upload new firmware (.bin file)</p>"

"<a href='/' class='btn btn-back'>← Back to WiFi Setup</a>"

// Info box
"<div class='info-box'>"
"<div class='info-row'><span class='info-label'>Current Version</span><span class='info-value' id='version'>...</span></div>"
"<div class='info-row'><span class='info-label'>Running Partition</span><span class='info-value' id='partition'>...</span></div>"
"<div class='info-row'><span class='info-label'>Free Space</span><span class='info-value' id='freeSpace'>...</span></div>"
"</div>"

// Upload area
"<div class='upload-area' id='uploadArea' onclick='document.getElementById(\"fileInput\").click()'>"
"<div class='upload-icon'>📁</div>"
"<div class='upload-text'>Click to select firmware file</div>"
"<div class='upload-hint'>or drag and drop .bin file here</div>"
"<div class='file-name' id='fileName' style='display:none'></div>"
"</div>"

"<input type='file' id='fileInput' class='file-input' accept='.bin'>"
"<button class='btn btn-upload' id='uploadBtn' onclick='uploadFirmware()' disabled>Upload Firmware</button>"

// Progress
"<div class='progress-container' id='progressContainer'>"
"<div class='progress-bar'>"
"<div class='progress-fill' id='progressFill' style='width:0%'></div>"
"<div class='progress-text' id='progressText'>0%</div>"
"</div>"
"<div class='progress-msg info' id='progressMsg'>Uploading...</div>"
"</div>"

// Warning
"<div class='warning-box'>"
"<div class='warning-title'>⚠️ Important Notes</div>"
"<div class='warning-text'>• Do not power off or disconnect during update</div>"
"<div class='warning-text'>• Device will restart automatically after update</div>"
"<div class='warning-text'>• Current configuration will be preserved</div>"
"</div>"

"</div>"

"<script>"
"let selectedFile=null;"

// Load info
"function loadInfo(){"
"fetch('/api/ota/info').then(r=>r.json()).then(d=>{"
"document.getElementById('version').textContent=d.version||'Unknown';"
"document.getElementById('partition').textContent=d.partition||'Unknown';"
"const freeMB=d.free_space?((d.free_space/1024/1024).toFixed(2)+' MB'):'Unknown';"  // FIX HERE
"document.getElementById('freeSpace').textContent=freeMB;"
"}).catch(e=>{console.error(e);document.getElementById('freeSpace').textContent='Error';});}"
"loadInfo();"

// Drag & drop
"const area=document.getElementById('uploadArea');"
"const fileInput=document.getElementById('fileInput');"
"area.addEventListener('dragover',e=>{e.preventDefault();area.classList.add('drag-over');});"
"area.addEventListener('dragleave',()=>area.classList.remove('drag-over'));"
"area.addEventListener('drop',e=>{"
"e.preventDefault();"
"area.classList.remove('drag-over');"
"if(e.dataTransfer.files.length>0)handleFile(e.dataTransfer.files[0]);"
"});"
"fileInput.addEventListener('change',e=>{if(e.target.files.length>0)handleFile(e.target.files[0]);});"

// Handle file
"function handleFile(file){"
"if(!file.name.endsWith('.bin')){alert('Please select a .bin file');return;}"
"selectedFile=file;"
"const fn=document.getElementById('fileName');"
"fn.textContent='📄 '+file.name+' ('+(file.size/1024/1024).toFixed(2)+' MB)';"
"fn.style.display='block';"
"document.getElementById('uploadBtn').disabled=false;"
"}"

// Upload
"function uploadFirmware(){"
"if(!selectedFile)return;"
"const formData=new FormData();"
"formData.append('file',selectedFile);"
"const btn=document.getElementById('uploadBtn');"
"const pc=document.getElementById('progressContainer');"
"const pf=document.getElementById('progressFill');"
"const pt=document.getElementById('progressText');"
"const pm=document.getElementById('progressMsg');"
"btn.disabled=true;"
"pc.style.display='block';"
"const xhr=new XMLHttpRequest();"
"xhr.upload.addEventListener('progress',e=>{"
"if(e.lengthComputable){"
"const pct=Math.round((e.loaded/e.total)*100);"
"pf.style.width=pct+'%';"
"pt.textContent=pct+'%';"
"}"
"});"
"xhr.addEventListener('load',()=>{"
"if(xhr.status===200){"
"try{"
"const resp=JSON.parse(xhr.responseText);"
"if(resp.success){"
"pm.className='progress-msg success';"
"pm.textContent='✓ Update successful! Device restarting in 3 seconds...';"
"setTimeout(()=>location.reload(),3000);"
"}else{"
"pm.className='progress-msg error';"
"pm.textContent='✗ Update failed: '+(resp.message||'Unknown error');"
"btn.disabled=false;"
"}"
"}catch(e){"
"pm.className='progress-msg error';"
"pm.textContent='✗ Invalid response from device';"
"btn.disabled=false;"
"}"
"}else{"
"pm.className='progress-msg error';"
"pm.textContent='✗ Upload error: HTTP '+xhr.status;"
"btn.disabled=false;"
"}"
"});"
"xhr.addEventListener('error',()=>{"
"pm.className='progress-msg error';"
"pm.textContent='✗ Network error occurred';"
"btn.disabled=false;"
"});"
"xhr.open('POST','/api/ota/update',true);"
"xhr.send(formData);"
"}"
"</script>"
"</body>"
"</html>";

// ============================================================================
// HTTP HANDLERS
// ============================================================================

/**
 * Root handler - provisioning page
 */
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, provisioning_html, strlen(provisioning_html));
    return ESP_OK;
}

/**
 * OTA page handler
 */
static esp_err_t ota_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, ota_html, strlen(ota_html));
    return ESP_OK;
}

/**
 * WiFi status API
 */
static esp_err_t api_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    wifi_state_t state = wifi_manager_get_state();
    bool is_connected = (state == WIFI_STATE_STA_CONNECTED);
    
    cJSON_AddBoolToObject(root, "connected", is_connected);
    cJSON_AddStringToObject(root, "ap_ip", WIFI_AP_IP);
    
    if (is_connected) {
        esp_netif_t *netif_sta = wifi_manager_get_sta_netif();
        if (netif_sta) {
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(netif_sta, &ip_info);
            
            char ip_str[16], subnet_str[16], gw_str[16];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            snprintf(subnet_str, sizeof(subnet_str), IPSTR, IP2STR(&ip_info.netmask));
            snprintf(gw_str, sizeof(gw_str), IPSTR, IP2STR(&ip_info.gw));
            
            cJSON_AddStringToObject(root, "ip", ip_str);
            cJSON_AddStringToObject(root, "subnet", subnet_str);
            cJSON_AddStringToObject(root, "gateway", gw_str);
            
            wifi_credentials_t creds;
            if (wifi_manager_load_credentials(&creds) == ESP_OK) {
                cJSON_AddStringToObject(root, "ssid", creds.ssid);
            }
        }
    }
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * Time API
 */
static esp_err_t api_time_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    struct tm timeinfo;
    bool time_valid = sntp_sync_get_time(&timeinfo);
    
    cJSON_AddBoolToObject(root, "synced", sntp_sync_is_synced());
    
    if (time_valid) {
        char time_str[100];
        strftime(time_str, sizeof(time_str), "%d.%m.%Y %H:%M:%S", &timeinfo);
        cJSON_AddStringToObject(root, "time", time_str);
        cJSON_AddNumberToObject(root, "year", timeinfo.tm_year + 1900);
        cJSON_AddNumberToObject(root, "month", timeinfo.tm_mon + 1);
        cJSON_AddNumberToObject(root, "day", timeinfo.tm_mday);
        cJSON_AddNumberToObject(root, "hour", timeinfo.tm_hour);
        cJSON_AddNumberToObject(root, "minute", timeinfo.tm_min);
        cJSON_AddNumberToObject(root, "second", timeinfo.tm_sec);
        cJSON_AddNumberToObject(root, "epoch", (double)sntp_sync_get_epoch());
    } else {
        cJSON_AddStringToObject(root, "time", "Not synchronized");
    }
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * WiFi save API
 */
static esp_err_t api_wifi_save_handler(httpd_req_t *req)
{
    char buf[200];
    int ret = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf)));
    
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    cJSON *password_json = cJSON_GetObjectItem(root, "password");
    
    if (!cJSON_IsString(ssid_json)) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }
    
    const char *ssid = ssid_json->valuestring;
    const char *password = cJSON_IsString(password_json) ? password_json->valuestring : "";
    
    esp_err_t err = wifi_manager_save_credentials(ssid, password);
    
    httpd_resp_set_type(req, "application/json");
    
    if (err == ESP_OK) {
        const char *resp = "{\"success\":true}";
        httpd_resp_send(req, resp, strlen(resp));
        
        cJSON_Delete(root);
        
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    } else {
        const char *resp = "{\"success\":false,\"message\":\"Save failed\"}";
        httpd_resp_send(req, resp, strlen(resp));
    }
    
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * OTA info API
 */
static esp_err_t api_ota_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddStringToObject(root, "version", ota_manager_get_version());
    cJSON_AddStringToObject(root, "partition", ota_manager_get_partition());
    
    const esp_partition_t *update_part = ota_manager_get_update_partition();
    if (update_part) {
        cJSON_AddNumberToObject(root, "free_space", update_part->size);
    }
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * OTA update API
 */
static esp_err_t api_ota_update_handler(httpd_req_t *req)
{
    char buf[512];
    int remaining = req->content_len;
    int received = 0;
    bool first_chunk = true;
    
    ESP_LOGI(TAG, "Starting OTA, size: %d bytes", remaining);
    
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        
        if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        } else if (recv_len <= 0) {
            ESP_LOGE(TAG, "Receive failed");
            ota_manager_abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload failed");
            return ESP_FAIL;
        }
        
        if (first_chunk) {
            if (ota_manager_begin(req->content_len) != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
                return ESP_FAIL;
            }
            first_chunk = false;
        }
        
        if (ota_manager_write(buf, recv_len) != ESP_OK) {
            ota_manager_abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
            return ESP_FAIL;
        }
        
        received += recv_len;
        remaining -= recv_len;
    }
    
    if (ota_manager_end() != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "OTA successful! Rebooting...");
    
    const char *resp = "{\"success\":true}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    
    led_set_system_status(LED_SYSTEM_CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    
    return ESP_OK;
}

// ============================================================================
// SERVER CONTROL
// ============================================================================

/**
 * Start web server
 */
esp_err_t web_server_start(void)
{
    if (server != NULL) {
        ESP_LOGW(TAG, "Web server already running");
        return ESP_OK;
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 15;
    
    ESP_LOGI(TAG, "Starting web server");
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Provisioning page
        httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = root_handler};
        httpd_register_uri_handler(server, &root_uri);
        
        // OTA page
        httpd_uri_t ota_uri = {.uri = "/ota", .method = HTTP_GET, .handler = ota_page_handler};
        httpd_register_uri_handler(server, &ota_uri);
        
        // APIs
        httpd_uri_t api_status = {.uri = "/api/status", .method = HTTP_GET, .handler = api_status_handler};
        httpd_register_uri_handler(server, &api_status);
        
        httpd_uri_t api_time = {.uri = "/api/time", .method = HTTP_GET, .handler = api_time_handler};
        httpd_register_uri_handler(server, &api_time);
        
        httpd_uri_t api_wifi_save = {.uri = "/api/wifi/save", .method = HTTP_POST, .handler = api_wifi_save_handler};
        httpd_register_uri_handler(server, &api_wifi_save);
        
        httpd_uri_t api_ota_info = {.uri = "/api/ota/info", .method = HTTP_GET, .handler = api_ota_info_handler};
        httpd_register_uri_handler(server, &api_ota_info);
        
        httpd_uri_t api_ota_update = {.uri = "/api/ota/update", .method = HTTP_POST, .handler = api_ota_update_handler};
        httpd_register_uri_handler(server, &api_ota_update);
        
        ESP_LOGI(TAG, "Web server started successfully");
        ESP_LOGI(TAG, "  Provisioning: http://192.168.4.1/");
        ESP_LOGI(TAG, "  OTA Update:   http://192.168.4.1/ota");
        
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start web server");
    return ESP_FAIL;
}

/**
 * Stop web server
 */
esp_err_t web_server_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
    return ESP_OK;
}

/**
 * Check if running
 */
bool web_server_is_running(void)
{
    return (server != NULL);
}