#!/bin/bash

gcc -c ./frozen-master/frozen.c -I ./frozen-master -o frozen.o
ar rcs libfrozen.a frozen.o

gcc main.c -o main.o -lsoundio -laubio -lm -I ./rpi_ws281x -L ./rpi_ws281x -lfrozen -lws2811 -Ofast
