(function () {
  const isPreview = location.protocol === 'file:';
  const isMainPage = document.body.dataset.page === 'main';
  const isSettingsPage = document.body.dataset.page === 'settings';
  const ui = {};

  const state = {
    handed: 'right',
    theme: 'dark',
    mode: 'joystick',
    source: 'rc',
    armed: false,
    connected: false,
    current: { x: 0, y: 0 },
    target: { x: 0, y: 0 },
    throttleLeft: 0,
    throttleRight: 0,
    deadzone: 0.08,
    smoothing: 0.28,
    lastSendAt: 0,
    activePointer: null,
    keys: { up: false, down: false, left: false, right: false },
    firmware: '0.1.0',
    network: {
      connectivity: 'Access Point',
      websocket: 'Disconnected',
      signalBars: 0,
      signalQuality: 0,
      signalDbm: 0,
    },
  };

  const previewInfo = {
    mac: 'A4:CF:12:9B:20:31',
    apMac: 'A4:CF:12:9B:20:32',
    ip: '192.168.4.23',
    wifiNetwork: 'Navvy-Lite-01',
    ssid: 'Navvy-Lite-01',
    connectivity: 'SoftAP + WebSocket',
    websocket: 'Connected',
    wifiSignalBars: 3,
    wifiSignalQuality: 72,
    wifiRssiDbm: -64,
    leftHanded: false,
  };

  function $(id) {
    return document.getElementById(id);
  }

  function clamp(value, min, max) {
    return Math.min(max, Math.max(min, value));
  }

  function lerp(current, target, amount) {
    return current + (target - current) * amount;
  }

  function applyDeadzone(value, zone) {
    const magnitude = Math.abs(value);
    if (magnitude <= zone) {
      return 0;
    }
    return Math.sign(value) * clamp((magnitude - zone) / (1 - zone), 0, 1);
  }

  function normToUs(value) {
    return Math.round(1500 + clamp(value, -1, 1) * 500);
  }

  function usToNorm(us) {
    return clamp((us - 1500) / 500, -1, 1);
  }

  function sourceLabel(source, rcOk) {
    if (source === 2) return 'Control App';
    if (source === 1 && rcOk) return 'RC Receiver';
    return 'No RC Signal';
  }

  function modeLabel(mode) {
    return mode === 'tank' ? 'Tank Steer' : 'Joystick';
  }

  function networkLabel(connectivity, websocket) {
    if ((connectivity || '').includes('Access Point') && websocket !== 'Connected') {
      return 'AP mode • waiting for browser';
    }
    if ((connectivity || '').includes('Station') && websocket !== 'Connected') {
      return 'Station mode • reconnecting';
    }
    if (websocket === 'Connected') {
      return `${connectivity || 'Network'} • online`;
    }
    return `${connectivity || 'Network'} • ${websocket || 'offline'}`;
  }

  function signalLevelFromQuality(quality) {
    if (quality >= 85) return 4;
    if (quality >= 65) return 3;
    if (quality >= 40) return 2;
    if (quality >= 15) return 1;
    return 0;
  }

  function setSignalLevel(level) {
    const clamped = clamp(Math.round(level), 0, 4);
    document.querySelectorAll('.signal-bars').forEach((node) => {
      node.dataset.level = String(clamped);
    });
  }

  function describeSignal(info) {
    const connectivity = info.connectivity || 'Disconnected';
    const websocket = info.websocket || 'Offline';
    const bars = typeof info.wifiSignalBars === 'number' ? info.wifiSignalBars : signalLevelFromQuality(info.wifiSignalQuality ?? 0);
    const quality = typeof info.wifiSignalQuality === 'number' ? info.wifiSignalQuality : 0;
    const rssiDbm = typeof info.wifiRssiDbm === 'number' ? info.wifiRssiDbm : 0;

    return {
      text: `${connectivity} • ${websocket}`,
      bars,
      signalText: quality > 0 ? `${bars}/4 ${rssiDbm ? `(${rssiDbm} dBm)` : ''}`.trim() : `${bars}/4`,
      quality,
      rssiDbm,
    };
  }

  function refreshNetworkIndicators() {
    const chipText = $('netChipText');
    const signalValue = $('signalValue');
    const text = state.network.text || networkLabel(state.network.connectivity, state.network.websocket);

    if (chipText) chipText.textContent = text;
    if (signalValue) signalValue.textContent = state.network.signalText || '--';
    setSignalLevel(state.network.signalBars || 0);
  }

  function setTheme(theme) {
    state.theme = theme;
    document.body.dataset.theme = theme;
    localStorage.setItem('navvy-theme', theme);
    document.querySelectorAll('#themeBtn span').forEach((node) => {
      node.textContent = theme === 'dark' ? '☾' : '☼';
    });
  }

  function setHandedness(handed) {
    state.handed = handed === 'left' ? 'left' : 'right';
    document.body.dataset.handed = state.handed;
    localStorage.setItem('navvy-handed', state.handed);

    const leftActive = state.handed === 'left';
    const buttons = document.querySelectorAll('#handedSeg .seg');
    buttons.forEach((button) => {
      const active = button.dataset.handed === state.handed;
      button.classList.toggle('active', active);
      button.setAttribute('aria-pressed', active ? 'true' : 'false');
    });

    const armButton = ui.armButton;
    if (armButton) {
      armButton.textContent = state.armed ? 'DISARM' : 'ARM';
    }

    if (isSettingsPage) {
      const note = $('wifiStatus');
      if (note) {
        note.textContent = state.handed === 'left' ? 'Left-handed layout selected.' : 'Right-handed layout selected.';
      }
    }
  }

  function setArmState(armed) {
    state.armed = !!armed;
    const pill = ui.armPill;
    const armButton = ui.armButton;

    if (pill) {
      pill.textContent = state.armed ? 'ARMED' : 'DISARMED';
      pill.classList.toggle('arm-pill-armed', state.armed);
      pill.classList.toggle('arm-pill-disarmed', !state.armed);
      pill.setAttribute('aria-pressed', state.armed ? 'true' : 'false');
    }

    if (armButton) {
      armButton.textContent = state.armed ? 'DISARM' : 'ARM';
      armButton.classList.toggle('arm-btn-arm', !state.armed);
      armButton.classList.toggle('arm-btn-disarm', state.armed);
      armButton.setAttribute('aria-pressed', state.armed ? 'true' : 'false');
    }
  }

  function setStatusSummary(packet) {
    const statusValue = $('statusValue');
    const sourceValue = $('sourceValue');
    const modeValue = $('modeValue');
    const batteryValue = $('batteryValue');
    const connectionState = $('connectivityState') || $('connectionState');
    const armPill = $('armPill');

    if (statusValue) statusValue.textContent = packet.armed ? 'Armed' : 'Disarmed';
    if (sourceValue) sourceValue.textContent = sourceLabel(packet.source, packet.rcOk);
    if (modeValue) modeValue.textContent = modeLabel(state.mode);
    if (batteryValue) batteryValue.textContent = packet.batteryV ? `${packet.batteryV.toFixed(1)}V` : '--';
    if (connectionState) connectionState.textContent = packet.webOk ? 'Connected' : 'Disconnected';
    setArmState(packet.armed);
  }

  function setSummaryLines(info) {
    const bind = (id, value) => {
      const node = $(id);
      if (node) {
        node.textContent = value || '--';
      }
    };

    bind('deviceMac', info.mac || info.apMac);
    bind('wifiNetwork', info.wifiNetwork);
    bind('wifiSsidInfo', info.ssid);
    bind('firmwareVersion', info.firmware || state.firmware);
    bind('connectivityState', info.connectivity);
    bind('websocketState', info.websocket);
    bind('signalValue', info.signalText);
    bind('deviceLog', `MAC: ${info.mac || '--'}\nAP MAC: ${info.apMac || '--'}\nIP: ${info.ip || '--'}\nWi-Fi: ${info.wifiNetwork || '--'}\nSSID: ${info.ssid || '--'}\nConnectivity: ${info.connectivity || '--'}`);
  }

  function setHandedButtons() {
    document.querySelectorAll('#handedSeg .seg').forEach((button) => {
      const active = button.dataset.handed === state.handed;
      button.classList.toggle('active', active);
      button.setAttribute('aria-pressed', active ? 'true' : 'false');
    });
  }

  function setMode(mode) {
    state.mode = mode === 'tank' ? 'tank' : 'joystick';
    document.body.dataset.mode = state.mode;
    const joystickPanel = $('joystickPanel');
    const tankPanel = $('tankPanel');

    if (joystickPanel) joystickPanel.classList.toggle('hidden', state.mode !== 'joystick');
    if (tankPanel) tankPanel.classList.toggle('hidden', state.mode !== 'tank');

    document.querySelectorAll('#modeSeg .seg').forEach((button) => {
      const active = button.dataset.mode === state.mode;
      button.classList.toggle('active', active);
      button.setAttribute('aria-pressed', active ? 'true' : 'false');
    });

    const modeValue = $('modeValue');
    if (modeValue) modeValue.textContent = modeLabel(state.mode);

    syncTankControlsFromDrive();
  }

  function setSource(source) {
    state.source = source === 'web' ? 'web' : 'rc';
    document.querySelectorAll('#sourceSeg .seg').forEach((button) => {
      const active = button.dataset.source === state.source;
      button.classList.toggle('active', active);
      button.setAttribute('aria-pressed', active ? 'true' : 'false');
    });
  }

  function setHint(text) {
    const hint = $('controlHint');
    if (hint) hint.textContent = '';
  }

  function positionThumb(x, y) {
    if (!ui.joystickThumb) return;
    ui.joystickThumb.style.transform = `translate(calc(-50% + ${(x * 74).toFixed(1)}px), calc(-50% + ${(-y * 74).toFixed(1)}px))`;
  }

  function setDriveSides(left, right) {
    state.throttleLeft = clamp(left, -1, 1);
    state.throttleRight = clamp(right, -1, 1);
  }

  function tankPointerState() {
    if (!state.tankPointers) {
      state.tankPointers = { left: null, right: null };
    }
    return state.tankPointers;
  }

  function tankValueFromPointer(trackNode, event) {
    const rect = trackNode.getBoundingClientRect();
    const normalized = clamp((rect.bottom - event.clientY) / rect.height, 0, 1);
    return normalized * 2 - 1;
  }

  function commitTankControl() {
    syncTankControlsFromDrive();
    sendControl();
  }

  function setTankSideFromPointer(side, event, trackNode, sliderNode) {
    const pointers = tankPointerState();
    if (event.type === 'pointerdown') {
      pointers[side] = event.pointerId;
      if (sliderNode && sliderNode.setPointerCapture) {
        sliderNode.setPointerCapture(event.pointerId);
      }
    } else if (pointers[side] !== event.pointerId) {
      return;
    }

    const value = tankValueFromPointer(trackNode, event);
    if (side === 'left') {
      state.throttleLeft = value;
    } else {
      state.throttleRight = value;
    }

    if (sliderNode) {
      sliderNode.value = String(Math.round(value * 100));
    }

    commitTankControl();
    event.preventDefault();
  }

  function releaseTankSide(side, event, sliderNode) {
    const pointers = tankPointerState();
    if (pointers[side] !== event.pointerId) {
      return;
    }

    pointers[side] = null;
    if (sliderNode && sliderNode.releasePointerCapture) {
      try {
        sliderNode.releasePointerCapture(event.pointerId);
      } catch (_) {}
    }
  }

  function setSignedMeter(fillNode, valueNode, value) {
    if (!fillNode || !valueNode) return;

    const percent = Math.round(clamp(value, -1, 1) * 100);
    fillNode.style.height = `${Math.abs(percent) * 0.49}%`;

    if (percent >= 0) {
      fillNode.style.top = 'auto';
      fillNode.style.bottom = '50%';
      fillNode.classList.add('is-forward');
      fillNode.classList.remove('is-reverse');
    } else {
      fillNode.style.bottom = 'auto';
      fillNode.style.top = '50%';
      fillNode.classList.add('is-reverse');
      fillNode.classList.remove('is-forward');
    }

    valueNode.textContent = `${percent >= 0 ? '+' : ''}${percent}%`;
  }

  function renderDriveMeters() {
    setSignedMeter($('leftDriveBar'), $('leftDriveValue'), state.throttleLeft);
    setSignedMeter($('rightDriveBar'), $('rightDriveValue'), state.throttleRight);
  }

  function syncTankControlsFromDrive() {
    const left = clamp(state.throttleLeft, -1, 1);
    const right = clamp(state.throttleRight, -1, 1);
    const leftSlider = $('leftSlider');
    const rightSlider = $('rightSlider');
    const leftFill = $('leftFill');
    const rightFill = $('rightFill');

    if (leftSlider) leftSlider.value = String(Math.round(left * 100));
    if (rightSlider) rightSlider.value = String(Math.round(right * 100));

    const applyTankFill = (fillNode, value) => {
      if (!fillNode) return;
      fillNode.style.height = `${Math.abs(value) * 49}%`;
      fillNode.classList.toggle('is-forward', value >= 0);
      fillNode.classList.toggle('is-reverse', value < 0);
      if (value >= 0) {
        fillNode.style.top = 'auto';
        fillNode.style.bottom = '50%';
      } else {
        fillNode.style.bottom = 'auto';
        fillNode.style.top = '50%';
      }
    };

    applyTankFill(leftFill, left);
    applyTankFill(rightFill, right);

    const steer = clamp((right - left) / 2, -1, 1);
    const throttle = clamp((left + right) / 2, -1, 1);
    state.steerUs = normToUs(steer);
    state.throttleUs = normToUs(throttle);

    renderDriveMeters();
  }

  function updateTankVisuals() {
    const left = $('leftSlider');
    const right = $('rightSlider');
    const leftFill = $('leftFill');
    const rightFill = $('rightFill');

    if (left && leftFill) {
      const value = Number(left.value);
      state.throttleLeft = clamp(value / 100, -1, 1);
    }

    if (right && rightFill) {
      const value = Number(right.value);
      state.throttleRight = clamp(value / 100, -1, 1);
    }

    const leftValue = state.throttleLeft;
    const rightValue = state.throttleRight;
    const steer = clamp((rightValue - leftValue) / 2, -1, 1);
    const throttle = clamp((leftValue + rightValue) / 2, -1, 1);
    state.steerUs = normToUs(steer);
    state.throttleUs = normToUs(throttle);

    syncTankControlsFromDrive();
    renderDriveMeters();
    sendControl();
  }

  function applyJoystickTarget(x, y) {
    const nx = clamp(applyDeadzone(x, state.deadzone), -1, 1);
    const ny = clamp(applyDeadzone(y, state.deadzone), -1, 1);
    state.target.x = nx;
    state.target.y = ny;
    setDriveSides(ny + nx, ny - nx);
    state.steerUs = normToUs(nx);
    state.throttleUs = normToUs(ny);
    renderDriveMeters();
  }

  function send(payload) {
    if (state.socket && state.socket.readyState === WebSocket.OPEN) {
      state.socket.send(JSON.stringify(payload));
    }
  }

  function sendControl() {
    const now = performance.now();
    if (now - state.lastSendAt < 16) return;
    state.lastSendAt = now;

    const payload = {
      type: 'control',
      mode: state.mode,
      steeringUs: Math.round(state.steerUs),
      throttleUs: Math.round(state.throttleUs),
    };

    if (state.mode === 'tank') {
      payload.driveLeftUs = normToUs(state.throttleLeft);
      payload.driveRightUs = normToUs(state.throttleRight);
    }

    send(payload);
  }

  function animateJoystick() {
    if (!ui.joystickThumb) {
      return;
    }

    state.current.x = lerp(state.current.x, state.target.x, state.smoothing);
    state.current.y = lerp(state.current.y, state.target.y, state.smoothing);
    positionThumb(state.current.x, state.current.y);
    sendControl();
    requestAnimationFrame(animateJoystick);
  }

  function syncPreviewValues() {
    setHandedness(localStorage.getItem('navvy-handed') || 'right');
    setTheme(localStorage.getItem('navvy-theme') || 'dark');
    document.body.dataset.mode = 'joystick';
    setSource('rc');
    setMode('joystick');
    setArmState(false);
    setSummaryLines({ ...previewInfo, signalText: '3/4 (-64 dBm)' });
    state.network = {
      connectivity: previewInfo.connectivity,
      websocket: previewInfo.websocket,
      signalBars: previewInfo.wifiSignalBars,
      signalQuality: previewInfo.wifiSignalQuality,
      signalDbm: previewInfo.wifiRssiDbm,
      text: networkLabel(previewInfo.connectivity, previewInfo.websocket),
      signalText: '3/4 (-64 dBm)',
    };
    refreshNetworkIndicators();
    setStatusSummary({ armed: false, source: 1, webOk: true, batteryV: 22.4 });
  }

  function updateFromTelemetry(packet) {
    state.armed = !!packet.armed;
    state.source = packet.source === 2 ? 'web' : 'rc';
    state.steerUs = packet.steeringUs ?? 1500;
    state.throttleUs = packet.throttleUs ?? 1500;
    state.connected = !!packet.webOk;
    setSource(state.source);
    setStatusSummary(packet);
    state.network.websocket = packet.webOk ? 'Connected' : 'Disconnected';
    state.network.text = networkLabel(state.network.connectivity, state.network.websocket);
    refreshNetworkIndicators();

    if (typeof packet.throttleLeftUs === 'number' && typeof packet.throttleRightUs === 'number') {
      setDriveSides(usToNorm(packet.throttleLeftUs), usToNorm(packet.throttleRightUs));
    } else if (typeof packet.displayDriveLeftUs === 'number' && typeof packet.displayDriveRightUs === 'number') {
      setDriveSides(usToNorm(packet.displayDriveLeftUs), usToNorm(packet.displayDriveRightUs));
    } else if (typeof packet.driveLeftUs === 'number' && typeof packet.driveRightUs === 'number') {
      setDriveSides(usToNorm(packet.driveLeftUs), usToNorm(packet.driveRightUs));
    } else if (typeof packet.throttleLeft === 'number' && typeof packet.throttleRight === 'number') {
      setDriveSides(packet.throttleLeft, packet.throttleRight);
    } else if (typeof packet.ThrottleLeft === 'number' && typeof packet.ThrottleRight === 'number') {
      setDriveSides(packet.ThrottleLeft / 100, packet.ThrottleRight / 100);
    } else {
      const steer = applyDeadzone(usToNorm(state.steerUs), state.deadzone);
      const throttle = applyDeadzone(usToNorm(state.throttleUs), state.deadzone);
      setDriveSides(throttle + steer, throttle - steer);
    }

    syncTankControlsFromDrive();

    const rcNode = $('connectivityState');
    if (rcNode) {
      rcNode.textContent = packet.webOk ? 'Connected' : 'Offline';
    }
  }

  function setInfoFromServer(info) {
    const signal = describeSignal(info || {});
    state.network = {
      connectivity: info.connectivity || 'Disconnected',
      websocket: info.websocket || 'Offline',
      signalBars: signal.bars,
      signalQuality: signal.quality,
      signalDbm: signal.rssiDbm,
      text: networkLabel(info.connectivity, info.websocket),
      signalText: signal.signalText,
    };
    setSummaryLines({
      mac: info.mac,
      apMac: info.apMac,
      ip: info.ip,
      wifiNetwork: info.wifiNetwork,
      ssid: info.ssid,
      connectivity: info.connectivity,
      websocket: info.websocket,
      firmware: info.firmware,
      signalText: signal.signalText,
    });
    refreshNetworkIndicators();

    if (typeof info.leftHanded === 'boolean') {
      setHandedness(info.leftHanded ? 'left' : 'right');
    }
  }

  function updateKeyboardTarget() {
    const x = (state.keys.right ? 1 : 0) - (state.keys.left ? 1 : 0);
    const y = (state.keys.up ? 1 : 0) - (state.keys.down ? 1 : 0);
    applyJoystickTarget(x, y);
  }

  function installBindings() {
    const suppressGestureZoom = (event) => {
      event.preventDefault();
    };

    document.addEventListener('gesturestart', suppressGestureZoom, { passive: false });
    document.addEventListener('gesturechange', suppressGestureZoom, { passive: false });
    document.addEventListener('gestureend', suppressGestureZoom, { passive: false });
    document.addEventListener('touchmove', (event) => {
      if (event.touches && event.touches.length > 1) {
        event.preventDefault();
      }
    }, { passive: false });

    ui.armPill = $('armPill');
    ui.armButton = $('armBtn');
    ui.joystickThumb = $('joystickThumb');

    const themeButton = $('themeBtn');
    if (themeButton) {
      themeButton.addEventListener('click', () => {
        setTheme(state.theme === 'dark' ? 'light' : 'dark');
      });
    }

    const sourceSeg = $('sourceSeg');
    if (sourceSeg) {
      sourceSeg.addEventListener('click', (event) => {
        const button = event.target.closest('button');
        if (!button) return;
        setSource(button.dataset.source);
        send({ type: 'source', source: state.source });
      });
    }

    const modeSeg = $('modeSeg');
    if (modeSeg) {
      modeSeg.addEventListener('click', (event) => {
        const button = event.target.closest('button');
        if (!button) return;
        setMode(button.dataset.mode);
        setHint('');
        send({ type: 'mode', mode: state.mode });
      });
    }

    if (ui.armPill) {
      ui.armPill.addEventListener('click', () => {
        if (ui.armButton) {
          ui.armButton.click();
          return;
        }
        setArmState(!state.armed);
        send({ type: state.armed ? 'arm' : 'disarm' });
      });
    }

    if (ui.armButton) {
      ui.armButton.addEventListener('click', () => {
        setArmState(!state.armed);
        send({ type: state.armed ? 'arm' : 'disarm' });
      });
    }

    const neutralButton = $('neutralBtn');
    if (neutralButton) {
      neutralButton.addEventListener('click', () => {
        state.target = { x: 0, y: 0 };
        state.current = { x: 0, y: 0 };
        state.steerUs = 1500;
        state.throttleUs = 1500;
        const left = $('leftSlider');
        const right = $('rightSlider');
        if (left) left.value = '0';
        if (right) right.value = '0';
        updateTankVisuals();
        positionThumb(0, 0);
        send({ type: 'neutral' });
      });
    }

    const joystickArea = $('joystickArea');
    if (joystickArea) {
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
        applyJoystickTarget(point.x, point.y);
        setHint('');
      });

      joystickArea.addEventListener('pointermove', (event) => {
        if (state.activePointer !== event.pointerId) return;
        const point = pointerToXY(event);
        applyJoystickTarget(point.x, point.y);
      });

      const releasePointer = (event) => {
        if (state.activePointer !== event.pointerId) return;
        state.activePointer = null;
        applyJoystickTarget(0, 0);
        setHint('');
      };

      joystickArea.addEventListener('pointerup', releasePointer);
      joystickArea.addEventListener('pointercancel', releasePointer);
      joystickArea.addEventListener('lostpointercapture', () => {
        state.activePointer = null;
        applyJoystickTarget(0, 0);
      });
    }

    const leftSlider = $('leftSlider');
    const rightSlider = $('rightSlider');
    if (leftSlider) {
      leftSlider.addEventListener('input', updateTankVisuals);
      const leftTrack = leftSlider.closest('.tank-track');
      if (leftTrack) {
        leftTrack.addEventListener('pointerdown', (event) => {
          setTankSideFromPointer('left', event, leftTrack, leftSlider);
        });
        leftTrack.addEventListener('pointermove', (event) => {
          const pointers = tankPointerState();
          if (pointers.left !== event.pointerId) return;
          setTankSideFromPointer('left', event, leftTrack, leftSlider);
        });
        leftTrack.addEventListener('pointerup', (event) => releaseTankSide('left', event, leftSlider));
        leftTrack.addEventListener('pointercancel', (event) => releaseTankSide('left', event, leftSlider));
        leftTrack.addEventListener('lostpointercapture', (event) => releaseTankSide('left', event, leftSlider));
      }
    }
    if (rightSlider) {
      rightSlider.addEventListener('input', updateTankVisuals);
      const rightTrack = rightSlider.closest('.tank-track');
      if (rightTrack) {
        rightTrack.addEventListener('pointerdown', (event) => {
          setTankSideFromPointer('right', event, rightTrack, rightSlider);
        });
        rightTrack.addEventListener('pointermove', (event) => {
          const pointers = tankPointerState();
          if (pointers.right !== event.pointerId) return;
          setTankSideFromPointer('right', event, rightTrack, rightSlider);
        });
        rightTrack.addEventListener('pointerup', (event) => releaseTankSide('right', event, rightSlider));
        rightTrack.addEventListener('pointercancel', (event) => releaseTankSide('right', event, rightSlider));
        rightTrack.addEventListener('lostpointercapture', (event) => releaseTankSide('right', event, rightSlider));
      }
    }

    document.addEventListener('keydown', (event) => {
      if (event.altKey || event.ctrlKey || event.metaKey) return;

      if (event.key === ' ') {
        event.preventDefault();
        if (ui.armButton) ui.armButton.click();
        return;
      }

      const mapping = { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right' };
      const key = mapping[event.key];
      if (!key) return;
      event.preventDefault();
      state.keys[key] = true;
      updateKeyboardTarget();
    });

    document.addEventListener('keyup', (event) => {
      const mapping = { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right' };
      const key = mapping[event.key];
      if (!key) return;
      state.keys[key] = false;
      updateKeyboardTarget();
    });

    const handedSeg = $('handedSeg');
    if (handedSeg) {
      handedSeg.addEventListener('click', (event) => {
        const button = event.target.closest('button');
        if (!button) return;
        setHandedness(button.dataset.handed);
        if (!isPreview) {
          fetch('/api/ui', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ leftHanded: state.handed === 'left' }),
          }).catch(() => {});
        }
      });
    }

    const wifiSaveBtn = $('wifiSaveBtn');
    if (wifiSaveBtn) {
      wifiSaveBtn.addEventListener('click', () => {
        const payload = {
          ssid: $('wifiSsid') ? $('wifiSsid').value.trim() : '',
          pass: $('wifiPassword') ? $('wifiPassword').value : '',
        };

        if (isPreview) {
          const status = $('wifiStatus');
          if (status) status.textContent = payload.ssid ? `Saved ${payload.ssid}` : 'Saved';
          return;
        }

        fetch('/api/wifi', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload),
        })
          .then((response) => response.json())
          .then((json) => {
            const status = $('wifiStatus');
            if (status) {
              status.textContent = json && json.ok ? 'Wi-Fi settings saved.' : 'Save failed.';
            }
          })
          .catch(() => {
            const status = $('wifiStatus');
            if (status) status.textContent = 'Save failed.';
          });
      });
    }
  }

  function connectWebSocket() {
    if (isPreview) {
      return;
    }

    const wsUrl = `${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.hostname}:81/`;
    const socket = new WebSocket(wsUrl);
    state.socket = socket;

    socket.onopen = () => {
      state.connected = true;
      const websocketState = $('websocketState');
      if (websocketState) websocketState.textContent = 'Connected';
      send({ type: 'source', source: state.source });
      sendControl();
    };

    socket.onclose = () => {
      state.connected = false;
      const websocketState = $('websocketState');
      if (websocketState) websocketState.textContent = 'Disconnected';
      setTimeout(connectWebSocket, 1200);
    };

    socket.onerror = () => {
      const websocketState = $('websocketState');
      if (websocketState) websocketState.textContent = 'Error';
    };

    socket.onmessage = (event) => {
      try {
        const packet = JSON.parse(event.data);
        if (packet && typeof packet === 'object') {
          updateFromTelemetry(packet);
        }
      } catch (error) {
        console.warn('Telemetry parse failed', error);
      }
    };
  }

  function lockAppHeight() {
    document.documentElement.style.setProperty('--app-height', `${window.innerHeight}px`);
  }

  function loadInfo() {
    if (isPreview) {
      setInfoFromServer({ ...previewInfo, firmware: state.firmware, leftHanded: previewInfo.leftHanded });
      return Promise.resolve(previewInfo);
    }

    return fetch('/api/info')
      .then((response) => response.json())
      .then((json) => {
        setInfoFromServer(json || {});
        return json;
      })
      .catch(() => null);
  }

  function loadUiState() {
    const savedTheme = localStorage.getItem('navvy-theme') || 'dark';
    const savedHanded = localStorage.getItem('navvy-handed') || 'right';

    if (isPreview) {
      syncPreviewValues();
      return Promise.resolve(null);
    }

    setTheme(savedTheme);
    setHandedness(savedHanded);
    return fetch('/api/ui')
      .then((response) => response.json())
      .then((json) => {
        if (json && typeof json.leftHanded === 'boolean') {
          setHandedness(json.leftHanded ? 'left' : 'right');
        }
      })
      .catch(() => {});
  }

  function seedPreviewTelemetry() {
    if (!isPreview) return;
    updateFromTelemetry({
      armed: false,
      source: 1,
      webOk: true,
      steeringUs: 1500,
      throttleUs: 1500,
      batteryV: 22.4,
    });
  }

  function init() {
    lockAppHeight();
    installBindings();
    if (isPreview) {
      syncPreviewValues();
      seedPreviewTelemetry();
      updateTankVisuals();
      requestAnimationFrame(animateJoystick);
      return;
    }

    loadUiState().then(() => {
      setTheme(localStorage.getItem('navvy-theme') || 'dark');
      loadInfo();
      connectWebSocket();
      updateTankVisuals();
      requestAnimationFrame(animateJoystick);
    });
  }

  init();
})();