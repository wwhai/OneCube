#include "SerialProtocolParser.h"

SerialProtocolParser::SerialProtocolParser()
    : rxIndex(0), rxState(0), lastRxTime(0)
{
    memset(rxBuffer, 0, sizeof(rxBuffer));
}

void SerialProtocolParser::init(unsigned long baudRate)
{
    Serial.begin(baudRate);
    resetRxState();
}

// XOR of length, command, and all payload bytes
uint8_t SerialProtocolParser::calculateChecksum(uint8_t length, uint8_t command,
                                                const uint8_t *payload)
{
    uint8_t cs = length ^ command;
    if (payload)
    {
        for (uint8_t i = 0; i < length; i++)
            cs ^= payload[i];
    }
    return cs;
}

bool SerialProtocolParser::validateFrame(ProtocolFrame *frame)
{
    if (frame->header[0] != PROTOCOL_HEADER_1 || frame->header[1] != PROTOCOL_HEADER_2)
        return false;
    if (frame->footer[0] != PROTOCOL_FOOTER_1 || frame->footer[1] != PROTOCOL_FOOTER_2)
        return false;
    if (frame->length > MAX_PAYLOAD_SIZE)
        return false;
    return calculateChecksum(frame->length, frame->command, frame->payload) == frame->checksum;
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
        return false;

    uint8_t checksum = calculateChecksum(length, command, payload);

    Serial.write(PROTOCOL_HEADER_1);
    Serial.write(PROTOCOL_HEADER_2);
    Serial.write(length);
    Serial.write(command);
    if (payload && length > 0)
        Serial.write(payload, length);
    Serial.write(checksum);
    Serial.write(PROTOCOL_FOOTER_1);
    Serial.write(PROTOCOL_FOOTER_2);

    return true;
}

bool SerialProtocolParser::sendAck(uint8_t command, uint8_t status,
                                   uint8_t *data, uint8_t length)
{
    uint8_t payload[MAX_PAYLOAD_SIZE];
    payload[0]    = status;
    uint8_t total = 1;

    if (data && length > 0 && (uint8_t)(length + 1u) <= MAX_PAYLOAD_SIZE)
    {
        memcpy(&payload[1], data, length);
        total = (uint8_t)(total + length);
    }

    return sendFrame(command, payload, total);
}

bool SerialProtocolParser::receiveFrame(ProtocolFrame *frame)
{
    if (!frame)
        return false;

    while (Serial.available() > 0)
    {
        uint8_t b    = (uint8_t)Serial.read();
        lastRxTime   = millis();

        switch (rxState)
        {
        case 0: // Wait for first header byte
            if (b == PROTOCOL_HEADER_1)
            {
                rxBuffer[rxIndex++] = b;
                rxState = 1;
            }
            break;

        case 1: // Wait for second header byte
            if (b == PROTOCOL_HEADER_2)
            {
                rxBuffer[rxIndex++] = b;
                rxState = 2;
            }
            else
                resetRxState();
            break;

        case 2: // Length byte
            if (b <= MAX_PAYLOAD_SIZE)
            {
                rxBuffer[rxIndex++] = b;
                rxState = 3;
            }
            else
                resetRxState();
            break;

        case 3: // Command byte – jump over payload state if length is 0
            rxBuffer[rxIndex++] = b;
            rxState = (rxBuffer[2] == 0u) ? 5 : 4;
            break;

        case 4: // Payload bytes
            rxBuffer[rxIndex++] = b;
            if (rxIndex >= (uint8_t)(4u + rxBuffer[2]))
                rxState = 5;
            break;

        case 5: // Checksum byte
            rxBuffer[rxIndex++] = b;
            rxState = 6;
            break;

        case 6: // First footer byte
            if (b == PROTOCOL_FOOTER_1)
            {
                rxBuffer[rxIndex++] = b;
                rxState = 7;
            }
            else
                resetRxState();
            break;

        case 7: // Second footer byte
            if (b == PROTOCOL_FOOTER_2)
            {
                rxBuffer[rxIndex++] = b;

                // Unpack
                frame->header[0] = rxBuffer[0];
                frame->header[1] = rxBuffer[1];
                frame->length    = rxBuffer[2];
                frame->command   = rxBuffer[3];
                memcpy(frame->payload, &rxBuffer[4], frame->length);
                frame->checksum  = rxBuffer[4 + frame->length];
                frame->footer[0] = rxBuffer[5 + frame->length];
                frame->footer[1] = rxBuffer[6 + frame->length];

                resetRxState();

                if (validateFrame(frame))
                    return true;
            }
            else
                resetRxState();
            break;

        default:
            resetRxState();
            break;
        }

        if (rxIndex >= sizeof(rxBuffer))
            resetRxState();
    }

    // Discard partial frame on timeout
    if (rxState > 0 && (millis() - lastRxTime) > RX_TIMEOUT_MS)
        resetRxState();

    return false;
}


