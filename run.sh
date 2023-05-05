#!/usr/bin/bash

c++ -Wall -pthread -fcoroutines -std=c++20 -fconcepts-diagnostics-depth=7 -o phone main.cpp Phone.cpp -lpigpio -lrt -lunifex

sudo ./phone
