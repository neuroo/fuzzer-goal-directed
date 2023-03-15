#!/usr/bin/env bash
rm -f fuzzing.log
./coverage-fuzz --fake-target-call=true -- t
