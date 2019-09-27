#!/bin/bash

gcc main.c -o main.o -lsoundio -laubio -lm -I ./rpi_ws281x -L ./rpi_ws281x -lws2811 -Ofast
