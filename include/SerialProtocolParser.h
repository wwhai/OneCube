// SerialProtocolParser.h
#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <Arduino.h>

// Protocol constants
#define PROTOCOL_HEADER_1 0xAA
#define PROTOCOL_HEADER_2 0x55
#define PROTOCOL_FOOTER_1 0x0D
#define PROTOCOL_FOOTER_2 0x0A
#define MAX_PAYLOAD_SIZE 64
#define FRAME_OVERHEAD 7 // 2 header + 1 length + 1 cmd + 1 checksum + 2 footer

// Command definitions
enum ProtocolCommand
{
    CMD_PING = 0x01,
    CMD_GET_STATUS = 0x02,
    CMD_SET_LED = 0x03,
    CMD_SET_BEEP = 0x04,
    CMD_GET_TEMP = 0x05,
    CMD_LCD_DISPLAY = 0x06,
    CMD_GET_TIME = 0x07,
    CMD_ERROR = 0xFF
};

// Frame structure
struct ProtocolFrame
{
    uint8_t header[2];                 // 0xAA, 0x55
    uint8_t length;                    // Payload length (0-64)
    uint8_t command;                   // Command type
    uint8_t payload[MAX_PAYLOAD_SIZE]; // Data payload
    uint8_t checksum;                  // XOR checksum of length + command + payload
    uint8_t footer[2];                 // 0x0D, 0x0A
};

// Response status codes
enum ResponseStatus
{
    STATUS_SUCCESS = 0x00,
    STATUS_ERROR = 0x01,
    STATUS_INVALID = 0x02,
    STATUS_TIMEOUT = 0x03
};

class SerialProtocolParser
{
private:
    uint8_t rxBuffer[MAX_PAYLOAD_SIZE + FRAME_OVERHEAD];
    uint8_t txBuffer[MAX_PAYLOAD_SIZE + FRAME_OVERHEAD];
    uint8_t rxIndex;
    uint8_t rxState;
    unsigned long lastRxTime;

    // Internal methods
    uint8_t calculateChecksum(uint8_t length, uint8_t command, uint8_t *payload);
    bool validateFrame(ProtocolFrame *frame);
    void resetRxState();

public:
    SerialProtocolParser();

    // Initialize protocol
    void init(unsigned long baudRate = 115200);

    // Send methods
    bool sendFrame(uint8_t command, uint8_t *payload = nullptr, uint8_t length = 0);
    bool sendResponse(uint8_t command, uint8_t status, uint8_t *data = nullptr, uint8_t length = 0);
    bool sendPing();
    bool sendError(uint8_t errorCode);

    // Receive methods
    bool receiveFrame(ProtocolFrame *frame);
    bool hasData();

    // Utility methods
    void printFrame(ProtocolFrame *frame);
    void printHex(uint8_t *data, uint8_t length);
};

#endif // SERIAL_PROTOCOL_H