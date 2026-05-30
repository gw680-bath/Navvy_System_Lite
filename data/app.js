(function () {
  const root = document.documentElement;
  const wsAddrEl = document.getElementById('wsAddr');
  const supportsFilePreview = location.protocol === 'file:';
  const hasTelemetry = !!document.getElementById('tele-armed');
  const hasSettings = !!document.getElementById('wifiForm');

  const state = {
    mode: 'joystick',
    source: 'rc',
    armed: false,
    connected: false,
    current: { x: 0, y: 0 },
    target: { x: 0, y: 0 },
    tank: { left: 0, right: 0 },
    deadzone: 0.08,
    smoothing: 0.18,
    lastSendAt: 0,
    activePointer: null,
    keys: { up: false, down: false, left: false, right: false },
  };

  const tele = hasTelemetry
    ? {
        armed: document.getElementById('tele-armed'),
        source: document.getElementById('tele-source'),
        rcok: document.getElementById('tele-rcok'),
        steer: document.getElementById('tele-steer'),
        throttle: document.getElementById('tele-throttle'),
        batt: document.getElementById('tele-batt'),
        last: document.getElementById('tele-last'),
      }
    : null;

  const $ = (id) => document.getElementById(id);

  function clamp(value, min, max) {
    return Math.min(max, Math.max(min, value));
  }

  function lerp(current, target, amount) {
    return current + (target - current) * amount;
  }

  function applyDeadzone(value, zone) {
    const abs = Math.abs(value);
    if (abs <= zone) {
      return 0;
    }
    const scaled = (abs - zone) / (1 - zone);
    return Math.sign(value) * clamp(scaled, 0, 1);
  }

  function setStatusHint(text) {
    const hint = $('controlHint');
    if (hint) {
      hint.textContent = text;
    }
  }

  function setTheme(theme) {
    root.setAttribute('data-theme', theme);
    localStorage.setItem('theme', theme);
    const label = theme === 'dark' ? '☀' : '🌙';
    document.querySelectorAll('.icon-btn').forEach((button) => {
      button.textContent = label;
      button.setAttribute('aria-label', theme === 'dark' ? 'Switch to light mode' : 'Switch to dark mode');
    });
  }

  function restoreTheme() {
    const saved = localStorage.getItem('theme');
    setTheme(saved === 'dark' ? 'dark' : 'light');
  }

  function wsUrl() {
    if (supportsFilePreview) {
      return 'ws://preview/ws';
    }
    return (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/ws';
  }

  let socket = null;
  let reconnectTimer = null;

  function send(payload) {
    if (socket && socket.readyState === 1) {
      socket.send(JSON.stringify(payload));
    }
  }

  function setWsState(ok) {
    state.connected = ok;
    document.querySelectorAll('#info-ws').forEach((element) => {
      element.textContent = ok ? 'connected' : 'disconnected';
    });
  }

  function connect() {
    if (supportsFilePreview) {
      setWsState(false);
      return;
    }

    clearTimeout(reconnectTimer);
    socket = new WebSocket(wsUrl());
    socket.onopen = () => setWsState(true);
    socket.onclose = () => {
      setWsState(false);
      reconnectTimer = setTimeout(connect, 2000);
    };
    socket.onerror = () => setWsState(false);
    socket.onmessage = (event) => handleMessage(event.data);
  }

  function renderTelemetry(packet) {
    if (!tele) {
      return;
    }
    tele.armed.textContent = packet.armed ? 'true' : 'false';
    tele.source.textContent = packet.source || 'unknown';
    tele.rcok.textContent = packet.rcOk ? 'true' : 'false';
    tele.steer.textContent = Number(packet.steering ?? 0).toFixed(2);
    tele.throttle.textContent = Number(packet.throttle ?? 0).toFixed(2);
    tele.batt.textContent = packet.battery ? String(packet.battery) : '—';
    tele.last.textContent = new Date().toLocaleTimeString();
  }

  function renderInfo(info) {
    const bind = (id, value) => {
      const element = $(id);
      if (element) {
        element.textContent = value || '—';
      }
    };

    bind('info-mac', info.mac);
    bind('info-ip', info.ip);
    bind('info-ssid', info.ssid);
    bind('info-fw', info.fw);
    bind('info-lastcmd', info.lastCommand);
  }

  function handleMessage(raw) {
    try {
      const packet = JSON.parse(raw);
      if (packet.telemetry) {
        renderTelemetry(packet.telemetry);
      }
      if (packet.info) {
        renderInfo(packet.info);
      }
    } catch (error) {
      console.warn('Unable to parse message', error);
    }
  }

  function syncSourceUI() {
    document.querySelectorAll('#sourceSeg .seg').forEach((button) => {
      const active = button.dataset.source === state.source;
      button.classList.toggle('active', active);
      button.setAttribute('aria-pressed', active ? 'true' : 'false');
    });
  }

  function syncModeUI() {
    const joystickArea = $('joystickArea');
    const tankArea = $('tankArea');
    if (joystickArea) joystickArea.classList.toggle('hidden', state.mode !== 'joystick');
    if (tankArea) tankArea.classList.toggle('hidden', state.mode !== 'tank');

    document.querySelectorAll('#modeSeg .seg').forEach((button) => {
      const active = button.dataset.mode === state.mode;
      button.classList.toggle('active', active);
      button.setAttribute('aria-pressed', active ? 'true' : 'false');
    });
  }

  function syncArmUI() {
    const armButton = $('armBtn');
    if (!armButton) {
      return;
    }
    armButton.textContent = state.armed ? 'DISARM' : 'ARM';
    armButton.classList.toggle('danger', state.armed);
    armButton.setAttribute('aria-pressed', state.armed ? 'true' : 'false');
  }

  function setJoystickVisual(x, y) {
    const stick = $('stick');
    if (!stick) {
      return;
    }
    stick.style.transform = `translate(${(x * 56).toFixed(1)}px, ${(-y * 56).toFixed(1)}px)`;
  }

  function setJoystickTarget(x, y) {
    state.target.x = clamp(applyDeadzone(x, state.deadzone), -1, 1);
    state.target.y = clamp(applyDeadzone(y, state.deadzone), -1, 1);
  }

  function sendControl() {
    const now = performance.now();
    if (now - state.lastSendAt < 20) {
      return;
    }
    state.lastSendAt = now;

    if (state.mode === 'joystick') {
      send({ type: 'control', mode: 'joystick', x: state.current.x, y: state.current.y });
      return;
    }

    send({ type: 'control', mode: 'tank', left: state.tank.left, right: state.tank.right });
  }

  function animateJoystick() {
    state.current.x = lerp(state.current.x, state.target.x, state.smoothing);
    state.current.y = lerp(state.current.y, state.target.y, state.smoothing);
    setJoystickVisual(state.current.x, state.current.y);
    sendControl();
    requestAnimationFrame(animateJoystick);
  }

  function bindControls() {
    const sourceSeg = $('sourceSeg');
    if (sourceSeg) {
      sourceSeg.addEventListener('click', (event) => {
        const button = event.target.closest('button');
        if (!button) {
          return;
        }
        state.source = button.dataset.source;
        syncSourceUI();
        send({ type: 'source', source: state.source });
      });
    }

    const modeSeg = $('modeSeg');
    if (modeSeg) {
      modeSeg.addEventListener('click', (event) => {
        const button = event.target.closest('button');
        if (!button) {
          return;
        }
        state.mode = button.dataset.mode;
        syncModeUI();
        setStatusHint(state.mode === 'joystick' ? 'Joystick mode active. Use touch, mouse, or arrow keys.' : 'Tank mode active. Use both sliders together for drive control.');
        send({ type: 'mode', mode: state.mode });
      });
    }

    const armButton = $('armBtn');
    if (armButton) {
      armButton.addEventListener('click', () => {
        state.armed = !state.armed;
        syncArmUI();
        send({ type: 'arm', armed: state.armed });
      });
    }

    const neutralButton = $('neutralBtn');
    if (neutralButton) {
      neutralButton.addEventListener('click', () => {
        state.target = { x: 0, y: 0 };
        state.current = { x: 0, y: 0 };
        state.tank = { left: 0, right: 0 };
        const leftSlider = $('leftSlider');
        const rightSlider = $('rightSlider');
        if (leftSlider) leftSlider.value = '0';
        if (rightSlider) rightSlider.value = '0';
        setJoystickVisual(0, 0);
        send({ type: 'neutral' });
      });
    }

    const joystickArea = $('joystickArea');
    if (joystickArea) {
      const stick = $('stick');
      const pointerToXY = (event) => {
        const rect = joystickArea.getBoundingClientRect();
        const x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
        const y = -(((event.clientY - rect.top) / rect.height) * 2 - 1);
        return { x: clamp(x, -1, 1), y: clamp(y, -1, 1) };
      };

      joystickArea.addEventListener('pointerdown', (event) => {
        joystickArea.setPointerCapture(event.pointerId);
        state.activePointer = event.pointerId;
        const point = pointerToXY(event);
        setJoystickTarget(point.x, point.y);
        if (stick) {
          stick.classList.add('dragging');
        }
        setStatusHint('Dragging joystick. Release to return to neutral.');
      });

      joystickArea.addEventListener('pointermove', (event) => {
        if (state.activePointer !== event.pointerId) {
          return;
        }
        const point = pointerToXY(event);
        setJoystickTarget(point.x, point.y);
      });

      const finishPointer = (event) => {
        if (state.activePointer !== event.pointerId) {
          return;
        }
        state.activePointer = null;
        setJoystickTarget(0, 0);
        if (stick) {
          stick.classList.remove('dragging');
        }
        setStatusHint('Joystick returned to neutral.');
      };

      joystickArea.addEventListener('pointerup', finishPointer);
      joystickArea.addEventListener('pointercancel', finishPointer);
      joystickArea.addEventListener('lostpointercapture', () => {
        state.activePointer = null;
        setJoystickTarget(0, 0);
      });
    }

    const leftSlider = $('leftSlider');
    const rightSlider = $('rightSlider');
    const updateTank = () => {
      state.tank.left = Number(leftSlider ? leftSlider.value : 0);
      state.tank.right = Number(rightSlider ? rightSlider.value : 0);
      sendControl();
    };

    if (leftSlider) {
      leftSlider.addEventListener('input', updateTank);
      leftSlider.setAttribute('aria-label', 'Left drive output');
    }
    if (rightSlider) {
      rightSlider.addEventListener('input', updateTank);
      rightSlider.setAttribute('aria-label', 'Right drive output');
    }

    document.addEventListener('keydown', (event) => {
      if (event.altKey || event.ctrlKey || event.metaKey) {
        return;
      }

      const map = { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right' };
      if (event.key === ' ') {
        event.preventDefault();
        if (armButton) {
          armButton.click();
        }
        return;
      }

      const key = map[event.key];
      if (!key) {
        return;
      }
      event.preventDefault();
      state.keys[key] = true;
      updateKeyboardTarget();
    });

    document.addEventListener('keyup', (event) => {
      const map = { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right' };
      const key = map[event.key];
      if (!key) {
        return;
      }
      state.keys[key] = false;
      updateKeyboardTarget();
    });
  }

  function updateKeyboardTarget() {
    const x = (state.keys.right ? 1 : 0) - (state.keys.left ? 1 : 0);
    const y = (state.keys.up ? 1 : 0) - (state.keys.down ? 1 : 0);
    setJoystickTarget(x, y);
  }

  function bindSettings() {
    const wifiForm = $('wifiForm');
    if (!wifiForm) {
      return;
    }

    wifiForm.addEventListener('submit', (event) => {
      event.preventDefault();
      const payload = {
        ssid: $('wifiSsid') ? $('wifiSsid').value.trim() : '',
        pass: $('wifiPass') ? $('wifiPass').value : '',
      };

      fetch('/api/wifi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      })
        .then((response) => response.json())
        .then((json) => {
          const banner = $('wifiStatus');
          if (banner) {
            banner.textContent = json && json.ok ? 'Saved' : 'Save failed';
            banner.setAttribute('data-state', json && json.ok ? 'ok' : 'error');
          }
        })
        .catch(() => {
          const banner = $('wifiStatus');
          if (banner) {
            banner.textContent = 'Save failed';
            banner.setAttribute('data-state', 'error');
          }
        });
    });
  }

  function bindThemeButtons() {
    document.querySelectorAll('.icon-btn').forEach((button) => {
      button.addEventListener('click', () => {
        const current = root.getAttribute('data-theme') === 'dark' ? 'dark' : 'light';
        setTheme(current === 'dark' ? 'light' : 'dark');
      });
    });
  }

  function requestInfo() {
    if (!hasSettings || supportsFilePreview) {
      return;
    }

    fetch('/api/info')
      .then((response) => response.json())
      .then((json) => renderInfo(json || {}))
      .catch(() => {});
  }

  function seedPreview() {
    if (!supportsFilePreview) {
      return;
    }

    renderInfo({
      mac: 'A4:CF:12:9B:20:31',
      ip: '192.168.4.23',
      ssid: 'Navvy-Lite',
      fw: '0.1.0-preview',
      lastCommand: 'Web control active',
    });

    if (tele) {
      renderTelemetry({
        armed: false,
        source: 'web',
        rcOk: true,
        steering: 0,
        throttle: 0,
        battery: '12.4V',
      });
    }
  }

  restoreTheme();
  bindControls();
  bindSettings();
  bindThemeButtons();
  syncSourceUI();
  syncModeUI();
  syncArmUI();
  connect();
  requestInfo();
  seedPreview();
  setStatusHint('Use touch, mouse, or keyboard to drive. Space toggles ARM.');
  requestAnimationFrame(animateJoystick);
})();