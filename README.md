# ac-swing-servo

This project uses an Atmega328p for controlling a Micro servo to control the airflow of a air conditioning unit.

The user interface is done using by intercepting the IR signal from the original remote control, but overloading other buttons which doesn't have any normal function while being activated in a special way, e.g.: long press, or pressing it on a specific state.

It also implement a Serial COM for external control.

A led illuminate the grid with different patterns as a user interface.

Multitasking is implemented via assyncronous loop, and task schedulation. Infrared protocol NEC is implemented through a IRRemote Lib which uses the hardware interrupt.

The code is well documented, feel free to [open](https://github.com/victornpb/ac-swing-servo/issues) an issue if you have any questions.
