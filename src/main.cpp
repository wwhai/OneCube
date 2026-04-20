#include <Arduino.h>
#include "LEDControl.h"
#include "BeepControl.h"
#include "LCDControl.h"
#include "SerialProtocolParser.h"
#include <stdio.h>

// ─── Pin assignments ──────────────────────────────────────────────────────────
static const int RED_PIN    = 2;
static const int GREEN_PIN  = 3;
static const int YELLOW_PIN = 5;
static const int BLUE_PIN   = 4;
static const int BEEP_PIN   = 6;

// ─── Module instances ─────────────────────────────────────────────────────────
static LEDControl           led(RED_PIN, GREEN_PIN, YELLOW_PIN, BLUE_PIN);
static BeepControl          beep(BEEP_PIN);
static LCDControl           lcdCtrl(0x27);
static SerialProtocolParser proto;

// ─── LCD host-control tracking ─────────────────────────────────────────────────
// When the host sends a display command the LCD is "host-owned" for
// HOST_LCD_TIMEOUT_MS.  After that, the device returns to auto-uptime display.
static const unsigned long HOST_LCD_TIMEOUT_MS = 30000UL;
static bool          hostLcdActive = false;
static unsigned long lastLcdCmdMs  = 0;
static unsigned long lastUptimeMs  = 0;

// ─── Device clock (set by host; device counts locally after calibration) ───
static bool          deviceClockMode   = false;
static uint32_t      clockBaseSeconds  = 0;
static unsigned long clockBaseMillis   = 0;
// Temporary LCD message (used to show errors / confirmations for a few seconds)
static unsigned long lcdTempUntilMs = 0;
static char lcdTempLine0[17] = "";
static char lcdTempLine1[17] = "";

// ─── Forward declarations ─────────────────────────────────────────────────────
static void bootSequence();
static void handleFrame(const ProtocolFrame &frame);
static uint32_t getDeviceTimeSeconds();

// ═════════════════════════════════════════════════════════════════════════════
void setup()
{
    led.init();
    beep.init();
    lcdCtrl.init();
    proto.init(115200);
    bootSequence();
}

void loop()
{
    // 1. Non-blocking hardware updates
    led.update();
    beep.update();

    // 2. Process one incoming frame per cycle
    ProtocolFrame frame;
    if (proto.receiveFrame(&frame))
        handleFrame(frame);

    // 3. Return LCD to auto-uptime when host has been silent long enough
    if (hostLcdActive && (millis() - lastLcdCmdMs) >= HOST_LCD_TIMEOUT_MS)
        hostLcdActive = false;

    // 4. Refresh uptime/clock display every second (when host is not controlling LCD)
    // If a temporary message is active, leave it on-screen until expiry.
    if (lcdTempUntilMs > millis())
    {
        // keep temporary message displayed (do nothing)
    }
    else if (!hostLcdActive && (millis() - lastUptimeMs) >= 1000UL)
    {
        lastUptimeMs = millis();
        if (deviceClockMode)
            lcdCtrl.displayClock(getDeviceTimeSeconds());
        else
            lcdCtrl.displayUptime();
    }
}

// Return the current device clock seconds.  If the host has calibrated
// the clock (`deviceClockMode`), return the calibrated epoch seconds +
// elapsed since calibration; otherwise fall back to uptime seconds.
static uint32_t getDeviceTimeSeconds()
{
    if (deviceClockMode)
    {
        unsigned long delta = (millis() - clockBaseMillis) / 1000UL;
        return (uint32_t)(clockBaseSeconds + delta);
    }
    return (uint32_t)(millis() / 1000UL);
}

// ─── Boot animation (blocking – runs once before loop) ────────────────────────
static void bootSequence()
{
    lcdCtrl.displayLine(0, "  OneCube v1.0  ");
    lcdCtrl.displayLine(1, "  Initializing  ");

    const uint8_t masks[4] = {
        LED_RED_MASK, LED_GREEN_MASK, LED_YELLOW_MASK, LED_BLUE_MASK
    };
    for (int i = 0; i < 4; i++)
    {
        led.setLed(masks[i], true);
        delay(150);
        led.setLed(masks[i], false);
    }

    // Boot chime (direct tone() – blocking is fine during setup)
    tone(BEEP_PIN, 1000, 150);
    delay(250);
    tone(BEEP_PIN, 1500, 100);
    delay(200);
    noTone(BEEP_PIN);
    pinMode(BEEP_PIN, OUTPUT);   // restore after noTone

    lcdCtrl.displayLine(1, "    Ready!      ");
    delay(800);
    lcdCtrl.clearDisplay();
    // Auto-uptime display will appear within the first loop tick
}

// ─── Command dispatcher ────────────────────────────────────────────────────────
static void handleFrame(const ProtocolFrame &frame)
{
    const uint8_t *p   = frame.payload;
    const uint8_t  len = frame.length;

    switch (frame.command)
    {
    // ── PING ──────────────────────────────────────────────────────────────────
    case CMD_PING:
        proto.sendAck(CMD_PING, STATUS_OK);
        break;

    // ── GET STATUS ────────────────────────────────────────────────────────────
    // Response: [status, fw_ver(1), led_mask(1), uptime_secs(4 BE)]
    case CMD_GET_STATUS:
    {
        uint32_t up = millis() / 1000UL;
        uint8_t  d[6];
        d[0] = 0x01;                      // firmware version
        d[1] = led.getState();
        d[2] = (uint8_t)(up >> 24);
        d[3] = (uint8_t)(up >> 16);
        d[4] = (uint8_t)(up >>  8);
        d[5] = (uint8_t)(up);
        proto.sendAck(CMD_GET_STATUS, STATUS_OK, d, 6);
        break;
    }

    // ── SET LED ───────────────────────────────────────────────────────────────
    // Payload: [mask(1), state(1)]   state: 0=OFF, 1=ON
    case CMD_SET_LED:
        if (len < 2) { proto.sendAck(CMD_SET_LED, STATUS_INVALID); break; }
        led.setLed(p[0], p[1] != 0u);
        proto.sendAck(CMD_SET_LED, STATUS_OK);
        break;

    // ── SET BLINK ─────────────────────────────────────────────────────────────
    // Payload: [mask(1), onH, onL, offH, offL, count(1)]   count=0 → infinite
    case CMD_SET_BLINK:
        if (len < 6) { proto.sendAck(CMD_SET_BLINK, STATUS_INVALID); break; }
        led.setBlink(p[0],
                     (uint16_t)((uint16_t)p[1] << 8 | p[2]),
                     (uint16_t)((uint16_t)p[3] << 8 | p[4]),
                     p[5]);
        proto.sendAck(CMD_SET_BLINK, STATUS_OK);
        break;

    // ── STOP BLINK ────────────────────────────────────────────────────────────
    // Payload: [mask(1)]
    case CMD_STOP_BLINK:
        if (len < 1) { proto.sendAck(CMD_STOP_BLINK, STATUS_INVALID); break; }
        led.stopBlink(p[0]);
        proto.sendAck(CMD_STOP_BLINK, STATUS_OK);
        break;

    // ── SET BEEP ──────────────────────────────────────────────────────────────
    // Payload: [fH, fL, onH, onL, offH, offL, count]
    // Send [0x00] (1-byte) to stop.  count must be >= 1.
    case CMD_SET_BEEP:
    {
        if (len < 1) { proto.sendAck(CMD_SET_BEEP, STATUS_INVALID); break; }
        if (len == 1 && p[0] == 0u)
        {
            beep.stop();
        }
        else if (len >= 7)
        {
            uint16_t f   = (uint16_t)((uint16_t)p[0] << 8 | p[1]);
            uint16_t on  = (uint16_t)((uint16_t)p[2] << 8 | p[3]);
            uint16_t off = (uint16_t)((uint16_t)p[4] << 8 | p[5]);
            uint8_t  cnt = p[6];
            if (cnt == 0u)
                beep.stop();
            else
                beep.beepPattern(f, on, off, cnt);
        }
        proto.sendAck(CMD_SET_BEEP, STATUS_OK);
        break;
    }

    // ── GET ANALOG INPUT ──────────────────────────────────────────────────────
    // Payload: [pin(1)]  pin 0–5 → A0–A5
    // Response: [status, valH, valL]  (0–1023)
    case CMD_GET_AI:
    {
        if (len < 1 || p[0] > 5u)
        {
            proto.sendAck(CMD_GET_AI, STATUS_INVALID);
            break;
        }
        uint16_t val   = (uint16_t)analogRead((int)(A0 + p[0]));
        uint8_t  d[2]  = { (uint8_t)(val >> 8), (uint8_t)(val) };
        proto.sendAck(CMD_GET_AI, STATUS_OK, d, 2);
        break;
    }

    // ── LCD DISPLAY ───────────────────────────────────────────────────────────
    // Payload: [row(1), col(1), len(1), text…]
    case CMD_LCD_DISPLAY:
    {
        if (len < 3) { proto.sendAck(CMD_LCD_DISPLAY, STATUS_INVALID); break; }
        uint8_t row  = p[0];
        uint8_t col  = p[1];
        uint8_t tlen = p[2];
        if (tlen > (len - 3u))
            tlen = (uint8_t)(len - 3u);
        char text[17];
        memcpy(text, &p[3], tlen);
        text[tlen] = '\0';
        lcdCtrl.displayText(row, col, text);
        hostLcdActive = true;
        lastLcdCmdMs  = millis();
        proto.sendAck(CMD_LCD_DISPLAY, STATUS_OK);
        break;
    }

    // ── LCD CLEAR ─────────────────────────────────────────────────────────────
    case CMD_LCD_CLEAR:
        lcdCtrl.clearDisplay();
        hostLcdActive = true;
        lastLcdCmdMs  = millis();
        proto.sendAck(CMD_LCD_CLEAR, STATUS_OK);
        break;

    // ── GET TIME (uptime) ─────────────────────────────────────────────────────
    // Response: [status, uptime_secs(4 BE)]
    case CMD_GET_TIME:
    {
        uint32_t up  = millis() / 1000UL;
        uint8_t  d[4] = {
            (uint8_t)(up >> 24), (uint8_t)(up >> 16),
            (uint8_t)(up >>  8), (uint8_t)(up)
        };
        proto.sendAck(CMD_GET_TIME, STATUS_OK, d, 4);
        break;
    }

    // ── SET TIME (host calibration) ──────────────────────────────────────────
    // Payload: [t3,t2,t1,t0]  4-byte big-endian epoch seconds
    // Payload: [0x00] (1-byte) → disable device clock mode
    case CMD_SET_TIME:
    {
        if (len == 1 && p[0] == 0u)
        {
            deviceClockMode = false;
            proto.sendAck(CMD_SET_TIME, STATUS_OK);
            break;
        }
        if (len < 4)
        {
            proto.sendAck(CMD_SET_TIME, STATUS_INVALID);
            // Show temporary error on LCD for a few seconds
            snprintf(lcdTempLine0, sizeof(lcdTempLine0), "  OneCube v1.0  ");
            snprintf(lcdTempLine1, sizeof(lcdTempLine1), "  Time Sync Err  ");
            lcdCtrl.displayLine(0, lcdTempLine0);
            lcdCtrl.displayLine(1, lcdTempLine1);
            lcdTempUntilMs = millis() + 3000UL;
            break;
        }

        uint32_t secs = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                        ((uint32_t)p[2] << 8)  | ((uint32_t)p[3]);
        clockBaseSeconds = secs;
        clockBaseMillis  = millis();
        deviceClockMode  = true;
        proto.sendAck(CMD_SET_TIME, STATUS_OK);

        // Briefly show confirmation
        snprintf(lcdTempLine0, sizeof(lcdTempLine0), "  OneCube v1.0  ");
        snprintf(lcdTempLine1, sizeof(lcdTempLine1), "   Time Set OK   ");
        lcdCtrl.displayLine(0, lcdTempLine0);
        lcdCtrl.displayLine(1, lcdTempLine1);
        lcdTempUntilMs = millis() + 2000UL;
        break;
    }

    // ── UNKNOWN ───────────────────────────────────────────────────────────────
    default:
    {
        uint8_t errCmd = frame.command;
        proto.sendAck(CMD_ERROR, STATUS_INVALID, &errCmd, 1);
        break;
    }
    }
}

