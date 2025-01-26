# Baremetal Driver Experiment for DS18B20 on NUCLEO-F446ZE

This uses Timer1 and DMA2 direct register access

PE11 - Input capture, directly connected to DS18B20 DQ line
PE13 - PWM, drives a switching NPN transistor base, with the collector connected to DQ line.

