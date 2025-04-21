#!/usr/bin/env bash

# Copyright 2025 Nicolai Electronics
# SPDX-License-Identifier: MIT

set -e
set -u

BASE_PATH=/usr/share/zoneinfo

TIMEZONES=`awk '/^Z/ { print $2 }; /^L/ { print $3 }' $BASE_PATH/tzdata.zi | sort`

IFS=$'\n'

FIRSTENTRY=1
NUMZONES=0
JSON="{"

CFILE="#include <stddef.h>
#include \"timezone.h\"

const timezone_t timezones[] = {"

for TIMEZONE in $TIMEZONES
do
TZSTRING=`cat $BASE_PATH/$TIMEZONE | tail -n 1`
if [[ "$FIRSTENTRY" -ne 1 ]]; then
JSON=$JSON","
fi
FIRSTENTRY=0
JSON=$JSON"\"$TIMEZONE\": \"$TZSTRING\""
CFILE=$CFILE"
    {\"$TIMEZONE\", \"$TZSTRING\"},"
((NUMZONES+=1))
done

JSON=$JSON"}"
CFILE=$CFILE"
};

const size_t timezones_len = $NUMZONES;
"

#echo "$JSON" > database.json
echo "$CFILE" > database.c
