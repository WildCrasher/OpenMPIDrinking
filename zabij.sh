#!/bin/bash

kill -9 `ps -aux |  grep chlanie |  tr -s " " | cut -d " " -f 2`
kill -9 `ps -aux | grep ./test | tr -s " " | cut -d " " -f 2`
ipcrm -a
