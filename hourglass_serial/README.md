
### Hourglass Serial Example

The Trinket M0 is a microcontroller that has a clock speed of 48MHz and can be used in a number of small applications. Interrupts can be configured and used for things such as taking sound samples at specific intervals as well as controlling PWM RGB LEDs. However due to the nature of interrupts, there may be issues running two different things that need to use interrupts at precise intervals as only one interval may run at a time and the running interrupt may block another interrupt from firing at the precise clock tick configured.

This is a problem especially for PWM LEDs which rely on very specific timings to active the LEDs, and its also a problem when using sound samples and filter functions that need a very specific and consistent sampling to run the calculations with.

One way to address this problem is by using two microcontrollers and have them communicate via Serial1. This frees up controller 1 to do one interrupt specific task (such as read sound samples), and frees up controller 2 to be fed that data and do another interrupt specific task (such as control a strip of PWM LEDs).

The two sketches, `consumer` and `producer` are meant to be loaded on to two separate micro controllers in order to demonstrate this serial communication.

#### Wiring

Pins 3 and 4 are the RX/TX pins for the Trinket M0. Connect Pin 3 on board 1 to pin 4 on board 2, and pin 4 on board 1 to pin 3 on board 2 (RX goes to TX and vice versa).

For the consumer, Pin 0 is the LED control pin.
Ground, 3.3v can be wired as normal. The boards ran off the microusb power.

#### Sketch

Using the Arduino IDE (v1.8.19 was used, v2 may be slightly different) open both consumer.ino and producer.ino. These will be loaded one to each board.

Under Tools, make sure the proper serial port is selected when loading the sketches on each trinket. Its helpful to view the serial monitor to see the debug output to figure out which port is for which board. The producer output will be a line of several digits several times a second, while the monitor for the consumer will say "reading bytes" and then echo whatever the producer sent over serial.

FYI - The Arduino IDE version being used only allowed one serial monitor to be open and the selected serial port applied for all open Arduino IDE instances (even if it looked like they should be separate as there were multiple windows open). 


If all went well, the LED should show a repeating pattern of a pixel moving from start to end, and slowing filling then removing on then off pixels with 7 colors per pixel.
