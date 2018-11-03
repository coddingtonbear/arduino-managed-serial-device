#include <Arduino.h>
#include <Regexp.h>
#include <ArduinoUnitTests.h>
#include "../src/AsyncDuplex.h"

unittest(simple) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    bool callbackExecuted = false;

    AsyncDuplex handler = AsyncDuplex();
    handler.begin(&Serial);
    handler.asyncExecute("TEST");
    handler.loop();

    assertEqual(
        "TEST\r\n",
        state->serialPort[0].dataOut
    );
}

unittest(can_resolve_expectation) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    bool callbackExecuted = false;

    AsyncDuplex handler = AsyncDuplex();
    handler.begin(&Serial);
    handler.asyncExecute(
        "TEST",
        "OK",
        AsyncDuplex::NEXT,
        [&callbackExecuted](MatchState ms) {
            callbackExecuted = true;
        }
    );
    handler.loop();

    assertEqual(
        "TEST\r\n",
        state->serialPort[0].dataOut
    );
    assertFalse(callbackExecuted);

    state->serialPort[0].dataIn = "OK";
    handler.loop();

    assertTrue(callbackExecuted);
}


unittest(can_fail_expectation) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    bool callbackExecuted = false;
    bool failureCallbackExecuted = false;

    AsyncDuplex handler = AsyncDuplex();
    handler.begin(&Serial);
    handler.asyncExecute(
        "TEST",
        "OK",
        AsyncDuplex::NEXT,
        [&callbackExecuted](MatchState ms) {
            std::cout << "Success fn executed\n";
            callbackExecuted = true;
        },
        [&failureCallbackExecuted](AsyncDuplex::Command* cmd) {
            std::cout << "Failure fn executed\n";
            failureCallbackExecuted = true;
        },
        0
    );
    handler.loop();

    assertEqual(
        "TEST\r\n",
        state->serialPort[0].dataOut
    );

    state->micros = state->micros + 100000;

    handler.loop();
    assertFalse(callbackExecuted);
    assertTrue(failureCallbackExecuted);
}

unittest(can_execute_from_object) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    AsyncDuplex::Command cmd = AsyncDuplex::Command(
        "TEST",
        "XYZ"
    );
    AsyncDuplex handler = AsyncDuplex();
    handler.begin(&Serial);
    handler.asyncExecute(&cmd);

    handler.loop();

    assertEqual(
        "TEST\r\n",
        state->serialPort[0].dataOut
    );
}

unittest(can_execute_chain) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    AsyncDuplex::Command commands[] = {
        AsyncDuplex::Command("TEST", "OK"),
        AsyncDuplex::Command("TEST2", "OK"),
        AsyncDuplex::Command("TEST3", "OK")
    };
    AsyncDuplex handler = AsyncDuplex();
    handler.begin(&Serial);
    handler.asyncExecuteChain(commands, 3);

    handler.loop();
    assertEqual(
        "TEST\r\n",
        state->serialPort[0].dataOut
    );
    state->serialPort[0].dataIn = "OK";
    state->serialPort[0].dataOut = "";

    handler.loop();
    assertEqual(
        "TEST2\r\n",
        state->serialPort[0].dataOut
    );
    state->serialPort[0].dataIn = "OK";
    state->serialPort[0].dataOut = "";

    handler.loop();
    handler.loop();
    assertEqual(
        "TEST3\r\n",
        state->serialPort[0].dataOut
    );
}

unittest(can_return_match_groups) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    char matchGroupResult[10] = {'\0'};

    AsyncDuplex handler = AsyncDuplex();
    handler.begin(&Serial);
    handler.asyncExecute(
        "TEST",
        "OK%[([%d]+)%]",
        AsyncDuplex::NEXT,
        [&matchGroupResult](MatchState ms) {
            char buffer[5];
            ms.GetCapture(buffer, 0);
            strcpy(matchGroupResult, buffer);
        }
    );
    handler.loop();

    assertEqual(
        "TEST\r\n",
        state->serialPort[0].dataOut
    );
    state->serialPort[0].dataIn = "OK[10]";
    handler.loop();

    assertEqual("10", matchGroupResult);
}

unittest_main()
