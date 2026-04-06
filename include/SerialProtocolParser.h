// SerialProtocolParser.h
//
// Binary serial protocol for OneCube host ↔ device communication.
//
// Frame layout:
//   [AA][55][LEN][CMD][PAYLOAD × LEN][CHKSUM][0D][0A]
//
// CHKSUM = XOR of LEN, CMD, and all PAYLOAD bytes.
//
// ─── Commands (host → device) ─────────────────────────────────────────────────
//  CMD  | Name           | Payload                                  | Response payload
//  0x01   CMD_PING          (none)                                    [status]
//  0x02   CMD_GET_STATUS    (none)                                    [status, fw_ver, led_mask, uptime×4]
//  0x03   CMD_SET_LED       [mask, state]  state: 0=OFF 1=ON         [status]
//  0x04   CMD_SET_BEEP      [fH,fL, onH,onL, offH,offL, count]       [status]
//                            count=0 → stop beep
//  0x05   CMD_GET_AI        [pin]  pin 0‒5 → A0‒A5                   [status, valH, valL]
//  0x06   CMD_LCD_DISPLAY   [row, col, len, text…]                    [status]
//  0x07   CMD_LCD_CLEAR     (none)                                    [status]
//  0x08   CMD_GET_TIME      (none)                                    [status, uptime×4]
//  0x09   CMD_SET_BLINK     [mask, onH,onL, offH,offL, count]         [status]
//                            count=0 → infinite
//  0x0A   CMD_STOP_BLINK    [mask]                                    [status]
//  0xFF   CMD_ERROR         [err_cmd]  device→host error notice
// ─────────────────────────────────────────────────────────────────────────────

#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <Arduino.h>

// Frame constants
#define PROTOCOL_HEADER_1  0xAAu
#define PROTOCOL_HEADER_2  0x55u
#define PROTOCOL_FOOTER_1  0x0Du
#define PROTOCOL_FOOTER_2  0x0Au
#define MAX_PAYLOAD_SIZE   32u
#define FRAME_OVERHEAD     7u    // header(2) + len(1) + cmd(1) + chksum(1) + footer(2)
#define RX_TIMEOUT_MS      200u

// Commands
enum ProtocolCommand : uint8_t
{
    CMD_PING        = 0x01,
    CMD_GET_STATUS  = 0x02,
    CMD_SET_LED     = 0x03,
    CMD_SET_BEEP    = 0x04,
    CMD_GET_AI      = 0x05,  // Read Analog Input (A0–A5)
    CMD_LCD_DISPLAY = 0x06,
    CMD_LCD_CLEAR   = 0x07,
    CMD_GET_TIME    = 0x08,
    CMD_SET_BLINK   = 0x09,
    CMD_STOP_BLINK  = 0x0A,
    CMD_ERROR       = 0xFF
};

// Response status (first byte of every ACK payload)
enum ResponseStatus : uint8_t
{
    STATUS_OK      = 0x00,
    STATUS_ERROR   = 0x01,
    STATUS_INVALID = 0x02
};

// Frame structure
struct ProtocolFrame
{
    uint8_t header[2];
    uint8_t length;
    uint8_t command;
    uint8_t payload[MAX_PAYLOAD_SIZE];
    uint8_t checksum;
    uint8_t footer[2];
};

class SerialProtocolParser
{
public:
    SerialProtocolParser();

    void init(unsigned long baudRate = 115200);

    // Call in loop() – returns true when a valid frame is ready
    bool receiveFrame(ProtocolFrame *frame);

    // Send an ACK frame: [status, data…]
    bool sendAck(uint8_t command, uint8_t status,
                 uint8_t *data = nullptr, uint8_t length = 0);

    // Send a raw frame
    bool sendFrame(uint8_t command, uint8_t *payload = nullptr, uint8_t length = 0);

private:
    uint8_t       rxBuffer[MAX_PAYLOAD_SIZE + FRAME_OVERHEAD];
    uint8_t       rxIndex;
    uint8_t       rxState;
    unsigned long lastRxTime;

    uint8_t calculateChecksum(uint8_t length, uint8_t command, const uint8_t *payload);
    bool    validateFrame(ProtocolFrame *frame);
    void    resetRxState();
};

#endif // SERIAL_PROTOCOL_H