#!/bin/bash

if [ -f /home/runner/actions-runner/.env ]; then
  /home/runner/actions-runner/run.sh
else
  /usr/bin/tail -f /dev/null
fi
