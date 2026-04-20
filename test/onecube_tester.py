"""
OneCube Serial Protocol Tester – Tkinter GUI
=============================================
Requirements:
    pip install pyserial
"""

import struct
import sys
import time
import threading
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    _root = tk.Tk()
    _root.withdraw()
    messagebox.showerror("Missing dependency",
                         "pyserial not installed.\nRun: pip install pyserial")
    sys.exit(1)

# ─── Protocol constants ───────────────────────────────────────────────────────
HEADER      = bytes([0xAA, 0x55])
FOOTER      = bytes([0x0D, 0x0A])

CMD_PING        = 0x01
CMD_GET_STATUS  = 0x02
CMD_SET_LED     = 0x03
CMD_SET_BEEP    = 0x04
CMD_GET_AI      = 0x05
CMD_LCD_DISPLAY = 0x06
CMD_LCD_CLEAR   = 0x07
CMD_GET_TIME    = 0x08
CMD_SET_BLINK   = 0x09
CMD_STOP_BLINK  = 0x0A
CMD_SET_TIME    = 0x0B
CMD_ERROR       = 0xFF

STATUS_OK      = 0x00
STATUS_ERROR   = 0x01
STATUS_INVALID = 0x02

LED_RED    = 0x01
LED_GREEN  = 0x02
LED_YELLOW = 0x04
LED_BLUE   = 0x08
LED_ALL    = 0x0F

STATUS_NAMES = {STATUS_OK: "OK", STATUS_ERROR: "ERROR", STATUS_INVALID: "INVALID"}
CMD_NAMES = {
    CMD_PING: "PING", CMD_GET_STATUS: "GET_STATUS", CMD_SET_LED: "SET_LED",
    CMD_SET_BEEP: "SET_BEEP", CMD_GET_AI: "GET_AI", CMD_LCD_DISPLAY: "LCD_DISPLAY",
    CMD_LCD_CLEAR: "LCD_CLEAR", CMD_GET_TIME: "GET_TIME",
    CMD_SET_BLINK: "SET_BLINK", CMD_STOP_BLINK: "STOP_BLINK", CMD_ERROR: "ERROR",
}

# ─── Frame helpers ────────────────────────────────────────────────────────────

def checksum(length: int, command: int, payload: bytes) -> int:
    cs = length ^ command
    for b in payload:
        cs ^= b
    return cs & 0xFF


def build_frame(cmd: int, payload: bytes = b"") -> bytes:
    ln = len(payload)
    cs = checksum(ln, cmd, payload)
    return HEADER + bytes([ln, cmd]) + payload + bytes([cs]) + FOOTER


def parse_frame(data: bytes):
    """Parse raw bytes into (cmd, payload) or raise ValueError."""
    if len(data) < 7:
        raise ValueError(f"Frame too short ({len(data)} bytes)")
    if data[0:2] != HEADER:
        raise ValueError(f"Bad header: {data[0:2].hex()}")
    if data[-2:] != FOOTER:
        raise ValueError(f"Bad footer: {data[-2:].hex()}")
    ln  = data[2]
    cmd = data[3]
    if len(data) != 7 + ln:
        raise ValueError(f"Length mismatch: expected {7+ln}, got {len(data)}")
    payload = data[4 : 4 + ln]
    cs_recv = data[4 + ln]
    cs_calc = checksum(ln, cmd, payload)
    if cs_recv != cs_calc:
        raise ValueError(f"Checksum mismatch: recv={cs_recv:02X} calc={cs_calc:02X}")
    return cmd, payload


# ─── OneCube client ───────────────────────────────────────────────────────────

class OneCubeClient:
    def __init__(self, port: str, baud: int = 115200, timeout: float = 1.0):
        self.ser = serial.Serial(port, baud, timeout=timeout)
        time.sleep(2)          # wait for Arduino reset after DTR toggle
        self.ser.reset_input_buffer()

    def close(self):
        self.ser.close()

    def _send_recv(self, cmd: int, payload: bytes = b"") -> tuple[int, bytes]:
        """Send a frame and receive the ACK. Returns (cmd, payload)."""
        frame = build_frame(cmd, payload)
        self.ser.reset_input_buffer()
        self.ser.write(frame)

        # Read response bytes with a simple timeout loop
        buf = bytearray()
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            chunk = self.ser.read(self.ser.in_waiting or 1)
            if chunk:
                buf.extend(chunk)
                # Minimum valid frame is 7 bytes; stop when we have header + length + enough
                if len(buf) >= 4 and len(buf) >= 7 + buf[2]:
                    break
        return parse_frame(bytes(buf))

    # ── Command wrappers ──────────────────────────────────────────────────────

    def ping(self) -> bool:
        _, pl = self._send_recv(CMD_PING)
        return pl[0] == STATUS_OK if pl else False

    def get_status(self) -> dict:
        _, pl = self._send_recv(CMD_GET_STATUS)
        if not pl or pl[0] != STATUS_OK or len(pl) < 7:
            raise RuntimeError(f"GET_STATUS failed: {pl.hex() if pl else 'empty'}")
        uptime = struct.unpack(">I", pl[3:7])[0]
        return {
            "status":   STATUS_NAMES.get(pl[0], f"0x{pl[0]:02X}"),
            "fw_ver":   pl[1],
            "led_mask": pl[2],          # raw integer bitmask
            "uptime_s": uptime,
        }

    def set_led(self, mask: int, on: bool) -> bool:
        _, pl = self._send_recv(CMD_SET_LED, bytes([mask, 0x01 if on else 0x00]))
        return pl[0] == STATUS_OK if pl else False

    def set_blink(self, mask: int, on_ms: int, off_ms: int, count: int = 0) -> bool:
        payload = struct.pack(">BHHB", mask, on_ms, off_ms, count)
        _, pl = self._send_recv(CMD_SET_BLINK, payload)
        return pl[0] == STATUS_OK if pl else False

    def stop_blink(self, mask: int) -> bool:
        _, pl = self._send_recv(CMD_STOP_BLINK, bytes([mask]))
        return pl[0] == STATUS_OK if pl else False

    def set_beep(self, freq: int, on_ms: int, off_ms: int, count: int) -> bool:
        payload = struct.pack(">HHHB", freq, on_ms, off_ms, count)
        _, pl = self._send_recv(CMD_SET_BEEP, payload)
        return pl[0] == STATUS_OK if pl else False

    def stop_beep(self) -> bool:
        _, pl = self._send_recv(CMD_SET_BEEP, bytes([0x00]))
        return pl[0] == STATUS_OK if pl else False

    def get_analog(self, pin: int) -> int:
        if not 0 <= pin <= 5:
            raise ValueError("pin must be 0–5")
        _, pl = self._send_recv(CMD_GET_AI, bytes([pin]))
        if not pl or pl[0] != STATUS_OK or len(pl) < 3:
            raise RuntimeError(f"GET_AI A{pin} failed")
        return struct.unpack(">H", pl[1:3])[0]

    def lcd_display(self, row: int, col: int, text: str) -> bool:
        encoded = text.encode("ascii", errors="replace")[:16]
        payload = bytes([row & 0x01, col & 0x0F, len(encoded)]) + encoded
        _, pl = self._send_recv(CMD_LCD_DISPLAY, payload)
        return pl[0] == STATUS_OK if pl else False

    def lcd_clear(self) -> bool:
        _, pl = self._send_recv(CMD_LCD_CLEAR)
        return pl[0] == STATUS_OK if pl else False

    def get_time(self) -> int:
        _, pl = self._send_recv(CMD_GET_TIME)
        if not pl or pl[0] != STATUS_OK or len(pl) < 5:
            raise RuntimeError("GET_TIME failed")
        return struct.unpack(">I", pl[1:5])[0]

    def set_time(self, epoch_seconds: int) -> bool:
        payload = struct.pack(">I", int(epoch_seconds))
        _, pl = self._send_recv(CMD_SET_TIME, payload)
        return pl[0] == STATUS_OK if pl else False

    def stop_time(self) -> bool:
        _, pl = self._send_recv(CMD_SET_TIME, bytes([0x00]))
        return pl[0] == STATUS_OK if pl else False


# ─── MIDI-like melody (buzzer note sequence) ──────────────────────────────────
# Each entry: (freq_hz, on_ms, off_ms)  – approx "Ode to Joy" first 8 bars
MELODY_NOTES = [
    (330, 200, 50), (330, 200, 50), (349, 200, 50), (392, 200, 50),
    (392, 200, 50), (349, 200, 50), (330, 200, 50), (294, 200, 50),
    (262, 200, 50), (262, 200, 50), (294, 200, 50), (330, 200, 50),
    (330, 300, 80), (294, 100, 50),
    (294, 400, 50),
    (330, 200, 50), (330, 200, 50), (349, 200, 50), (392, 200, 50),
    (392, 200, 50), (349, 200, 50), (330, 200, 50), (294, 200, 50),
    (262, 200, 50), (262, 200, 50), (294, 200, 50), (330, 200, 50),
    (294, 300, 80), (262, 100, 50),
    (262, 400, 50),
]

# ─── Tkinter GUI ──────────────────────────────────────────────────────────────

PAD = 6  # standard padding


class App(tk.Tk):
    # LED indicator colours: dim (off) and bright (on)
    LED_DIM    = {"r": "#4a0000", "g": "#004a00", "y": "#4a4a00", "b": "#00004a"}
    LED_BRIGHT = {"r": "#ff4444", "g": "#44ff44", "y": "#ffff44", "b": "#4488ff"}
    LED_BITS   = {"r": LED_RED,   "g": LED_GREEN,  "y": LED_YELLOW, "b": LED_BLUE}

    # Marquee LED sequence: iterate through R→G→Y→B→all→off
    _MARQUEE_SEQ = [LED_RED, LED_GREEN, LED_YELLOW, LED_BLUE,
                    LED_RED | LED_GREEN, LED_GREEN | LED_YELLOW,
                    LED_YELLOW | LED_BLUE, LED_BLUE | LED_RED, LED_ALL]

    def __init__(self):
        super().__init__()
        self.title("OneCube Tester")
        self.minsize(980, 640)
        self.client: OneCubeClient | None = None
        self._cmd_widgets: list[tk.Widget] = []   # toggled on connect/disconnect
        self._clock_running = False   # LCD clock ticker
        self._marquee_running = False
        self._build_ui()
        self._set_connected(False)
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ── UI construction ───────────────────────────────────────────────────────

    def _build_ui(self):
        self._build_connection_bar()

        # Main notebook: 功能区 | 测试区
        self.notebook = ttk.Notebook(self)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=PAD, pady=(0, PAD))

        func_tab = ttk.Frame(self.notebook)
        test_tab = ttk.Frame(self.notebook)
        self.notebook.add(func_tab, text="  功能区  ")
        self.notebook.add(test_tab, text="  测试区  ")

        self._build_func_tab(func_tab)
        self._build_test_tab(test_tab)

    # ── Connection bar ────────────────────────────────────────────────────────

    def _build_connection_bar(self):
        bar = ttk.Frame(self, relief="solid", borderwidth=1)
        bar.pack(fill=tk.X, padx=PAD, pady=PAD)

        ttk.Label(bar, text="Port:").pack(side=tk.LEFT, padx=(PAD, 2))
        self.port_var = tk.StringVar()
        self.port_cb  = ttk.Combobox(bar, textvariable=self.port_var, width=12)
        self.port_cb.pack(side=tk.LEFT, padx=(0, 2))
        ttk.Button(bar, text="⟳", width=3, command=self._refresh_ports).pack(side=tk.LEFT, padx=(0, PAD))

        ttk.Label(bar, text="Baud:").pack(side=tk.LEFT, padx=(0, 2))
        self.baud_var = tk.StringVar(value="115200")
        ttk.Entry(bar, textvariable=self.baud_var, width=8).pack(side=tk.LEFT, padx=(0, PAD))

        self.conn_btn = ttk.Button(bar, text="Connect", command=self._toggle_connect)
        self.conn_btn.pack(side=tk.LEFT, padx=(0, PAD))

        self.status_var = tk.StringVar(value="● Disconnected")
        self.status_lbl = ttk.Label(bar, textvariable=self.status_var, foreground="red")
        self.status_lbl.pack(side=tk.LEFT, padx=(0, PAD))

        # LED state indicators
        ttk.Separator(bar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=PAD)
        ttk.Label(bar, text="LEDs:").pack(side=tk.LEFT, padx=(0, 4))
        self._led_canvas: dict[str, tuple[tk.Canvas, int]] = {}
        for key, label in [("r", "R"), ("g", "G"), ("y", "Y"), ("b", "B")]:
            c = tk.Canvas(bar, width=18, height=18, highlightthickness=0,
                          bg=self.cget("bg"))
            oval = c.create_oval(2, 2, 16, 16, fill=self.LED_DIM[key], outline="")
            c.pack(side=tk.LEFT, padx=1)
            ttk.Label(bar, text=label).pack(side=tk.LEFT, padx=(0, 6))
            self._led_canvas[key] = (c, oval)

        self._refresh_ports()

    # ══════════════════════════════════════════════════════════════════════════
    # 功能区 tab
    # ══════════════════════════════════════════════════════════════════════════

    def _build_func_tab(self, parent):
        paned = tk.PanedWindow(parent, orient=tk.HORIZONTAL, sashwidth=5,
                               bg=self.cget("bg"))
        paned.pack(fill=tk.BOTH, expand=True, padx=PAD, pady=PAD)

        left = ttk.Frame(paned, width=380)
        left.pack_propagate(False)
        paned.add(left, minsize=320)

        right = ttk.Frame(paned)
        paned.add(right, minsize=340)

        self._build_func_panels(left)
        self._build_log_panel(right)

    def _build_func_panels(self, parent):
        # ── LCD Clock ──────────────────────────────────────────────────────
        lf = ttk.LabelFrame(parent, text="LCD 时钟显示", padding=PAD)
        lf.pack(fill=tk.X, padx=PAD, pady=(PAD, 0))

        info = ttk.Label(lf, text="实时时间同步并每秒刷新至 1602 LCD",
                         foreground="#555555")
        info.pack(anchor="w")

        # PC clock preview
        self._pc_clock_var = tk.StringVar(value="--:--:--")
        ttk.Label(lf, textvariable=self._pc_clock_var,
                  font=("Consolas", 22, "bold"), foreground="#0066cc").pack(pady=(4, 0))

        btn_row = ttk.Frame(lf)
        btn_row.pack(fill=tk.X, pady=(6, 0))
        self._clock_btn = self._cmd_btn(btn_row, "▶  一键对时  启动时钟", self._toggle_clock)
        self._clock_btn.pack(side=tk.LEFT, padx=(0, 8))
        ttk.Label(btn_row, text="← 同步当前电脑时间到 LCD",
                  foreground="#666666").pack(side=tk.LEFT)

        # start PC clock preview ticker (always runs, just shows local time)
        self._tick_pc_clock()

        # ── Marquee ────────────────────────────────────────────────────────
        lf2 = ttk.LabelFrame(parent, text="跑马灯", padding=PAD)
        lf2.pack(fill=tk.X, padx=PAD, pady=(PAD, 0))

        ttk.Label(lf2, text="依次点亮 R→G→Y→B→组合→全亮，间隔可调",
                  foreground="#555555").pack(anchor="w")

        cfg_row = ttk.Frame(lf2)
        cfg_row.pack(fill=tk.X, pady=(4, 0))
        ttk.Label(cfg_row, text="间隔 ms:").pack(side=tk.LEFT)
        self._marquee_interval = tk.StringVar(value="200")
        e = ttk.Entry(cfg_row, textvariable=self._marquee_interval, width=6)
        e.pack(side=tk.LEFT, padx=(4, 0))
        self._cmd_widgets.append(e)

        btn_row2 = ttk.Frame(lf2)
        btn_row2.pack(fill=tk.X, pady=(6, 0))
        self._marquee_btn = self._cmd_btn(btn_row2, "▶  一键开启跑马灯", self._toggle_marquee)
        self._marquee_btn.pack(side=tk.LEFT)

        # ── MIDI Melody ────────────────────────────────────────────────────
        lf3 = ttk.LabelFrame(parent, text="MIDI 旋律播放 (~3s)", padding=PAD)
        lf3.pack(fill=tk.X, padx=PAD, pady=(PAD, 0))

        ttk.Label(lf3, text="通过蜂鸣器播放「欢乐颂」片段（约3秒）",
                  foreground="#555555").pack(anchor="w")

        btn_row3 = ttk.Frame(lf3)
        btn_row3.pack(fill=tk.X, pady=(6, 0))
        self._cmd_btn(btn_row3, "♪  播放旋律", self._do_play_melody).pack(side=tk.LEFT, padx=(0, 8))
        self._cmd_btn(btn_row3, "■  停止",     self._do_stop_beep  ).pack(side=tk.LEFT)

    # ══════════════════════════════════════════════════════════════════════════
    # 测试区 tab
    # ══════════════════════════════════════════════════════════════════════════

    def _build_test_tab(self, parent):
        paned = tk.PanedWindow(parent, orient=tk.HORIZONTAL, sashwidth=5,
                               bg=self.cget("bg"))
        paned.pack(fill=tk.BOTH, expand=True, padx=PAD, pady=PAD)

        left = ttk.Frame(paned, width=380)
        left.pack_propagate(False)
        paned.add(left, minsize=320)

        right = ttk.Frame(paned)
        paned.add(right, minsize=340)

        self._build_test_panels(left)
        self._build_test_log_panel(right)

    def _build_test_panels(self, parent):
        canvas = tk.Canvas(parent, highlightthickness=0)
        sb = ttk.Scrollbar(parent, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=sb.set)
        sb.pack(side=tk.RIGHT, fill=tk.Y)
        canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        inner = ttk.Frame(canvas)
        win_id = canvas.create_window((0, 0), window=inner, anchor="nw")

        canvas.bind("<Configure>",
                    lambda e: canvas.itemconfig(win_id, width=e.width))
        inner.bind("<Configure>",
                   lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.bind_all("<MouseWheel>",
                        lambda e: canvas.yview_scroll(-1 * (e.delta // 120), "units"))

        self._build_basic_section(inner)
        self._build_analog_section(inner)
        self._build_led_section(inner)
        self._build_blink_section(inner)
        self._build_beep_section(inner)
        self._build_lcd_section(inner)
        self._build_autotest_section(inner)

    def _section(self, parent, title) -> ttk.LabelFrame:
        lf = ttk.LabelFrame(parent, text=title, padding=PAD)
        lf.pack(fill=tk.X, padx=PAD, pady=(PAD, 0))
        return lf

    def _cmd_btn(self, parent, text, command, **kw) -> ttk.Button:
        btn = ttk.Button(parent, text=text, command=command, **kw)
        self._cmd_widgets.append(btn)
        return btn

    def _build_basic_section(self, parent):
        lf = self._section(parent, "Basic")
        row = ttk.Frame(lf)
        row.pack(fill=tk.X)
        self._cmd_btn(row, "PING",       self._do_ping      ).pack(side=tk.LEFT, padx=2, pady=2)
        self._cmd_btn(row, "GET_STATUS", self._do_get_status).pack(side=tk.LEFT, padx=2, pady=2)
        self._cmd_btn(row, "GET_TIME",   self._do_get_time  ).pack(side=tk.LEFT, padx=2, pady=2)

    def _build_analog_section(self, parent):
        lf = self._section(parent, "Analog Input")
        row = ttk.Frame(lf)
        row.pack(fill=tk.X)
        ttk.Label(row, text="Pin A:").pack(side=tk.LEFT)
        self.analog_pin = tk.StringVar(value="0")
        cb = ttk.Combobox(row, textvariable=self.analog_pin,
                          values=[str(i) for i in range(6)], width=4, state="readonly")
        cb.pack(side=tk.LEFT, padx=(4, 8))
        self._cmd_widgets.append(cb)
        self._cmd_btn(row, "Read", self._do_get_analog).pack(side=tk.LEFT)
        self.analog_result = ttk.Label(lf, text="—", foreground="#0066cc")
        self.analog_result.pack(anchor="w", pady=(2, 0))

    def _build_led_section(self, parent):
        lf = self._section(parent, "LED Control")
        check_row = ttk.Frame(lf)
        check_row.pack(fill=tk.X)
        self.led_vars: dict[str, tk.BooleanVar] = {}
        for key, label, color in [("r", "Red",    "#cc0000"),
                                   ("g", "Green",  "#008800"),
                                   ("y", "Yellow", "#aaaa00"),
                                   ("b", "Blue",   "#0000cc")]:
            v = tk.BooleanVar(value=True)
            self.led_vars[key] = v
            cb = tk.Checkbutton(check_row, text=label, variable=v,
                                fg=color, selectcolor="black", activeforeground=color)
            cb.pack(side=tk.LEFT, padx=2)
            self._cmd_widgets.append(cb)
        btn_row = ttk.Frame(lf)
        btn_row.pack(fill=tk.X, pady=(4, 0))
        self._cmd_btn(btn_row, "Turn ON",  lambda: self._do_set_led(True) ).pack(side=tk.LEFT, padx=2)
        self._cmd_btn(btn_row, "Turn OFF", lambda: self._do_set_led(False)).pack(side=tk.LEFT, padx=2)

    def _build_blink_section(self, parent):
        lf = self._section(parent, "Blink")
        check_row = ttk.Frame(lf)
        check_row.pack(fill=tk.X)
        self.blink_led_vars: dict[str, tk.BooleanVar] = {}
        for key, label, color in [("r", "Red",    "#cc0000"),
                                   ("g", "Green",  "#008800"),
                                   ("y", "Yellow", "#aaaa00"),
                                   ("b", "Blue",   "#0000cc")]:
            v = tk.BooleanVar(value=False)
            self.blink_led_vars[key] = v
            cb = tk.Checkbutton(check_row, text=label, variable=v,
                                fg=color, selectcolor="black", activeforeground=color)
            cb.pack(side=tk.LEFT, padx=2)
            self._cmd_widgets.append(cb)

        grid = ttk.Frame(lf)
        grid.pack(fill=tk.X, pady=4)
        self.blink_on  = tk.StringVar(value="500")
        self.blink_off = tk.StringVar(value="500")
        self.blink_cnt = tk.StringVar(value="0")
        for col, (lbl, var) in enumerate([("ON ms:",  self.blink_on),
                                           ("OFF ms:", self.blink_off),
                                           ("Count:",  self.blink_cnt)]):
            ttk.Label(grid, text=lbl).grid(row=0, column=col * 2,     sticky="e", padx=(4, 0))
            e = ttk.Entry(grid, textvariable=var, width=7)
            e.grid(         row=0, column=col * 2 + 1, padx=(2, 4))
            self._cmd_widgets.append(e)

        btn_row = ttk.Frame(lf)
        btn_row.pack(fill=tk.X)
        self._cmd_btn(btn_row, "Start Blink", self._do_set_blink ).pack(side=tk.LEFT, padx=2)
        self._cmd_btn(btn_row, "Stop Blink",  self._do_stop_blink).pack(side=tk.LEFT, padx=2)

    def _build_beep_section(self, parent):
        lf = self._section(parent, "Beep")
        grid = ttk.Frame(lf)
        grid.pack(fill=tk.X, pady=4)
        self.beep_freq  = tk.StringVar(value="1000")
        self.beep_on    = tk.StringVar(value="200")
        self.beep_off   = tk.StringVar(value="200")
        self.beep_count = tk.StringVar(value="3")
        for col, (lbl, var) in enumerate([("Hz:",     self.beep_freq),
                                           ("ON ms:",  self.beep_on),
                                           ("OFF ms:", self.beep_off),
                                           ("Count:",  self.beep_count)]):
            ttk.Label(grid, text=lbl).grid(row=0, column=col * 2,     sticky="e", padx=(4, 0))
            e = ttk.Entry(grid, textvariable=var, width=7)
            e.grid(         row=0, column=col * 2 + 1, padx=(2, 4))
            self._cmd_widgets.append(e)
        btn_row = ttk.Frame(lf)
        btn_row.pack(fill=tk.X)
        self._cmd_btn(btn_row, "Beep",      self._do_set_beep ).pack(side=tk.LEFT, padx=2)
        self._cmd_btn(btn_row, "Stop Beep", self._do_stop_beep).pack(side=tk.LEFT, padx=2)

    def _build_lcd_section(self, parent):
        lf = self._section(parent, "LCD Display")
        row1 = ttk.Frame(lf)
        row1.pack(fill=tk.X, pady=2)
        ttk.Label(row1, text="Row:").pack(side=tk.LEFT)
        self.lcd_row = tk.StringVar(value="0")
        cb = ttk.Combobox(row1, textvariable=self.lcd_row,
                          values=["0", "1"], width=3, state="readonly")
        cb.pack(side=tk.LEFT, padx=(4, 10))
        self._cmd_widgets.append(cb)
        ttk.Label(row1, text="Col:").pack(side=tk.LEFT)
        self.lcd_col = tk.StringVar(value="0")
        e = ttk.Entry(row1, textvariable=self.lcd_col, width=4)
        e.pack(side=tk.LEFT, padx=(4, 0))
        self._cmd_widgets.append(e)

        row2 = ttk.Frame(lf)
        row2.pack(fill=tk.X, pady=2)
        ttk.Label(row2, text="Text:").pack(side=tk.LEFT)
        self.lcd_text = tk.StringVar()
        e2 = ttk.Entry(row2, textvariable=self.lcd_text)
        e2.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(4, 0))
        self._cmd_widgets.append(e2)

        btn_row = ttk.Frame(lf)
        btn_row.pack(fill=tk.X, pady=(4, 0))
        self._cmd_btn(btn_row, "Display", self._do_lcd_display).pack(side=tk.LEFT, padx=2)
        self._cmd_btn(btn_row, "Clear",   self._do_lcd_clear  ).pack(side=tk.LEFT, padx=2)

    def _build_autotest_section(self, parent):
        lf = self._section(parent, "Auto Test Suite")
        self._cmd_btn(lf, "▶  Run Auto Tests", self._do_auto_tests, width=24).pack(pady=4)

    # ── Log panel shared by 功能区 (right pane) ───────────────────────────────

    def _build_log_panel(self, parent):
        ttk.Label(parent, text="Output Log").pack(anchor="w", padx=PAD)
        self.log = scrolledtext.ScrolledText(
            parent, state="disabled", wrap=tk.WORD,
            font=("Consolas", 9), bg="#1e1e1e", fg="#d4d4d4",
            insertbackground="white")
        self.log.pack(fill=tk.BOTH, expand=True, padx=PAD, pady=(2, 2))
        self.log.tag_config("ok",   foreground="#4ec9b0")
        self.log.tag_config("fail", foreground="#f44747")
        self.log.tag_config("info", foreground="#9cdcfe")
        self.log.tag_config("sep",  foreground="#569cd6")

        ttk.Button(parent, text="Clear Log", command=self._clear_log).pack(
            anchor="e", padx=PAD, pady=(0, PAD))

    # ── Log panel for 测试区 (separate widget, same style) ────────────────────

    def _build_test_log_panel(self, parent):
        ttk.Label(parent, text="Output Log").pack(anchor="w", padx=PAD)
        self.test_log = scrolledtext.ScrolledText(
            parent, state="disabled", wrap=tk.WORD,
            font=("Consolas", 9), bg="#1e1e1e", fg="#d4d4d4",
            insertbackground="white")
        self.test_log.pack(fill=tk.BOTH, expand=True, padx=PAD, pady=(2, 2))
        self.test_log.tag_config("ok",   foreground="#4ec9b0")
        self.test_log.tag_config("fail", foreground="#f44747")
        self.test_log.tag_config("info", foreground="#9cdcfe")
        self.test_log.tag_config("sep",  foreground="#569cd6")

        ttk.Button(parent, text="Clear Log",
                   command=self._clear_test_log).pack(
            anchor="e", padx=PAD, pady=(0, PAD))

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_cb["values"] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def _log(self, msg: str, tag: str = ""):
        """Thread-safe log append – goes to 功能区 log."""
        self.after(0, self._log_append, self.log, msg, tag)

    def _tlog(self, msg: str, tag: str = ""):
        """Thread-safe log append – goes to 测试区 log."""
        self.after(0, self._log_append, self.test_log, msg, tag)

    def _log_append(self, widget: scrolledtext.ScrolledText, msg: str, tag: str):
        widget.configure(state="normal")
        widget.insert(tk.END, msg + "\n", tag if tag else ())
        widget.see(tk.END)
        widget.configure(state="disabled")

    def _clear_log(self):
        self.log.configure(state="normal")
        self.log.delete("1.0", tk.END)
        self.log.configure(state="disabled")

    def _clear_test_log(self):
        self.test_log.configure(state="normal")
        self.test_log.delete("1.0", tk.END)
        self.test_log.configure(state="disabled")

    def _set_connected(self, connected: bool):
        state = "normal" if connected else "disabled"
        for w in self._cmd_widgets:
            try:
                w.configure(state=state)
            except tk.TclError:
                pass
        if connected:
            self.conn_btn.configure(text="Disconnect")
            self.status_var.set("● Connected")
            self.status_lbl.configure(foreground="#009900")
        else:
            self.conn_btn.configure(text="Connect")
            self.status_var.set("● Disconnected")
            self.status_lbl.configure(foreground="red")
            self._clock_running = False
            self._marquee_running = False
            self._update_led_indicators(0)

    def _update_led_indicators(self, mask: int):
        for key, bit in self.LED_BITS.items():
            canvas, oval = self._led_canvas[key]
            color = self.LED_BRIGHT[key] if (mask & bit) else self.LED_DIM[key]
            canvas.itemconfig(oval, fill=color)

    def _get_led_mask(self, var_dict: dict) -> int:
        mask = 0
        for key, bit in self.LED_BITS.items():
            if var_dict[key].get():
                mask |= bit
        return mask

    def _run(self, fn):
        """Run fn in a daemon thread so the UI stays responsive."""
        threading.Thread(target=fn, daemon=True).start()

    # ── PC clock ticker (always running, no device needed) ────────────────────

    def _tick_pc_clock(self):
        now = time.strftime("%H:%M:%S")
        self._pc_clock_var.set(now)
        self.after(500, self._tick_pc_clock)

    # ── Connection ────────────────────────────────────────────────────────────

    def _toggle_connect(self):
        if self.client:
            self._disconnect()
        else:
            self._run(self._connect)

    def _connect(self):
        port = self.port_var.get().strip()
        if not port:
            self.after(0, messagebox.showerror, "Error", "No port selected.")
            return
        try:
            baud = int(self.baud_var.get())
        except ValueError:
            self.after(0, messagebox.showerror, "Error", "Invalid baud rate.")
            return
        self._log(f"Connecting to {port} @ {baud} baud …", "info")
        try:
            self.client = OneCubeClient(port, baud)
            self.after(0, self._set_connected, True)
            self._log(f"Connected to {port}.", "ok")
        except Exception as exc:
            self.client = None
            self._log(f"Connection failed: {exc}", "fail")

    def _disconnect(self):
        self._clock_running = False
        self._marquee_running = False
        if self.client:
            try:
                self.client.close()
            except Exception:
                pass
            self.client = None
        self.after(0, self._set_connected, False)
        self._log("Disconnected.", "info")

    def _on_close(self):
        self._disconnect()
        self.destroy()

    # ── 功能区: LCD Clock ─────────────────────────────────────────────────────

    def _toggle_clock(self):
        if self._clock_running:
            self._clock_running = False
            self._clock_btn.configure(text="▶  一键对时  启动时钟")
            try:
                self.client.stop_time()
            except Exception:
                pass
            self._log("[CLOCK] 时钟已停止", "info")
        else:
            self._clock_running = True
            self._clock_btn.configure(text="■  停止时钟")
            self._log("[CLOCK] 已启动 LCD 时钟同步", "ok")
            # One-shot: send current PC epoch to device as calibration.
            def do_sync():
                try:
                    epoch = int(time.time())
                    ok = self.client.set_time(epoch)
                    if ok:
                        self._log(f"[CLOCK] 对时成功 → {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(epoch))}", "ok")
                    else:
                        self._log("[CLOCK] 对时失败", "fail")
                except Exception as exc:
                    self._log(f"[CLOCK] ERROR: {exc}", "fail")
            self._run(do_sync)

    def _clock_loop(self):
        # Clear LCD once to remove any old uptime/test content
        try:
            self.client.lcd_clear()
        except Exception:
            pass
        while self._clock_running and self.client:
            now      = time.localtime()
            # Pad to 16 chars so displayText() overwrites the entire LCD row,
            # erasing any leftover characters from previous content.
            date_str = time.strftime("%Y-%m-%d", now).ljust(16)
            time_str = time.strftime("%H:%M:%S", now).ljust(16)
            try:
                self.client.lcd_display(0, 0, date_str)
                self.client.lcd_display(1, 0, time_str)
            except Exception as exc:
                self._log(f"[CLOCK] ERROR: {exc}", "fail")
                self._clock_running = False
                self.after(0, self._clock_btn.configure,
                           {"text": "▶  一键对时  启动时钟"})
                break
            # Sleep to the next exact second boundary to avoid drift / skipped seconds
            sleep_for = 1.0 - (time.time() % 1.0)
            if sleep_for < 0.05:   # guard: don't re-enter too early at a second boundary
                sleep_for += 1.0
            time.sleep(sleep_for)

    # ── 功能区: Marquee ───────────────────────────────────────────────────────

    def _toggle_marquee(self):
        if self._marquee_running:
            self._marquee_running = False
            self._marquee_btn.configure(text="▶  一键开启跑马灯")
            self._log("[MARQUEE] 跑马灯已停止", "info")
            self._run(lambda: self.client and self.client.set_led(LED_ALL, False))
        else:
            try:
                interval_ms = int(self._marquee_interval.get())
            except ValueError:
                self._log("[MARQUEE] 间隔值无效", "fail")
                return
            self._marquee_running = True
            self._marquee_btn.configure(text="■  停止跑马灯")
            self._log(f"[MARQUEE] 跑马灯已启动 (间隔 {interval_ms} ms)", "ok")
            self._run(lambda ms=interval_ms: self._marquee_loop(ms))

    def _marquee_loop(self, interval_ms: int):
        seq = self._MARQUEE_SEQ
        idx = 0
        while self._marquee_running and self.client:
            mask = seq[idx % len(seq)]
            try:
                self.client.set_led(LED_ALL, False)
                self.client.set_led(mask, True)
                self.after(0, self._update_led_indicators, mask)
            except Exception as exc:
                self._log(f"[MARQUEE] ERROR: {exc}", "fail")
                self._marquee_running = False
                self.after(0, self._marquee_btn.configure,
                           {"text": "▶  一键开启跑马灯"})
                break
            idx += 1
            time.sleep(interval_ms / 1000.0)
        # clean up LEDs when stopped
        if self.client:
            try:
                self.client.set_led(LED_ALL, False)
                self.after(0, self._update_led_indicators, 0)
            except Exception:
                pass

    # ── 功能区: MIDI Melody ───────────────────────────────────────────────────

    def _do_play_melody(self):
        def fn():
            self._log("[MELODY] 开始播放「欢乐颂」…", "info")
            try:
                for freq, on_ms, off_ms in MELODY_NOTES:
                    if not self.client:
                        break
                    self.client.set_beep(freq, on_ms, off_ms, 1)
                    time.sleep((on_ms + off_ms) / 1000.0)
                self._log("[MELODY] 播放完毕", "ok")
            except Exception as exc:
                self._log(f"[MELODY] ERROR: {exc}", "fail")
        self._run(fn)

    # ── 测试区 command handlers ───────────────────────────────────────────────

    def _do_ping(self):
        def fn():
            try:
                ok = self.client.ping()
                self._tlog(f"[PING] → {'PONG (OK)' if ok else 'No response'}",
                           "ok" if ok else "fail")
            except Exception as exc:
                self._tlog(f"[PING] ERROR: {exc}", "fail")
        self._run(fn)

    def _do_get_status(self):
        def fn():
            try:
                s = self.client.get_status()
                self._tlog(
                    f"[GET_STATUS] fw_ver={s['fw_ver']}  "
                    f"led_mask=0x{s['led_mask']:02X}  "
                    f"uptime={s['uptime_s']}s  status={s['status']}", "ok")
                self.after(0, self._update_led_indicators, s["led_mask"])
            except Exception as exc:
                self._tlog(f"[GET_STATUS] ERROR: {exc}", "fail")
        self._run(fn)

    def _do_get_time(self):
        def fn():
            try:
                t = self.client.get_time()
                self._tlog(f"[GET_TIME] Uptime: {t} s  ({t // 60}m {t % 60}s)", "ok")
            except Exception as exc:
                self._tlog(f"[GET_TIME] ERROR: {exc}", "fail")
        self._run(fn)

    def _do_get_analog(self):
        def fn():
            pin = int(self.analog_pin.get())
            try:
                val = self.client.get_analog(pin)
                msg = f"A{pin} = {val}  ({val / 1023 * 100:.1f}%  ~{val * 5.0 / 1023:.3f} V @5V)"
                self._tlog(f"[GET_ANALOG] {msg}", "ok")
                self.after(0, lambda m=msg: self.analog_result.configure(text=m))
            except Exception as exc:
                self._tlog(f"[GET_ANALOG] ERROR: {exc}", "fail")
        self._run(fn)

    def _do_set_led(self, on: bool):
        mask = self._get_led_mask(self.led_vars)
        if not mask:
            self._tlog("[SET_LED] No LED selected.", "fail")
            return
        def fn():
            try:
                ok = self.client.set_led(mask, on)
                self._tlog(
                    f"[SET_LED] mask=0x{mask:02X} {'ON' if on else 'OFF'} → {'OK' if ok else 'FAIL'}",
                    "ok" if ok else "fail")
            except Exception as exc:
                self._tlog(f"[SET_LED] ERROR: {exc}", "fail")
        self._run(fn)

    def _do_set_blink(self):
        mask = self._get_led_mask(self.blink_led_vars)
        if not mask:
            self._tlog("[SET_BLINK] No LED selected.", "fail")
            return
        try:
            on_ms  = int(self.blink_on.get())
            off_ms = int(self.blink_off.get())
            count  = int(self.blink_cnt.get())
        except ValueError as exc:
            self._tlog(f"[SET_BLINK] Invalid input: {exc}", "fail")
            return
        def fn():
            try:
                ok = self.client.set_blink(mask, on_ms, off_ms, count)
                self._tlog(
                    f"[SET_BLINK] mask=0x{mask:02X}  {on_ms}/{off_ms} ms  "
                    f"×{count if count else '∞'} → {'OK' if ok else 'FAIL'}",
                    "ok" if ok else "fail")
            except Exception as exc:
                self._tlog(f"[SET_BLINK] ERROR: {exc}", "fail")
        self._run(fn)

    def _do_stop_blink(self):
        mask = self._get_led_mask(self.blink_led_vars)
        if not mask:
            self._tlog("[STOP_BLINK] No LED selected.", "fail")
            return
        def fn():
            try:
                ok = self.client.stop_blink(mask)
                self._tlog(f"[STOP_BLINK] mask=0x{mask:02X} → {'OK' if ok else 'FAIL'}",
                           "ok" if ok else "fail")
            except Exception as exc:
                self._tlog(f"[STOP_BLINK] ERROR: {exc}", "fail")
        self._run(fn)

    def _do_set_beep(self):
        try:
            freq   = int(self.beep_freq.get())
            on_ms  = int(self.beep_on.get())
            off_ms = int(self.beep_off.get())
            count  = int(self.beep_count.get())
        except ValueError as exc:
            self._tlog(f"[SET_BEEP] Invalid input: {exc}", "fail")
            return
        def fn():
            try:
                ok = self.client.set_beep(freq, on_ms, off_ms, count)
                self._tlog(
                    f"[SET_BEEP] {freq} Hz  {on_ms}/{off_ms} ms  ×{count} → {'OK' if ok else 'FAIL'}",
                    "ok" if ok else "fail")
            except Exception as exc:
                self._tlog(f"[SET_BEEP] ERROR: {exc}", "fail")
        self._run(fn)

    def _do_stop_beep(self):
        def fn():
            try:
                ok = self.client.stop_beep()
                # log to whichever tab makes sense – both contexts use stop
                self._log(f"[STOP_BEEP] → {'OK' if ok else 'FAIL'}", "ok" if ok else "fail")
                self._tlog(f"[STOP_BEEP] → {'OK' if ok else 'FAIL'}", "ok" if ok else "fail")
            except Exception as exc:
                self._log(f"[STOP_BEEP] ERROR: {exc}", "fail")
        self._run(fn)

    def _do_lcd_display(self):
        try:
            row = int(self.lcd_row.get())
            col = int(self.lcd_col.get())
        except ValueError as exc:
            self._tlog(f"[LCD_DISPLAY] Invalid row/col: {exc}", "fail")
            return
        text = self.lcd_text.get()
        def fn():
            try:
                ok = self.client.lcd_display(row, col, text)
                self._tlog(f"[LCD_DISPLAY] ({row},{col}) \"{text}\" → {'OK' if ok else 'FAIL'}",
                           "ok" if ok else "fail")
            except Exception as exc:
                self._tlog(f"[LCD_DISPLAY] ERROR: {exc}", "fail")
        self._run(fn)

    def _do_lcd_clear(self):
        def fn():
            try:
                ok = self.client.lcd_clear()
                self._tlog(f"[LCD_CLEAR] → {'OK' if ok else 'FAIL'}", "ok" if ok else "fail")
            except Exception as exc:
                self._tlog(f"[LCD_CLEAR] ERROR: {exc}", "fail")
        self._run(fn)

    def _do_auto_tests(self):
        def fn():
            c = self.client
            results: list[bool] = []
            self._tlog("── Auto Test Suite ──────────────────────────────────────", "sep")

            def test(name, thefn):
                try:
                    result = thefn()
                    ok = bool(result) if not isinstance(result, dict) else True
                    extra = f" → {result}" if result not in (True, False) else ""
                    self._tlog(f"  [{'PASS' if ok else 'FAIL'}] {name}{extra}",
                               "ok" if ok else "fail")
                    results.append(ok)
                except Exception as exc:
                    self._tlog(f"  [FAIL] {name} → {exc}", "fail")
                    results.append(False)

            test("PING",                    c.ping)
            test("GET_STATUS",              c.get_status)
            test("GET_TIME",                lambda: f"{c.get_time()} s")
            test("GET_ANALOG A0",           lambda: f"{c.get_analog(0)} (0–1023)")
            test("SET_LED Red ON",          lambda: c.set_led(LED_RED, True))
            time.sleep(0.5)
            test("SET_LED Red OFF",         lambda: c.set_led(LED_RED, False))
            test("SET_LED All ON",          lambda: c.set_led(LED_ALL, True))
            time.sleep(0.5)
            test("SET_LED All OFF",         lambda: c.set_led(LED_ALL, False))
            test("SET_BLINK Green 500/500", lambda: c.set_blink(LED_GREEN, 500, 500, 4))
            time.sleep(4.5)
            test("STOP_BLINK Green",        lambda: c.stop_blink(LED_GREEN))
            test("SET_BEEP 1kHz 3x",        lambda: c.set_beep(1000, 200, 200, 3))
            time.sleep(1.5)
            test("STOP_BEEP",               c.stop_beep)
            test("LCD_DISPLAY row0",        lambda: c.lcd_display(0, 0, "OneCube Test"))
            test("LCD_DISPLAY row1",        lambda: c.lcd_display(1, 0, "Python Tester"))
            time.sleep(1.0)
            test("LCD_CLEAR",               c.lcd_clear)

            passed = sum(results)
            total  = len(results)
            self._tlog(f"\n  Result: {passed}/{total} passed",
                       "ok" if passed == total else "fail")
            self._tlog("─────────────────────────────────────────────────────────", "sep")

        self._run(fn)


# ─── Entry point ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    app = App()
    app.mainloop()

