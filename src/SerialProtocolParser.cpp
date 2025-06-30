#include "SerialProtocolParser.h"
// SerialProtocolParser.cpp
SerialProtocolParser::SerialProtocolParser() : rxIndex(0), rxState(0), lastRxTime(0)
{
    memset(rxBuffer, 0, sizeof(rxBuffer));
    memset(txBuffer, 0, sizeof(txBuffer));
}

void SerialProtocolParser::init(unsigned long baudRate)
{
    Serial.begin(baudRate);
    resetRxState();
}

uint8_t SerialProtocolParser::calculateChecksum(uint8_t length, uint8_t command, uint8_t *payload)
{
    uint8_t checksum = length ^ command;
    for (uint8_t i = 0; i < length; i++)
    {
        checksum ^= payload[i];
    }
    return checksum;
}

bool SerialProtocolParser::validateFrame(ProtocolFrame *frame)
{
    // Check headers
    if (frame->header[0] != PROTOCOL_HEADER_1 || frame->header[1] != PROTOCOL_HEADER_2)
    {
        return false;
    }

    // Check footers
    if (frame->footer[0] != PROTOCOL_FOOTER_1 || frame->footer[1] != PROTOCOL_FOOTER_2)
    {
        return false;
    }

    // Check length
    if (frame->length > MAX_PAYLOAD_SIZE)
    {
        return false;
    }

    // Verify checksum
    uint8_t calculatedChecksum = calculateChecksum(frame->length, frame->command, frame->payload);
    return (calculatedChecksum == frame->checksum);
}

void SerialProtocolParser::resetRxState()
{
    rxIndex = 0;
    rxState = 0;
    memset(rxBuffer, 0, sizeof(rxBuffer));
}

bool SerialProtocolParser::sendFrame(uint8_t command, uint8_t *payload, uint8_t length)
{
    if (length > MAX_PAYLOAD_SIZE)
    {
        return false;
    }

    ProtocolFrame frame;

    // Build frame
    frame.header[0] = PROTOCOL_HEADER_1;
    frame.header[1] = PROTOCOL_HEADER_2;
    frame.length = length;
    frame.command = command;

    // Copy payload
    if (payload && length > 0)
    {
        memcpy(frame.payload, payload, length);
    }

    // Calculate checksum
    frame.checksum = calculateChecksum(frame.length, frame.command, frame.payload);

    // Set footer
    frame.footer[0] = PROTOCOL_FOOTER_1;
    frame.footer[1] = PROTOCOL_FOOTER_2;

    // Send frame
    Serial.write(frame.header, 2);
    Serial.write(frame.length);
    Serial.write(frame.command);
    if (length > 0)
    {
        Serial.write(frame.payload, length);
    }
    Serial.write(frame.checksum);
    Serial.write(frame.footer, 2);

    return true;
}

bool SerialProtocolParser::sendResponse(uint8_t command, uint8_t status, uint8_t *data, uint8_t length)
{
    uint8_t payload[MAX_PAYLOAD_SIZE];
    payload[0] = status;

    uint8_t totalLength = 1;
    if (data && length > 0 && length < MAX_PAYLOAD_SIZE)
    {
        memcpy(&payload[1], data, length);
        totalLength += length;
    }

    return sendFrame(command, payload, totalLength);
}

bool SerialProtocolParser::sendPing()
{
    return sendFrame(CMD_PING);
}

bool SerialProtocolParser::sendError(uint8_t errorCode)
{
    return sendFrame(CMD_ERROR, &errorCode, 1);
}

bool SerialProtocolParser::receiveFrame(ProtocolFrame *frame)
{
    if (!frame)
        return false;

    while (Serial.available() > 0)
    {
        uint8_t receivedByte = Serial.read();
        lastRxTime = millis();

        switch (rxState)
        {
        case 0: // Waiting for first header byte
            if (receivedByte == PROTOCOL_HEADER_1)
            {
                rxBuffer[rxIndex++] = receivedByte;
                rxState = 1;
            }
            break;

        case 1: // Waiting for second header byte
            if (receivedByte == PROTOCOL_HEADER_2)
            {
                rxBuffer[rxIndex++] = receivedByte;
                rxState = 2;
            }
            else
            {
                resetRxState();
            }
            break;

        case 2: // Length byte
            if (receivedByte <= MAX_PAYLOAD_SIZE)
            {
                rxBuffer[rxIndex++] = receivedByte;
                rxState = 3;
            }
            else
            {
                resetRxState();
            }
            break;

        case 3: // Command byte
            rxBuffer[rxIndex++] = receivedByte;
            rxState = 4;
            break;

        case 4: // Payload bytes
            rxBuffer[rxIndex++] = receivedByte;
            if (rxIndex >= (4 + rxBuffer[2]))
            { // 4 = header(2) + length(1) + command(1)
                rxState = 5;
            }
            break;

        case 5: // Checksum byte
            rxBuffer[rxIndex++] = receivedByte;
            rxState = 6;
            break;

        case 6: // First footer byte
            if (receivedByte == PROTOCOL_FOOTER_1)
            {
                rxBuffer[rxIndex++] = receivedByte;
                rxState = 7;
            }
            else
            {
                resetRxState();
            }
            break;

        case 7: // Second footer byte
            if (receivedByte == PROTOCOL_FOOTER_2)
            {
                rxBuffer[rxIndex++] = receivedByte;

                // Parse received frame
                frame->header[0] = rxBuffer[0];
                frame->header[1] = rxBuffer[1];
                frame->length = rxBuffer[2];
                frame->command = rxBuffer[3];
                memcpy(frame->payload, &rxBuffer[4], frame->length);
                frame->checksum = rxBuffer[4 + frame->length];
                frame->footer[0] = rxBuffer[5 + frame->length];
                frame->footer[1] = rxBuffer[6 + frame->length];

                resetRxState();

                // Validate frame
                if (validateFrame(frame))
                {
                    return true;
                }
            }
            else
            {
                resetRxState();
            }
            break;

        default:
            resetRxState();
            break;
        }

        // Prevent buffer overflow
        if (rxIndex >= sizeof(rxBuffer))
        {
            resetRxState();
        }
    }

    // Check for timeout
    if (rxState > 0 && (millis() - lastRxTime) > 1000)
    {
        resetRxState();
    }

    return false;
}

bool SerialProtocolParser::hasData()
{
    return Serial.available() > 0;
}

void SerialProtocolParser::printFrame(ProtocolFrame *frame)
{
    Serial.print("Frame: ");
    Serial.print("Header=[0x");
    Serial.print(frame->header[0], HEX);
    Serial.print(",0x");
    Serial.print(frame->header[1], HEX);
    Serial.print("] Len=");
    Serial.print(frame->length);
    Serial.print(" Cmd=0x");
    Serial.print(frame->command, HEX);
    Serial.print(" Payload=[");
    printHex(frame->payload, frame->length);
    Serial.print("] Checksum=0x");
    Serial.print(frame->checksum, HEX);
    Serial.print(" Footer=[0x");
    Serial.print(frame->footer[0], HEX);
    Serial.print(",0x");
    Serial.print(frame->footer[1], HEX);
    Serial.println("]");
}

void SerialProtocolParser::printHex(uint8_t *data, uint8_t length)
{
    for (uint8_t i = 0; i < length; i++)
    {
        if (i > 0)
            Serial.print(",");
        Serial.print("0x");
        if (data[i] < 0x10)
            Serial.print("0");
        Serial.print(data[i], HEX);
    }
}
