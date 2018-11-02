#pragma once

#include <Arduino.h>
#include <Regexp.h>

#include "AsyncDuplex.h"

AsyncDuplex::AsyncDuplex(Stream* _stream): stream(_stream) {
}

bool AsyncDuplex::asyncExecute(
    const char *command,
    const char *expectation,
    AsyncTiming timing,
    std::function<void(MatchState)> function
) {
    uint8_t position = 0;
    if(timing == ANY) {
        position = queueLength;
        queueLength++;
    } else {
        shiftRight();
        queueLength++;
    }

    strcpy(commandQueue[position].command, command);
    strcpy(commandQueue[position].expectation, expectation);
    commandQueue[position].function = function;

    return true
}

void AsyncDuplex::loop(){
    while(stream->available()) {
        inputBuffer[++bufferPos] = stream->read();
        inputBuffer[bufferPos] = '\0';

        if(processing) {
            MatchState ms;
            ms.Target(inputBuffer);

            char result = ms.Match(commandQueue[0].expectation);
            if(result) {
                processing=false;

                std::function<void(MatchState)> fn = commandQueue[0].function;
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
    }
}

void AsyncDuplex::getResponse(char* buffer, uint16_t length) {
}

void AsyncDuplex::shiftRight() {
    for(int8_t i = 0; i < queueLength - 1; i++) {
        strcpy(commandQueue[i+1].command, commandQueue[i].command);
        strcpy(commandQueue[i+1].expectation, commandQueue[i].expectation);
        commandQueue[i+1].function = commandQueue[i].function;
    }
    queueLength++;
}

void AsyncDuplex::shiftLeft() {
    for(int8_t i = queueLength - 1; i > 0; i--) {
        strcpy(commandQueue[i-1].command, commandQueue[i].command);
        strcpy(commandQueue[i-1].expectation, commandQueue[i].expectation);
        commandQueue[i-1].function = commandQueue[i].function;
    }
    queueLength--;
}
