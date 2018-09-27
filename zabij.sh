#!/bin/bash

kill -9 `ps -aux |  grep chlanie2 |  tr -s " " | cut -d " " -f 2`
kill -9 `ps -aux |  grep chlanie3 |  tr -s " " | cut -d " " -f 2`
kill -9 `ps -aux | grep ./test | tr -s " " | cut -d " " -f 2`
ipcrm -a
