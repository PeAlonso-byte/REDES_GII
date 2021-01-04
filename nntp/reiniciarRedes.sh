#!/bin/bash
pkill -9 servidor
make
./servidor
./cliente localhost TCP

