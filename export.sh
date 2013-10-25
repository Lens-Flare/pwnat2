#!/bin/sh

COMMAND = $0
CONFIG = $1

PREFIX = `$0 -0 prefix`
ENVVARS = `$0 -0 list`

source "$CONFIG"

for VAR in $ENVVARS
do
	export $PREFIX$VAR = ${!VAR}
done