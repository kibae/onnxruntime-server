#!/bin/bash

if [ -f /home/runner/actions-runner/.env ]; then
  su -c /home/runner/actions-runner/run.sh - runner
else
  /usr/bin/tail -f /dev/null
fi
