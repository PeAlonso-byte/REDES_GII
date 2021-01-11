#!/bin/bash
pkill -9 servidor
make
./servidor
./cliente localhost TCP ../ordenes/ordenes/ordenes3

