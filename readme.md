# Arduino Async Duplex

Asynchronously interact with any serial device having a call-and-response
style interface.

## Requirements

* Regexp (https://github.com/nickgammon/Regexp)

## Examples

The following examples are based upon interactions with a SIM7000 LTE modem.

### Sequential

When executing three independent commands, you can follow the below
pattern:

```c++
#include <AsyncDuplex.h>
#include <Regexp.h>

AsyncDuplex handler = AsyncDuplex(&Serial);

void setup() {
    // Get the current timestamp
    time_t currentTime;
    AsyncDuplex.asyncExecute(
        "AT+CCLK?",
        "+CCLK: \"([%d]+)/([%d]+)/([%d]+),([%d]+):([%d]+):([%d]+)([\\+\\-])([%d]+)\"",
        ANY,
        [&currentTime](MatchState ms) {
            char year_str[3];
            char month_str[3];
            char day_str[3];
            char hour_str[3];
            char minute_str[3];
            char second_str[3];
            char zone_dir_str[2];
            char zone_str[3];

            ms.GetCapture(year_str, 0);
            ms.GetCapture(month_str, 1);
            ms.GetCapture(day_str, 2);
            ms.GetCapture(hour_str, 3);
            ms.GetCapture(minute_str, 4);
            ms.GetCapture(second_str, 5);
            ms.GetCapture(zone_dir_str, 6);
            ms.GetCapture(zone_str, 7);

            tmElements_t timeEts;
            timeEts.Hour = atoi(hour_str);
            timeEts.Minute = atoi(minute_str);
            timeEts.Second = atoi(second_str);
            timeEts.Day = atoi(day_str);
            timeEts.Month = atoi(month_str);
            timeEts.Year = (2000 + atoi(year_str)) - 1970;

            currentTime = makeTime(timeEts);
        }
    );

    char connectionStatus[10];
    AsyncDuplex.asyncExecute(
        "AT+CIPSTATUS",
        "STATE: (.*)\n"
        ANY,
        [&connectionStatus](MatchState ms) {
            ms.GetCapture(connectionStatus, 0);
        }
    );
}

void loop() {
    AsyncDuplex.loop();
}
```

This pattern will work fine for independent tasks like the above, but
for a few reasons, this isn't the recommended pattern to follow for
sequential related steps:

1. If one of these tasks' expectations are not met (i.e. `AT+CIPSTART`
   in the examples below returns `ERROR` instead of `OK`), the subsequent
   tasks will still be executed.
2. No guarantee is made that these will be executed sequentially.  More
   commands could be queued and inserted between the above commands if
   another function queues a high-priority (`AsyncDuplex::Timing::NEXT`)
   command.
2. There are a limited number of independent queue slots (by default: 3,
   but this value can be adjusted by changing `COMMAND_QUEUE_SIZE`).


### Nested Callbacks

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
            Serial.println("Connected");

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

This is a simplified overview of connecting to a TCP server using
a SIM7000 LTE modem.

1. Send `AT+CIPSTART...`; wait for `OK` followed by the line ending.
2. Send `AT+CIPSEND...`; wait for a `>` to be printed.  Ensure that
   this is the next command executed.
3. Send the data you want to send followed by CTRL+Z (`\x1a`).  Ensure
   that this is the next command executed.

If at any point the expectation cannot be met, the chain of actions will
be aborted.

### Chaining

Most of the time, you probably just want to run a few commands in sequence,
and the callback structure above may become tedious.  For sequential commands
that should be chained together (like above: aborting subsequent steps
should any command's expectations not be met), there is a simpler way
of handling these:

```c++
#include <AsyncDuplex.h>
#include <Regexp.h>

AsyncDuplex handler = AsyncDuplex(&Serial);

void setup() {
    AsyncDuplex::Command commands[] = {
        AsyncDuplex::Command(
            "AT+CIPSTART=\"TCP\",\"mywebsite.com\",\"80\"", // Command
            "OK\r\n",  // Expectation regex
            [](MatchState ms){
                Serial.println("Connected");
            }
        },
        AsyncDuplex::Command(
            "AT+CIPSEND",
            ">",
        ),
        AsyncDuplex::Command(
            "abc\r\n\x1a"
            "SEND OK\r\n"
        )
    }
    handler.asyncExecuteChain(commands, 3);
}


void loop() {
    AsyncDuplex.loop();
}
```

This is identical in function to the "Nested Callbacks" example above.
