# Timer1 UART example

This example uses the Timer1 of the CH32V003 to perform UART RX.  Transitions on the RX line are captured by the CH1 and CH2 channels via DMA to a buffer in memory where the main loop will them parse it and print the received data.

This code is based off of the ch32fun environment.
