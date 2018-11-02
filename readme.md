# Arduino Async Duplex

Asynchronously interact with any serial device having a call-and-response
style interface.

## Requirements

* Regexp (https://github.com/nickgammon/Regexp)

## Example

```c++
#include <AsyncDuplex.h>
#include <Regexp.h>

AsyncDuplex handler = AsyncDuplex(&Serial);

void setup() {
    AsyncDuplex.asyncExecute(
        "AT+CIPSTART=\"TCP\",\"mywebsite.com\",\"80\"", // Command
        "OK\r\n",  // Expectation regex
        ANY,
        [](MatchState ms) -> void {
            AsyncDuplex.asyncExecute(
                "AT+CIPSEND",
                ">",
                NEXT,
                [](MatchState ms) -> void {
                    AsyncDuplex.asyncExecute(
                        "abc\r\n\x1a"
                        "SEND OK\r\n"
                        NEXT,
                    );
                }
            )
        }
    );
}


void loop() {
    AsyncDuplex.loop();
}
```

This example is based upon interactions with a SIM7000 LTE modem.

1. Send `AT+CIPSTART...`; wait for `OK` followed by the line ending.
2. Send `AT+CIPSEND...`; wait for a `>` to be printed.  Ensure that
   this is the next command executed.
3. Send the data you want to send followed y CTRL+Z (`\x1a`).  Ensure
   that this is the next command executed.

If at any point the expectation cannot be met, the chain of actions will
be aborted.
