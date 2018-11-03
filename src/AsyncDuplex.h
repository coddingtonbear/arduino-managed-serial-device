#pragma once

#include <Arduino.h>
#include <Regexp.h>
#include <functional>

#define COMMAND_QUEUE_SIZE 3
#define INPUT_BUFFER_LENGTH 256
#define MAX_COMMAND_LENGTH 64
#define MAX_EXPECTATION_LENGTH 128
#define COMMAND_TIMEOUT 2500

//#define ASYNC_DUPLEX_DEBUG

class AsyncDuplex: public Stream {
    public:
        enum Timing{
            NEXT,
            ANY
        };
        struct Command {
            char command[MAX_COMMAND_LENGTH];
            char expectation[MAX_EXPECTATION_LENGTH];
            std::function<void(MatchState)> success;
            std::function<void(Command*)> failure;
            uint16_t timeout;
            uint32_t delay;

            Command();
            Command(
                const char* _cmd,
                const char* _expect,
                std::function<void(MatchState)> _success = NULL,
                std::function<void(Command*)> _failure = NULL,
                uint16_t _timeout = COMMAND_TIMEOUT,
                uint32_t _delay = 0
            );
        };

        AsyncDuplex();

        void begin(Stream*);
        bool asyncExecute(
            const char *_command,
            const char *_expectation = "",
            Timing _timing = Timing::ANY,
            std::function<void(MatchState)> _success = NULL,
            std::function<void(Command*)> _failure = NULL,
            uint16_t _timeout = COMMAND_TIMEOUT,
            uint32_t _delay = 0
        );
        bool asyncExecute(
            const Command*,
            Timing _timing = Timing::ANY
        );
        bool asyncExecuteChain(
            Command*,
            uint16_t count,
            Timing _timing = Timing::ANY
        );

        void loop();

        uint8_t getQueueLength();
        void getResponse(char*, uint16_t);

        // Stream
        int available();
        size_t write(uint8_t);
        int read();
        int peek();
        void flush();
    private:
        Command commandQueue[COMMAND_QUEUE_SIZE];

        void shiftRight();
        void shiftLeft();

        void copyCommand(Command*, const Command*);
        void createChain(Command*, const Command*);

        char inputBuffer[INPUT_BUFFER_LENGTH];
        uint16_t bufferPos = 0;
        uint16_t timeout = 0;

        bool began = false;
        bool processing = false;

        Stream* stream;
        uint8_t queueLength = 0;
};
