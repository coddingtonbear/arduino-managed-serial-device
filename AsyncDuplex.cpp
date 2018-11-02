#pragma once

#include <Arduino.h>
#include <Regexp.h>

#include "AsyncDuplex.h"

AsyncDuplex::AsyncDuplex(Stream* _stream): stream(_stream) {
}

bool AsyncDuplex::asyncExecute(
    const char *_command,
    const char *_expectation,
    AsyncTiming _timing,
    std::function<void(MatchState)> _success,
    std::function<void()> _failure,
    uint16_t _timeout
) {
    uint8_t position = 0;
    if(_timing == ANY) {
        position = queueLength;
        queueLength++;
    } else {
        shiftRight();
        queueLength++;
    }

    strcpy(commandQueue[position].command, _command);
    strcpy(commandQueue[position].expectation, _expectation);
    commandQueue[position].success = _success;
    commandQueue[position].failure = _failure;
    commandQueue[position].timeout = _timeout;

    return true
}

void AsyncDuplex::loop(){
    while(stream->available()) {
        inputBuffer[++bufferPos] = stream->read();
        inputBuffer[bufferPos] = '\0';

        if(processing) {
            if(timeout > millis()) {
                std::function<void()> fn = commandQueue[0].failure;
                if(fn) {
                    fn();
                }
                shiftLeft();
                inputBuffer[0] = '\0';
            }

            MatchState ms;
            ms.Target(inputBuffer);

            char result = ms.Match(commandQueue[0].expectation);
            if(result) {
                processing=false;

                std::function<void(MatchState)> fn = commandQueue[0].success;
                shiftLeft();
                if(fn) {
                    fn(ms);
                }
                uint16_t offset = ms.MatchStart + ms.MatchLength;
                for(uint16_t i = offset; i < INPUT_BUFFER_LENGTH; i++) {
                    inputBuffer[i - offset] = inputBuffer[i];
                    // If we reached the end of the capture, we
                    // do not need to copy anything further
                    if(inputBuffer[i] == '\0') {
                        break;
                    }
                }
            }
        }
    }
    if(!processing && queueLength > 0) {
        stream->println(commandQueue[0].command);
        processing = true;
        timeout = millis() + commandQueue[0].timeout;
    }
}

void AsyncDuplex::getResponse(char* buffer, uint16_t length) {
    strncpy(buffer, inputBuffer, length);
}

void AsyncDuplex::shiftRight() {
    for(int8_t i = 0; i < queueLength - 1; i++) {
        strcpy(commandQueue[i+1].command, commandQueue[i].command);
        strcpy(commandQueue[i+1].expectation, commandQueue[i].expectation);
        commandQueue[i+1].success = commandQueue[i].success;
        commandQueue[i+1].failure = commandQueue[i].failure;
        commandQueue[i-1].timeout = commandQueue[i].timeout;
    }
    queueLength++;
}

void AsyncDuplex::shiftLeft() {
    for(int8_t i = queueLength - 1; i > 0; i--) {
        strcpy(commandQueue[i-1].command, commandQueue[i].command);
        strcpy(commandQueue[i-1].expectation, commandQueue[i].expectation);
        commandQueue[i-1].success = commandQueue[i].success;
        commandQueue[i-1].failure = commandQueue[i].failure;
        commandQueue[i+1].timeout = commandQueue[i].timeout;
    }
    queueLength--;
}
