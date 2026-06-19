#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ESP8266httpUpdate.h>

const char* ssid = "MERCUSYS_5E58";
const char* password = "94306750";
const char* webhookUrl = "http://webhook.site/97732e3d-c284-4fe3-ad06-f3024ccbca3c";
const char* otaHostname = "esp32";
const char* mdnsHostname = "esp32";

const char* fwVersionUrl = "https://github.com/zohaib-fastn/esp-testing/releases/latest/download/version.txt";
const char* fwBinaryUrl = "https://github.com/zohaib-fastn/esp-testing/releases/latest/download/firmware.bin";
const unsigned long updateCheckInterval = 5 * 60 * 1000; // 5 minutes
unsigned long lastUpdateCheck = 0;

#define LED_PIN 2

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
bool doorLocked = true;

const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Door Lock</title>
  <style>
    :root {
      --bg1: #0f0c29; --bg2: #302b63; --bg3: #24243e;
      --card-bg: rgba(255,255,255,0.06); --card-border: rgba(255,255,255,0.12);
      --text: #f0e6ff; --text-dim: #a78bfa; --text-muted: #6d5a9e;
      --cyan: #06d6a0; --cyan-glow: rgba(6,214,160,0.3);
      --orange: #f72585; --orange-glow: rgba(247,37,133,0.3);
      --green: #06d6a0; --red: #ef476f; --purple: #7209b7;
    }
    [data-theme="light"] {
      --bg1: #faf5ff; --bg2: #ede9fe; --bg3: #ddd6fe;
      --card-bg: rgba(255,255,255,0.85); --card-border: rgba(114,9,183,0.15);
      --text: #1e1b4b; --text-dim: #6d28d9; --text-muted: #a78bfa;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      background: linear-gradient(135deg, var(--bg1), var(--bg2), var(--bg3));
      color: var(--text);
      min-height: 100vh;
      display: flex; justify-content: center; align-items: center;
      transition: background 0.5s, color 0.3s;
    }
    .container { width: 100%; max-width: 480px; padding: 16px; }

    .card {
      background: var(--card-bg); backdrop-filter: blur(12px);
      border: 1px solid var(--card-border); border-radius: 24px;
      padding: 32px 28px; text-align: center;
      box-shadow: 0 8px 32px rgba(0,0,0,0.3);
      margin-bottom: 16px; transition: background 0.3s, border-color 0.3s;
    }

    .top-bar { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
    .theme-btn {
      background: none; border: 1px solid var(--card-border); border-radius: 50%;
      width: 36px; height: 36px; font-size: 18px; cursor: pointer;
      display: flex; align-items: center; justify-content: center;
      transition: transform 0.3s, border-color 0.3s;
    }
    .theme-btn:hover { transform: rotate(30deg); }
    .wifi-signal { display: flex; align-items: center; gap: 6px; font-size: 12px; color: var(--green); }
    .wifi-dot { width: 8px; height: 8px; background: var(--green); border-radius: 50%; animation: pulse 2s infinite; }

    @keyframes pulse { 0%,100% { opacity: 1; box-shadow: 0 0 4px var(--green); } 50% { opacity: 0.4; box-shadow: none; } }
    @keyframes lockBounce { 0% { transform: scale(1); } 30% { transform: scale(1.3) rotate(-10deg); } 60% { transform: scale(0.9) rotate(5deg); } 100% { transform: scale(1) rotate(0); } }
    @keyframes slideIn { from { opacity: 0; transform: translateY(-12px); } to { opacity: 1; transform: translateY(0); } }
    @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
    @keyframes barGrow { from { width: 0; } }
    @keyframes shake { 0%,100% { transform: translateX(0); } 20% { transform: translateX(-6px); } 40% { transform: translateX(6px); } 60% { transform: translateX(-4px); } 80% { transform: translateX(4px); } }
    @keyframes spin { to { transform: rotate(360deg); } }

    .lock-icon {
      font-size: 72px; display: block; margin-bottom: 12px;
      transition: transform 0.4s; cursor: default;
    }
    .lock-icon.animate { animation: lockBounce 0.5s ease; }

    h1 { font-size: 24px; font-weight: 700; }
    .subtitle { color: var(--text-dim); font-size: 13px; margin-bottom: 24px; }

    .status-bar {
      display: flex; align-items: center; justify-content: center; gap: 10px;
      padding: 14px 24px; border-radius: 14px; margin-bottom: 24px;
      font-size: 17px; font-weight: 600; transition: all 0.4s ease;
    }
    .status-locked { background: rgba(6,214,160,0.12); border: 1px solid rgba(6,214,160,0.25); color: var(--cyan); }
    .status-unlocked { background: rgba(247,37,133,0.12); border: 1px solid rgba(247,37,133,0.25); color: var(--orange); }
    .dot { width: 10px; height: 10px; border-radius: 50%; transition: all 0.3s; }
    .dot.locked { background: var(--cyan); box-shadow: 0 0 10px var(--cyan); }
    .dot.unlocked { background: var(--orange); box-shadow: 0 0 10px var(--orange); }

    .btn-group { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 16px; }
    .btn {
      padding: 14px 8px; font-size: 15px; border: none; border-radius: 14px;
      cursor: pointer; color: #fff; font-weight: 700; transition: transform 0.15s, box-shadow 0.2s, opacity 0.2s;
      position: relative; overflow: hidden;
    }
    .btn:active { transform: scale(0.95); }
    .btn:disabled { opacity: 0.5; cursor: not-allowed; transform: none; }
    .btn-unlock { background: linear-gradient(135deg, #f72585, #b5179e); box-shadow: 0 4px 15px rgba(247,37,133,0.3); }
    .btn-lock { background: linear-gradient(135deg, #06d6a0, #05a67a); box-shadow: 0 4px 15px rgba(6,214,160,0.3); }
    .btn-full { grid-column: 1 / -1; }
    .btn-ota { background: linear-gradient(135deg, #7209b7, #560bad); box-shadow: 0 4px 15px rgba(114,9,183,0.3); font-size: 13px; padding: 11px; }
    .btn-danger { background: linear-gradient(135deg, #ef476f, #d62246); box-shadow: 0 4px 15px rgba(239,71,111,0.3); font-size: 13px; padding: 11px; }
    .btn .spinner { display: none; width: 16px; height: 16px; border: 2px solid rgba(255,255,255,0.3); border-top-color: #fff; border-radius: 50%; animation: spin 0.6s linear infinite; margin: 0 auto; }
    .btn.loading .label { display: none; }
    .btn.loading .spinner { display: inline-block; }
    .btn::after {
      content: ''; position: absolute; top: 50%; left: 50%;
      width: 0; height: 0; background: rgba(255,255,255,0.2);
      border-radius: 50%; transform: translate(-50%,-50%);
      transition: width 0.4s, height 0.4s, opacity 0.4s;
      opacity: 0;
    }
    .btn:active::after { width: 200px; height: 200px; opacity: 1; transition: 0s; }

    .tabs { display: flex; gap: 4px; margin-bottom: 16px; background: rgba(0,0,0,0.2); border-radius: 12px; padding: 4px; }
    .tab {
      flex: 1; padding: 8px; font-size: 12px; font-weight: 600; border: none;
      background: transparent; color: var(--text-dim); border-radius: 10px;
      cursor: pointer; transition: all 0.3s;
    }
    .tab.active { background: var(--card-bg); color: var(--text); box-shadow: 0 2px 8px rgba(0,0,0,0.2); }
    .tab-content { display: none; animation: fadeIn 0.3s ease; }
    .tab-content.active { display: block; }

    .stats-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 12px; }
    .stat-card {
      background: rgba(0,0,0,0.15); border-radius: 14px; padding: 14px 10px;
      text-align: center; border: 1px solid rgba(255,255,255,0.05);
    }
    .stat-value { font-size: 22px; font-weight: 700; margin-bottom: 2px; }
    .stat-label { font-size: 10px; color: var(--text-dim); text-transform: uppercase; letter-spacing: 1px; }
    .stat-value.cyan { color: var(--cyan); }
    .stat-value.orange { color: var(--orange); }
    .stat-value.green { color: var(--green); }
    .stat-value.purple { color: var(--purple); }

    .chart-container { background: rgba(0,0,0,0.15); border-radius: 14px; padding: 14px; margin-bottom: 12px; border: 1px solid rgba(255,255,255,0.05); }
    .chart-title { font-size: 11px; color: var(--text-dim); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 10px; text-align: left; }
    canvas { width: 100% !important; height: 120px !important; }

    .meter { margin-bottom: 12px; }
    .meter-header { display: flex; justify-content: space-between; font-size: 11px; margin-bottom: 6px; }
    .meter-label { color: var(--text-dim); text-transform: uppercase; letter-spacing: 1px; }
    .meter-value { font-weight: 600; }
    .meter-bar { height: 8px; background: rgba(255,255,255,0.08); border-radius: 4px; overflow: hidden; }
    .meter-fill { height: 100%; border-radius: 4px; transition: width 0.6s ease; animation: barGrow 0.8s ease; }
    .meter-fill.cyan { background: linear-gradient(90deg, var(--cyan), #0abde3); }
    .meter-fill.green { background: linear-gradient(90deg, var(--green), #20bf6b); }
    .meter-fill.orange { background: linear-gradient(90deg, var(--orange), #f7b731); }
    .meter-fill.red { background: linear-gradient(90deg, var(--red), #ff6348); }

    .log-list { max-height: 200px; overflow-y: auto; text-align: left; }
    .log-item {
      display: flex; align-items: center; gap: 10px; padding: 10px 12px;
      border-bottom: 1px solid rgba(255,255,255,0.04); animation: slideIn 0.3s ease;
      font-size: 13px;
    }
    .log-item:last-child { border-bottom: none; }
    .log-dot { width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0; }
    .log-dot.lock { background: var(--cyan); }
    .log-dot.unlock { background: var(--orange); }
    .log-dot.system { background: var(--purple); }
    .log-time { color: var(--text-muted); font-size: 11px; margin-left: auto; white-space: nowrap; }
    .log-empty { color: var(--text-muted); font-size: 13px; padding: 24px; text-align: center; }

    .badges { display: flex; gap: 6px; justify-content: center; flex-wrap: wrap; margin-top: 12px; }
    .badge {
      display: inline-flex; align-items: center; gap: 4px;
      padding: 4px 10px; border-radius: 20px; font-size: 10px; font-weight: 600;
      text-transform: uppercase; letter-spacing: 0.5px;
    }
    .badge-green { background: rgba(6,214,160,0.15); border: 1px solid rgba(6,214,160,0.25); color: var(--green); }
    .badge-purple { background: rgba(114,9,183,0.15); border: 1px solid rgba(114,9,183,0.25); color: var(--purple); }
    .badge-cyan { background: rgba(6,214,160,0.15); border: 1px solid rgba(6,214,160,0.25); color: var(--cyan); }

    .toast-container { position: fixed; top: 16px; right: 16px; z-index: 1000; display: flex; flex-direction: column; gap: 8px; }
    .toast {
      padding: 12px 20px; border-radius: 12px; font-size: 13px; font-weight: 600;
      box-shadow: 0 8px 24px rgba(0,0,0,0.4); animation: slideIn 0.3s ease;
      display: flex; align-items: center; gap: 8px; backdrop-filter: blur(8px);
    }
    .toast-success { background: rgba(46,213,115,0.9); color: #fff; }
    .toast-info { background: rgba(72,219,251,0.9); color: #fff; }
    .toast-error { background: rgba(255,71,87,0.9); color: #fff; }
    .toast-warning { background: rgba(255,159,67,0.9); color: #fff; }

    .divider { border: none; border-top: 1px solid rgba(255,255,255,0.06); margin: 16px 0; }
    .footer { font-size: 11px; color: var(--text-muted); line-height: 1.8; }

    .connection-lost {
      position: fixed; bottom: 0; left: 0; right: 0; background: var(--red);
      color: #fff; text-align: center; padding: 8px; font-size: 13px; font-weight: 600;
      transform: translateY(100%); transition: transform 0.3s; z-index: 999;
    }
    .connection-lost.show { transform: translateY(0); animation: shake 0.5s ease; }

    ::-webkit-scrollbar { width: 4px; }
    ::-webkit-scrollbar-track { background: transparent; }
    ::-webkit-scrollbar-thumb { background: rgba(255,255,255,0.1); border-radius: 2px; }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <div class="top-bar">
        <div class="wifi-signal"><span class="wifi-dot"></span> <span id="wifiStatus">Connected</span></div>
        <button class="theme-btn" onclick="toggleTheme()" id="themeBtn">&#9790;</button>
      </div>
      <span class="lock-icon" id="lockIcon">&#128274;</span>
      <h1>Smart Door Lock</h1>
      <p class="subtitle">NodeMCU v3 IoT Controller - OTA Auto-Updated!</p>
      <div class="status-bar status-locked" id="statusBar">
        <span class="dot locked" id="statusDot"></span>
        <span id="statusText">Door is LOCKED</span>
      </div>
      <div class="btn-group">
        <button class="btn btn-unlock" onclick="sendAction('unlock')" id="btnUnlock">
          <span class="label">UNLOCK</span><span class="spinner"></span>
        </button>
        <button class="btn btn-lock" onclick="sendAction('lock')" id="btnLock">
          <span class="label">LOCK</span><span class="spinner"></span>
        </button>
        <a class="btn btn-ota btn-full" href="/update"><span class="label">FIRMWARE UPDATE (OTA)</span></a>
        <button class="btn btn-danger btn-full" onclick="sendAction('restart')" id="btnRestart">
          <span class="label">RESTART DEVICE</span><span class="spinner"></span>
        </button>
      </div>
    </div>

    <div class="card">
      <div class="tabs">
        <button class="tab active" onclick="switchTab('dashboard')">Dashboard</button>
        <button class="tab" onclick="switchTab('chart')">Memory</button>
        <button class="tab" onclick="switchTab('log')">Activity</button>
      </div>

      <div id="tab-dashboard" class="tab-content active">
        <div class="stats-grid">
          <div class="stat-card"><div class="stat-value cyan" id="statUptime">0s</div><div class="stat-label">Uptime</div></div>
          <div class="stat-card"><div class="stat-value green" id="statHeap">0</div><div class="stat-label">Free Heap</div></div>
          <div class="stat-card"><div class="stat-value orange" id="statWifi">0</div><div class="stat-label">WiFi dBm</div></div>
          <div class="stat-card"><div class="stat-value purple" id="statActions">0</div><div class="stat-label">Actions</div></div>
        </div>
        <div class="meter">
          <div class="meter-header"><span class="meter-label">Heap Usage</span><span class="meter-value" id="heapPercent">0%</span></div>
          <div class="meter-bar"><div class="meter-fill green" id="heapBar" style="width:0%"></div></div>
        </div>
        <div class="meter">
          <div class="meter-header"><span class="meter-label">WiFi Strength</span><span class="meter-value" id="wifiPercent">0%</span></div>
          <div class="meter-bar"><div class="meter-fill cyan" id="wifiBar" style="width:0%"></div></div>
        </div>
      </div>

      <div id="tab-chart" class="tab-content">
        <div class="chart-container">
          <div class="chart-title">Free Heap Over Time</div>
          <canvas id="heapChart"></canvas>
        </div>
        <div class="chart-container">
          <div class="chart-title">WiFi Signal Over Time</div>
          <canvas id="wifiChart"></canvas>
        </div>
      </div>

      <div id="tab-log" class="tab-content">
        <div class="log-list" id="logList">
          <div class="log-empty">No activity yet</div>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="footer">
        <p>IP: <strong id="footIp">-</strong> | Firmware: <strong>v%VERSION%</strong></p>
        <p>Chip ID: <strong id="footChip">-</strong> | Flash: <strong id="footFlash">-</strong></p>
      </div>
      <div class="badges">
        <span class="badge badge-green">Webhook</span>
        <span class="badge badge-purple">OTA</span>
        <span class="badge badge-cyan">mDNS</span>
      </div>
    </div>
  </div>

  <div class="toast-container" id="toasts"></div>
  <div class="connection-lost" id="connBanner">Connection lost — retrying...</div>

<script>
var actionCount = 0, heapData = [], wifiData = [], maxPoints = 30, failCount = 0;

function toast(msg, type) {
  var t = document.createElement('div');
  t.className = 'toast toast-' + (type||'info');
  t.textContent = msg;
  document.getElementById('toasts').appendChild(t);
  setTimeout(function(){ t.style.opacity='0'; t.style.transition='opacity 0.3s'; setTimeout(function(){ t.remove(); },300); }, 3000);
}

function addLog(msg, type) {
  var list = document.getElementById('logList');
  if (list.querySelector('.log-empty')) list.innerHTML = '';
  var item = document.createElement('div');
  item.className = 'log-item';
  var now = new Date();
  var ts = now.getHours().toString().padStart(2,'0') + ':' + now.getMinutes().toString().padStart(2,'0') + ':' + now.getSeconds().toString().padStart(2,'0');
  item.innerHTML = '<span class="log-dot ' + (type||'system') + '"></span><span>' + msg + '</span><span class="log-time">' + ts + '</span>';
  list.insertBefore(item, list.firstChild);
  if (list.children.length > 50) list.lastChild.remove();
}

function sendAction(action) {
  var btn = document.getElementById(action === 'unlock' ? 'btnUnlock' : action === 'lock' ? 'btnLock' : 'btnRestart');
  btn.classList.add('loading'); btn.disabled = true;
  var x = new XMLHttpRequest();
  x.open('GET', '/api/' + action);
  x.onload = function() {
    btn.classList.remove('loading'); btn.disabled = false;
    if (x.status === 200) {
      var d = JSON.parse(x.responseText);
      updateUI(d);
      actionCount++;
      document.getElementById('statActions').textContent = actionCount;
      if (action === 'restart') { toast('Device restarting...', 'warning'); addLog('Device restart triggered', 'system'); }
      else { toast('Door ' + (d.locked ? 'locked' : 'unlocked'), d.locked ? 'info' : 'success'); addLog('Door ' + (d.locked ? 'locked' : 'unlocked'), d.locked ? 'lock' : 'unlock'); }
      var icon = document.getElementById('lockIcon');
      icon.classList.remove('animate');
      void icon.offsetWidth;
      icon.classList.add('animate');
    } else { toast('Action failed!', 'error'); }
  };
  x.onerror = function() { btn.classList.remove('loading'); btn.disabled = false; toast('Network error!', 'error'); };
  x.send();
}

function updateUI(d) {
  document.getElementById('lockIcon').innerHTML = d.locked ? '&#128274;' : '&#128275;';
  document.getElementById('statusText').textContent = 'Door is ' + (d.locked ? 'LOCKED' : 'UNLOCKED');
  var bar = document.getElementById('statusBar');
  bar.className = 'status-bar ' + (d.locked ? 'status-locked' : 'status-unlocked');
  var dot = document.getElementById('statusDot');
  dot.className = 'dot ' + (d.locked ? 'locked' : 'unlocked');
}

function formatUptime(s) {
  if (s < 60) return s + 's';
  if (s < 3600) return Math.floor(s/60) + 'm ' + (s%60) + 's';
  var h = Math.floor(s/3600); s %= 3600;
  return h + 'h ' + Math.floor(s/60) + 'm';
}

function fetchStatus() {
  var x = new XMLHttpRequest();
  x.open('GET', '/api/status');
  x.timeout = 5000;
  x.onload = function() {
    if (x.status === 200) {
      failCount = 0;
      document.getElementById('connBanner').classList.remove('show');
      var d = JSON.parse(x.responseText);
      updateUI(d);
      document.getElementById('statUptime').textContent = formatUptime(d.uptime);
      document.getElementById('statHeap').textContent = (d.heap/1024).toFixed(1) + 'K';
      document.getElementById('statWifi').textContent = d.rssi;
      document.getElementById('footIp').textContent = d.ip;
      document.getElementById('footChip').textContent = d.chipId;
      document.getElementById('footFlash').textContent = (d.flashSize/1024) + 'KB';

      var heapMax = 81920;
      var heapUsed = ((heapMax - d.heap) / heapMax * 100).toFixed(0);
      document.getElementById('heapPercent').textContent = heapUsed + '%';
      var hBar = document.getElementById('heapBar');
      hBar.style.width = heapUsed + '%';
      hBar.className = 'meter-fill ' + (heapUsed > 80 ? 'red' : heapUsed > 60 ? 'orange' : 'green');

      var wifiQ = Math.min(100, Math.max(0, 2 * (d.rssi + 100)));
      document.getElementById('wifiPercent').textContent = wifiQ + '%';
      var wBar = document.getElementById('wifiBar');
      wBar.style.width = wifiQ + '%';
      wBar.className = 'meter-fill ' + (wifiQ < 30 ? 'red' : wifiQ < 60 ? 'orange' : 'cyan');

      heapData.push(d.heap); if (heapData.length > maxPoints) heapData.shift();
      wifiData.push(d.rssi); if (wifiData.length > maxPoints) wifiData.shift();
      drawChart('heapChart', heapData, 'rgba(6,214,160,0.8)', 'rgba(6,214,160,0.1)');
      drawChart('wifiChart', wifiData, 'rgba(114,9,183,0.8)', 'rgba(114,9,183,0.1)');
    }
  };
  x.onerror = x.ontimeout = function() {
    failCount++;
    if (failCount >= 3) document.getElementById('connBanner').classList.add('show');
  };
  x.send();
}

function drawChart(id, data, stroke, fill) {
  var c = document.getElementById(id);
  var ctx = c.getContext('2d');
  var W = c.width = c.offsetWidth * 2;
  var H = c.height = 240;
  ctx.clearRect(0,0,W,H);
  if (data.length < 2) return;
  var min = Math.min.apply(null,data), max = Math.max.apply(null,data);
  if (min === max) { min -= 1; max += 1; }
  var pad = 10;
  ctx.strokeStyle = 'rgba(255,255,255,0.05)'; ctx.lineWidth = 1;
  for (var g = 0; g < 4; g++) { var gy = pad + g * ((H-2*pad)/3); ctx.beginPath(); ctx.moveTo(0,gy); ctx.lineTo(W,gy); ctx.stroke(); }
  ctx.beginPath(); ctx.lineWidth = 3; ctx.strokeStyle = stroke; ctx.lineJoin = 'round';
  var pts = [];
  for (var i = 0; i < data.length; i++) {
    var x = pad + i * ((W-2*pad)/(maxPoints-1));
    var y = H - pad - ((data[i]-min)/(max-min)) * (H-2*pad);
    pts.push({x:x,y:y});
    if (i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
  }
  ctx.stroke();
  ctx.lineTo(pts[pts.length-1].x, H); ctx.lineTo(pts[0].x, H); ctx.closePath();
  ctx.fillStyle = fill; ctx.fill();
  ctx.fillStyle = stroke; ctx.beginPath(); ctx.arc(pts[pts.length-1].x, pts[pts.length-1].y, 5, 0, Math.PI*2); ctx.fill();
  ctx.fillStyle='rgba(255,255,255,0.15)'; ctx.beginPath(); ctx.arc(pts[pts.length-1].x, pts[pts.length-1].y, 10, 0, Math.PI*2); ctx.fill();
}

function switchTab(name) {
  document.querySelectorAll('.tab').forEach(function(t){ t.classList.remove('active'); });
  document.querySelectorAll('.tab-content').forEach(function(t){ t.classList.remove('active'); });
  document.getElementById('tab-'+name).classList.add('active');
  event.target.classList.add('active');
  if (name==='chart') { drawChart('heapChart',heapData,'rgba(6,214,160,0.8)','rgba(6,214,160,0.1)'); drawChart('wifiChart',wifiData,'rgba(114,9,183,0.8)','rgba(114,9,183,0.1)'); }
}

function toggleTheme() {
  var b = document.documentElement;
  var dark = b.getAttribute('data-theme') !== 'light';
  b.setAttribute('data-theme', dark ? 'light' : 'dark');
  document.getElementById('themeBtn').innerHTML = dark ? '&#9728;' : '&#9790;';
  toast('Switched to ' + (dark ? 'light' : 'dark') + ' mode', 'info');
}

addLog('Dashboard loaded', 'system');
fetchStatus();
setInterval(fetchStatus, 2000);
</script>
</body>
</html>
)rawliteral";

#define FIRMWARE_VERSION "1.4.2"

void sendWebhook(const char* action) {
  WiFiClient client;
  HTTPClient http;

  http.begin(client, webhookUrl);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"action\":\"";
  payload += action;
  payload += "\",\"device\":\"NodeMCU-DoorLock\",\"ip\":\"";
  payload += WiFi.localIP().toString();
  payload += "\",\"firmware\":\"v";
  payload += FIRMWARE_VERSION;
  payload += "\",\"uptime\":";
  payload += String(millis() / 1000);
  payload += "}";

  int httpCode = http.POST(payload);
  Serial.printf("[Webhook] %s -> HTTP %d\n", action, httpCode);
  http.end();
}

String buildStatusJson() {
  String json = "{\"locked\":";
  json += doorLocked ? "true" : "false";
  json += ",\"uptime\":";
  json += String(millis() / 1000);
  json += ",\"heap\":";
  json += String(ESP.getFreeHeap());
  json += ",\"rssi\":";
  json += String(WiFi.RSSI());
  json += ",\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\",\"chipId\":\"";
  json += String(ESP.getChipId(), HEX);
  json += "\",\"flashSize\":";
  json += String(ESP.getFlashChipRealSize());
  json += ",\"version\":\"";
  json += FIRMWARE_VERSION;
  json += "\"}";
  return json;
}

void handleRoot() {
  String page = FPSTR(webpage);
  page.replace("%VERSION%", FIRMWARE_VERSION);
  server.send(200, "text/html", page);
}

void handleApiStatus() {
  server.send(200, "application/json", buildStatusJson());
}

void handleApiUnlock() {
  doorLocked = false;
  digitalWrite(LED_PIN, LOW);
  sendWebhook("DOOR_OPENED");
  server.send(200, "application/json", buildStatusJson());
}

void handleApiLock() {
  doorLocked = true;
  digitalWrite(LED_PIN, HIGH);
  sendWebhook("DOOR_LOCKED");
  server.send(200, "application/json", buildStatusJson());
}

void handleApiRestart() {
  server.send(200, "application/json", "{\"restarting\":true}");
  delay(500);
  ESP.restart();
}

void checkForOTAUpdate() {
  Serial.println("[OTA] Checking for firmware updates...");

  BearSSL::WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, fwVersionUrl);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String newVersion = http.getString();
    newVersion.trim();
    Serial.printf("[OTA] Current: %s, Available: %s\n", FIRMWARE_VERSION, newVersion.c_str());

    if (newVersion != FIRMWARE_VERSION) {
      Serial.println("[OTA] New version found! Downloading...");
      http.end();

      ESPhttpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
      t_httpUpdate_return ret = ESPhttpUpdate.update(client, fwBinaryUrl);

      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("[OTA] Update failed: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
          break;
        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("[OTA] No updates available.");
          break;
        case HTTP_UPDATE_OK:
          Serial.println("[OTA] Update successful! Rebooting...");
          break;
      }
    } else {
      Serial.println("[OTA] Firmware is up to date.");
    }
  } else {
    Serial.printf("[OTA] Version check failed: HTTP %d\n", httpCode);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.println("\n=============================");
  Serial.println("  Smart Door Lock Server");
  Serial.printf("  Firmware: v%s\n", FIRMWARE_VERSION);
  Serial.println("=============================");

  Serial.printf("Connecting to %s", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("\nFailed to connect! Status: %d\n", WiFi.status());
    delay(5000);
    ESP.restart();
  }

  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin(mdnsHostname)) {
    Serial.printf("mDNS started: http://%s.local\n", mdnsHostname);
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("mDNS failed to start!");
  }

  // --- ArduinoOTA (upload from PlatformIO/Arduino IDE wirelessly) ---
  ArduinoOTA.setHostname(otaHostname);
  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Update starting...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update complete! Rebooting...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  // --- HTTP OTA (upload .bin from browser) ---
  httpUpdater.setup(&server, "/update");

  // Web routes
  server.on("/", handleRoot);
  server.on("/api/status", handleApiStatus);
  server.on("/api/unlock", handleApiUnlock);
  server.on("/api/lock", handleApiLock);
  server.on("/api/restart", handleApiRestart);

  server.begin();
  Serial.println("Smart Door Lock server started!");
  Serial.println("Webhook notifications enabled.");
  Serial.println("OTA enabled (IDE + Browser).");
  Serial.printf("Browser OTA: http://%s/update\n", WiFi.localIP().toString().c_str());
}

void loop() {
  MDNS.update();
  ArduinoOTA.handle();
  server.handleClient();

  if (millis() - lastUpdateCheck >= updateCheckInterval) {
    lastUpdateCheck = millis();
    checkForOTAUpdate();
  }
}
