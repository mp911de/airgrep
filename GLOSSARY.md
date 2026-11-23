# airgrep firmware

Language of the ESP32-S3 firmware. Hardware terms live in `docs/`.

## Language

**Event loop**:
The single FreeRTOS task that runs Tasks in submission order. Actions may block on hardware only for a short, bounded time.
_Avoid_: Scheduler, executor, dispatcher

**Task**:
A unit of work submitted to the event loop: an action, an optional name for logging and an optional timeout. A Task has no identity beyond its promise. A FreeRTOS task is always called that.
_Avoid_: Event (that is a published fact), job, message, runnable

**Action**:
What a Task does when the loop runs it. Completes its promise at once, reports an error, or leaves it pending for later completion.

**Promise**:
The outcome carrier of a Task. Delivers the action's value to the success listener or an outcome to the error listener, both fixed at submission, then ceases to exist.
_Avoid_: Future, callback

**Event**:
A fact a component publishes when its state changes, such as a mode change or a lost fix. A small immutable snapshot, delivered on the event loop to every subscriber of its type. An Event published before a subscriber subscribed never reaches it.
_Avoid_: Signal, notification, message

**Subscriber**:
A handler registered for Event types, or for a driver's Polls or messages. It stays registered until restart.

**Handle**:
What a submitter gets back from a successful submission. Used to cancel, to complete a pending promise and to ask whether the promise is still pending. A handle whose promise is gone is harmless.

**Listener**:
A success or error callback given at submission. Runs on the task that completes the promise. Cancellation reaches the error listener.
_Avoid_: Listener for a predicate-selected message handler, that is a subscriber.

**Tick**:
The loop's own wake-up when no Task arrives, at a tunable period. Drives timeouts and scheduled Tasks.
_Avoid_: Timer

**Decoder**:
Pure logic that turns received bytes into messages, accumulating until a message is complete. Owns no hardware, so it is testable in RAM.
_Avoid_: Parser, splitter

**Hardware test mode**:
A firmware variant that runs checks against the fitted hardware through the event loop and reports each result on the console.

**State**:
A driver's last known picture of its device, including decoded values, raw registers, update time and errors.

**Poll**:
A periodic Task that refreshes a State through bounded hardware actions.

**Command**:
A console entry that prints a State or posts an action.

**Charge control**:
The rules in the system component that set the charger's input limit, charge current, charge enable, TREG and hardware input limit from the USB source, the pack capacity and the pack temperature. AUTO applies them, MANUAL leaves the charger to Commands.

**Charger target**:
What charge control decided for the charger at its last run, kept in the charger State next to the registers the chip reports. A target the registers do not match yet is pending.
_Avoid_: Target alone, that is an aircraft.

**Health check**:
The evaluation of whether every component the current operating mode needs is usable. Its result is a mask of health bits, one per component, shown by `uptime` and on the USR LED.

**Health bit**:
One bit of the health check result. It names the component to look at, not the reason.

**Device ID**:
`airgrep-` followed by the factory MAC in lower case hex without separators. Names the device on the console banner and its Wi-Fi network.
_Avoid_: Hostname, SSID, serial number

**Target**:
One aircraft or FLARM transmitter the ADSBee reports, identified by its address and its Source. Holds the last report and when it was seen, nothing older.
_Avoid_: aircraft (FLARM obstacles and ground vehicles are targets too), track

**Source**:
How a Target reached the ADSBee: 1090 ADS-B, ADS-R, TIS-B or FLARM. The same address from two sources is two Targets.

**Traffic**:
Every Target seen within the last minute. Relative distance and bearing are computed from the own position when asked, never stored.
_Avoid_: dictionary

**Alerting result**:
The outcome of evaluating one Target against own ship. It gives the alert level, when the alert threshold is predicted to be violated, and whether the Target is predicted to enter the corrective volume. No alert is a valid result.

**Own ship**:
The aircraft that carries the device. Every Target is evaluated against it.
_Avoid_: ownship, self

**Flight**:
A Target or the own ship at one moment: its position, altitude, track, ground speed and vertical speed, on a clock that all flights of a step share.
_Avoid_: aircraft, state

**Well clear**:
The volume around the own ship that DO-365B keeps Targets out of. An alert counts down to its loss.

**Look-ahead**:
How far ahead in time a Target must be able to lose well clear to be evaluated at all.

**Time to attention**:
The soonest a Flight could stop being not a factor to the own ship, if both flew at each other as fast and as steeply as the Bands allow. It lies before any loss of well clear.
_Avoid_: time to violation, reach, arrival

**Traffic filter**:
What keeps the Targets whose time to attention lies within the look-ahead, the soonest first. Only these are evaluated.
_Avoid_: prefilter, ranking

**Alert targets**:
The first Targets the traffic filter keeps, up to a fixed number, that the engine evaluates for alerts.
_Avoid_: alerted

**Band targets**:
The first of the alert targets, fewer of them, that the Bands are computed against.
_Avoid_: banded, near

**Guide**:
One quantity of the own ship that a Band speaks of: track, ground speed, vertical speed or altitude.

**Band**:
A stretch of one Guide that leads into a conflict, with the level of that conflict. Once well clear is lost, a stretch that leads out of it soonest.
_Avoid_: interval, stretch

**Band span**:
How far the Bands of ground speed and of altitude reach to either side of the own ship's current value.
_Avoid_: reach

**Radar position**:
Where a Target lies relative to the outer radar ring. A listed Target can be inside the ring, beyond it, or at an unknown range.

**Beyond-ring marker**:
A section of the outer radar ring at the bearing of a relevant Target beyond the displayed range. A Target can appear in the traffic list without a marker.

**Wi-Fi slider**:
SW3. Its position is the only truth about whether the ADSBee's access point runs. The firmware switches the ADSBee on every flip and corrects it when a poll finds it differing. Nothing is ever saved on the ADSBee.
_Avoid_: Wi-Fi setting

**Log ring**:
A bounded history of complete log lines, replayed or tailed by the console.
