#!/bin/sh

cc -c -o dybuf.o dybuf.c
cc -c -o grabber.o grabber.c
cc -c -o output_csv.o output_csv.c
cc -c -o output_xml.o output_xml.c
cc -c -o output_json.o output_json.c
cc -o grabber grabber.o dybuf.o output_csv.o output_xml.o output_json.o -levent
