#!/bin/sh

cc -c -o dybuf.o dybuf.c
cc -o grabber grabber.c dybuf.o -levent
