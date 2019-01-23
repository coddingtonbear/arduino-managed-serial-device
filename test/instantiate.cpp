#include <Arduino.h>
#include <Regexp.h>
#include <ArduinoUnitTests.h>
#include "../src/ManagedSerialDevice.h"

unittest(simple) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    bool callbackExecuted = false;

    ManagedSerialDevice handler = ManagedSerialDevice();
    handler.begin(&Serial);
    handler.execute("TEST");
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

    ManagedSerialDevice handler = ManagedSerialDevice();
    handler.begin(&Serial);
    handler.execute(
        "TEST",
        "OK",
        ManagedSerialDevice::NEXT,
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

    ManagedSerialDevice handler = ManagedSerialDevice();
    handler.begin(&Serial);
    handler.execute(
        "TEST",
        "OK",
        ManagedSerialDevice::NEXT,
        [&callbackExecuted](MatchState ms) {
            callbackExecuted = true;
        },
        [&failureCallbackExecuted](ManagedSerialDevice::Command* cmd) {
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

unittest(can_fail_expectation_with_display) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    bool callbackExecuted = false;
    bool failureCallbackExecuted = false;

    ManagedSerialDevice handler = ManagedSerialDevice();
    handler.begin(&Serial);
    handler.execute(
        "TEST",
        "OK",
        ManagedSerialDevice::NEXT,
        [&callbackExecuted](MatchState ms) {
            callbackExecuted = true;
        },
        handler.printFailure(&Serial),
        0
    );
    handler.loop();
    state->micros = state->micros + 100000;
    handler.loop();
    assertEqual(
        "TEST\r\nCommand 'TEST' failed.\r\n",
        state->serialPort[0].dataOut
    );
}

unittest(can_execute_from_object) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    ManagedSerialDevice::Command cmd = ManagedSerialDevice::Command(
        "TEST",
        "XYZ"
    );
    ManagedSerialDevice handler = ManagedSerialDevice();
    handler.begin(&Serial);
    handler.execute(&cmd);

    handler.loop();

    assertEqual(
        "TEST\r\n",
        state->serialPort[0].dataOut
    );
}

unittest(can_execute_chain) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    uint8_t appendedCallbackCalls = 0;

    ManagedSerialDevice::Command commands[] = {
        ManagedSerialDevice::Command("TEST", "OK"),
        ManagedSerialDevice::Command("TEST2", "OK"),
        ManagedSerialDevice::Command("TEST3", "OK")
    };
    ManagedSerialDevice handler = ManagedSerialDevice();
    handler.begin(&Serial);
    handler.executeChain(
        commands,
        3,
        ManagedSerialDevice::Timing::ANY,
        [&appendedCallbackCalls](MatchState ms) {
            appendedCallbackCalls++;
        }
    );

    handler.loop();
    assertEqual(
        "TEST\r\n",
        state->serialPort[0].dataOut
    );
    state->serialPort[0].dataIn = "OK";
    state->serialPort[0].dataOut = "";

    handler.loop();
    assertEqual(1, appendedCallbackCalls);
    assertEqual(
        "TEST2\r\n",
        state->serialPort[0].dataOut
    );
    state->serialPort[0].dataIn = "OK";
    state->serialPort[0].dataOut = "";

    handler.loop();
    assertEqual(2, appendedCallbackCalls);
    assertEqual(
        "TEST3\r\n",
        state->serialPort[0].dataOut
    );
    state->serialPort[0].dataIn = "OK";
    state->serialPort[0].dataOut = "";

    handler.loop();
    assertEqual(3, appendedCallbackCalls);
}

unittest(can_return_match_groups) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    char matchGroupResult[10] = {'\0'};

    ManagedSerialDevice handler = ManagedSerialDevice();
    handler.begin(&Serial);
    handler.execute(
        "TEST",
        "OK%[([%d]+)%]",
        ManagedSerialDevice::NEXT,
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

unittest(task_overflows_non_dangerous) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    ManagedSerialDevice handler = ManagedSerialDevice();
    handler.begin(&Serial);

    for(uint8_t i = 0; i < COMMAND_QUEUE_SIZE + 1; i++) {
        bool result = handler.execute("TEST");

        if(i < COMMAND_QUEUE_SIZE) {
            assertTrue(result);
        } else {
            assertFalse(result);
        }
    }

    assertEqual(COMMAND_QUEUE_SIZE, handler.getQueueLength());
}

unittest(can_register_and_run_hooks) {
    GodmodeState* state = GODMODE();
    state->resetPorts();

    bool hookExecuted = false;

    ManagedSerialDevice handler = ManagedSerialDevice();
    handler.begin(&Serial);

    handler.registerHook(
        "%*PSUTTZ(.*)\r\n",
        [&hookExecuted](MatchState ms) {
            hookExecuted = true;
        }
    );

    assertFalse(hookExecuted);

    state->serialPort[0].dataIn = "SOMETHING";
    handler.loop();
    assertFalse(hookExecuted);

    state->serialPort[0].dataIn = "\r\n";
    handler.loop();
    assertFalse(hookExecuted);

    state->serialPort[0].dataIn = "*PSUTTZ: 18/11/04,22:38:07\",\"-32\",0";
    handler.loop();
    assertFalse(hookExecuted);

    state->serialPort[0].dataIn = "\r\n";
    handler.loop();
    assertTrue(hookExecuted);
}

unittest_main()
