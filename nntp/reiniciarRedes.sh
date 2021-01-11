#!/bin/bash
pkill -9 servidor
make
./servidor
./cliente nogal TCP ordenes1.txt &
./cliente nogal TCP ordenes2.txt &
./cliente nogal TCP ordenes3.txt &

