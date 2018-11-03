#include <functional>

#include <Arduino.h>
#undef min
#undef max
#include <Regexp.h>

#include "AsyncDuplex.h"

#ifdef ASYNC_DUPLEX_DEBUG
    #include <iostream>
#endif

AsyncDuplex::Command::Command() {}

AsyncDuplex::Command::Command(
    const char* _cmd,
    const char* _expect,
    std::function<void(MatchState)> _success,
    std::function<void(Command*)> _failure,
    uint16_t _timeout,
    uint32_t _delay
) {
    strcpy(command, _cmd);
    strcpy(expectation, _expect);
    success = _success;
    failure = _failure;
    timeout = _timeout;
    delay = _delay;
}

AsyncDuplex::AsyncDuplex(){}

bool AsyncDuplex::begin(Stream* _stream) {
    stream = _stream;
    began = true;

    // Sublcasses may perform additional actions here that may not
    // be successful; in this base class' case, this isn't variable
    return true;
}

bool AsyncDuplex::wait(uint32_t timeout, std::function<void()> feed_watchdog) {
    uint32_t started = millis();

    while(queueLength > 0) {
        if(timeout && (millis() > started + timeout)) {
            // Wait timeout
            return false;
        }
        if(feed_watchdog) {
            feed_watchdog();
        }
        loop();
    }
    return true;
}

bool AsyncDuplex::asyncExecute(
    const char *_command,
    const char *_expectation,
    AsyncDuplex::Timing _timing,
    std::function<void(MatchState)> _success,
    std::function<void(Command*)> _failure,
    uint16_t _timeout,
    uint32_t _delay
) {
    if(queueLength == COMMAND_QUEUE_SIZE) {
        return false;
    }

    uint8_t position = 0;
    if(_timing == ANY) {
        position = queueLength;
        queueLength++;
    } else {
        shiftRight();
    }

    strcpy(commandQueue[position].command, _command);
    strcpy(commandQueue[position].expectation, _expectation);
    commandQueue[position].success = _success;
    commandQueue[position].failure = _failure;
    commandQueue[position].timeout = _timeout;

    // Once queued, the delay signifies the point in time at
    // which this task can begin being processed
    commandQueue[position].delay = _delay + millis();

    return true;
}

bool AsyncDuplex::asyncExecute(
    const Command* cmd,
    Timing _timing
) {
    return AsyncDuplex::asyncExecute(
        cmd->command,
        cmd->expectation,
        _timing,
        cmd->success,
        cmd->failure,
        cmd->timeout
    );
}

bool AsyncDuplex::asyncExecuteChain(
    const Command* cmdArray,
    uint16_t count,
    Timing _timing,
    std::function<void(MatchState)> _success,
    std::function<void(Command*)> _failure
) {
    if(count < 2) {
        return false;
    }

    Command scratch;
    Command chain = cmdArray[count - 1];
    AsyncDuplex::prependCallback(&chain, _success, _failure);

    for(int16_t i = count - 2; i >= 0; i--) {
        AsyncDuplex::copyCommand(
            &scratch,
            &cmdArray[i]
        );
        AsyncDuplex::prependCallback(&scratch, _success, _failure);
        AsyncDuplex::createChain(
            &scratch,
            &chain
        );
        AsyncDuplex::copyCommand(
            &chain,
            &scratch
        );
    }
    return AsyncDuplex::asyncExecute(
        &chain,
        _timing
    );
}

void AsyncDuplex::createChain(Command* dest, const Command* toChain) {
    Command chained;
    copyCommand(&chained, toChain);

    std::function<void(MatchState)> originalSuccess = dest->success;
    dest->success = [this, chained, originalSuccess](MatchState ms){
        if(originalSuccess) {
            #ifdef ASYNC_DUPLEX_DEBUG
                std::cout << "Executing pre-chain success fn\n";
            #endif
            originalSuccess(ms);
        }
        else {
            #ifdef ASYNC_DUPLEX_DEBUG
                std::cout << "No pre-chain success fn to execute\n";
            #endif
        }
        #ifdef ASYNC_DUPLEX_DEBUG
            std::cout << "Queueing chained command: ";
            std::cout << chained.command;
            std::cout << "\n";
        #endif
        AsyncDuplex::asyncExecute(
            &chained,
            Timing::NEXT
        );
    };
}

void AsyncDuplex::copyCommand(Command* dest, const Command* src) {
    strcpy(dest->command, src->command);
    strcpy(dest->expectation, src->expectation);
    dest->success = src->success;
    dest->failure = src->failure;
    dest->timeout = src->timeout;
    dest->delay = src->delay;
}

void AsyncDuplex::prependCallback(
    Command* cmd, 
    std::function<void(MatchState)> _success,
    std::function<void(Command*)> _failure
) {
    if(_success) {
        #ifdef ASYNC_DUPLEX_DEBUG
            std::cout << "Prepending success callback for '";
            std::cout << cmd->command;
            std::cout << "'\n";
        #endif
        std::function<void(MatchState)> originalFn = cmd->success;
        cmd->success = [_success, originalFn](MatchState ms){
            #ifdef ASYNC_DUPLEX_DEBUG
                std::cout << "Executing success fn.\n";
            #endif
            _success(ms);
            if(originalFn) {
                originalFn(ms);
            }
        };
    }
    if(_failure) {
        #ifdef ASYNC_DUPLEX_DEBUG
            std::cout << "Prepending failure callback to '";
            std::cout << cmd->command;
            std::cout << "'\n";
        #endif
        std::function<void(Command*)> originalFn = cmd->failure;
        cmd->failure = [_failure, originalFn](Command* cmd){
            #ifdef ASYNC_DUPLEX_DEBUG
                std::cout << "Executing failure fn.\n";
            #endif
            _failure(cmd);
            if(originalFn) {
                originalFn(cmd);
            }
        };
    }
}

void AsyncDuplex::loop(){
    if(!began) {
        return;
    }
    if(processing && timeout < millis()) {
        #ifdef ASYNC_DUPLEX_DEBUG
            std::cout << "Command timeout\n";
        #endif

        Command failedCommand;
        AsyncDuplex::copyCommand(&failedCommand, &commandQueue[0]);
        shiftLeft();
        inputBuffer[0] = '\0';
        processing=false;

        std::function<void(Command*)> fn = failedCommand.failure;
        if(fn) {
            // Clear delay settings before handing to error
            // handler callback to prevent erroneously delaying
            // for forty years if the error handler tries to retry
            failedCommand.delay = 0;
            fn(&failedCommand);
        }
    }
    while(stream->available()) {
        inputBuffer[bufferPos++] = stream->read();
        inputBuffer[bufferPos] = '\0';

        #ifdef ASYNC_DUPLEX_DEBUG
            std::cout << "Input buffer (";
            std::cout << String(bufferPos);
            std::cout << ") \"";
            std::cout << inputBuffer;
            std::cout << "\"\n";
        #endif

        if(processing) {
            MatchState ms;
            ms.Target(inputBuffer);

            char result = ms.Match(commandQueue[0].expectation);
            if(result) {
                #ifdef ASYNC_DUPLEX_DEBUG
                    std::cout << "Expectation matched\n";
                #endif

                processing=false;

                std::function<void(MatchState)> fn = commandQueue[0].success;
                shiftLeft();
                if(fn) {
                    fn(ms);
                }
                uint16_t offset = ms.MatchStart + ms.MatchLength;
                for(uint16_t i = offset; i < INPUT_BUFFER_LENGTH; i++) {
                    inputBuffer[i - offset] = inputBuffer[i];
                    bufferPos = i - offset;
                    // If we reached the end of the capture, we
                    // do not need to copy anything further
                    if(inputBuffer[i] == '\0') {
                        break;
                    }
                }
            }
        }
    }
    if(!processing && queueLength > 0 && commandQueue[0].delay <= millis()) {
        #ifdef ASYNC_DUPLEX_DEBUG
            std::cout << "Command started: ";
            std::cout << commandQueue[0].command;
            std::cout << "\n";
        #endif
        stream->println(commandQueue[0].command);
        stream->flush();
        processing = true;
        timeout = millis() + commandQueue[0].timeout;
    }
}

uint8_t AsyncDuplex::getQueueLength() {
    return queueLength;
}

void AsyncDuplex::getResponse(char* buffer, uint16_t length) {
    strncpy(buffer, inputBuffer, length);
}

void AsyncDuplex::shiftRight() {
    for(int8_t i = 0; i < queueLength - 1; i++) {
        AsyncDuplex::copyCommand(&commandQueue[i+1], &commandQueue[i]);
    }
    queueLength++;
}

void AsyncDuplex::shiftLeft() {
    for(int8_t i = queueLength - 1; i > 0; i--) {
        AsyncDuplex::copyCommand(&commandQueue[i-1], &commandQueue[i]);
    }
    queueLength--;
}

std::function<void(AsyncDuplex::Command*)> AsyncDuplex::printFailure(Stream* stream) {
    return [stream](AsyncDuplex::Command* cmd) {
        stream->println(
            "Command '" + String(cmd->command) + "' failed."
        );
    };
}

inline int AsyncDuplex::available() {
    return stream->available();
}

inline size_t AsyncDuplex::write(uint8_t bt) {
    return stream->write(bt);
}

inline int AsyncDuplex::read() {
    return stream->read();
}

inline int AsyncDuplex::peek() {
    return stream->peek();
}

inline void AsyncDuplex::flush() {
    return stream->flush();
}
