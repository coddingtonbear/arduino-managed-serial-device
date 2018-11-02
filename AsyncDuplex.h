#pragma once

#include <Arduino.h>
#include <functional>

#define COMMAND_QUEUE_SIZE 10
#define INPUT_BUFFER_LENGTH 512
#define MAX_COMMAND_LENGTH 128
#define MAX_EXPECTATION_LENGTH 128
#define COMMAND_TIMEOUT 2500

typedef enum {
    NEXT,
    ANY
} AsyncTiming;

class AsyncDuplex {
    public:
        AsyncDuplex(Stream*);

        bool asyncExecute(
            const char *_command,
            const char *_expectation,
            AsyncTiming _timing = ANY,
            std::function<void(MatchState)> _success = NULL,
            std::function<void()> _failure = NULL,
            uint16_t _timeout = COMMAND_TIMEOUT
        );

        void loop();

        void getResponse(char*, uint16_t);

    private:
        struct AsyncDuplexCommand {
            char command[MAX_COMMAND_LENGTH];
            char expectation[MAX_EXPECTATION_LENGTH];
            std::function<void(MatchState)> success;
            std::function<void()> failure;
            uint16_t timeout;
        };

        void shiftRight();
        void shiftLeft();

        AsyncDuplexCommand commandQueue[COMMAND_QUEUE_SIZE];
        char inputBuffer[INPUT_BUFFER_LENGTH];
        uint16_t bufferPos = 0;
        uint16_t timeout = 0;

        bool processing = false;

        Stream* stream;
        uint8_t queueLength = 0;
};
