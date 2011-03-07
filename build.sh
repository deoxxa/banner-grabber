#!/bin/sh

cc -c -o dybuf.o dybuf.c
cc -c -o grabber.o grabber.c
cc -o grabber grabber.o dybuf.o -levent
