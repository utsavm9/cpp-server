#!/bin/bash

TESTDRIVER="$0"
WEBSERVER="$1"
PORT="$2"
DIR=$(dirname ${TESTDRIVER})

# Utility functions
warn() {
	local MESSAGE="${1}"
	local YELLOW=$(tput setaf 3)
	local NORMAL=$(tput sgr0)
	echo "${YELLOW}WARN: ${MESSAGE}${NORMAL}"
}

# Pre-test checks
if [ ! -x "${WEBSERVER}" ]; then
	echo "Usage: ${0} <path-to-webserver> <port-num>"
	warn "webserver executable not passed in for integration tests, got '${WEBSERVER}' instead "
	exit 1
fi

NUM_REGEX='^[0-9]+$'
if [[ ! "$PORT" =~ $NUM_REGEX ]]; then
	echo "Usage: ${0} <path-to-webserver> <port-num>"
	warn "got '$PORT' as the port number, integer expected"
	exit 1
fi

# Server setup and teardown
# stop() should clear the files left by start()
start() {
	local CONFIG="
		port $PORT;

		location /static StaticHandler{
			root ../data/static_data;
		}

		location /echo EchoHandler {
		}

		location /print EchoHandler {
		}
	"

	# Check if port is free
	PORT_FREE=$(
		nc -z localhost $PORT
		echo $?
	)
	if [ $PORT_FREE -ne 1 ]; then
		warn "port $PORT not free, stopping test"
		exit 1
	fi

	CONFIG_PATH="${DIR}/default.conf"
	echo "$CONFIG" >"$CONFIG_PATH"

	"${WEBSERVER}" "${CONFIG_PATH}" &
	PID="$!"
	sleep 1
}

stop() {
	kill ${PID}
	KILL_RET=$?

	rm "$CONFIG_PATH"

	if [ $KILL_RET -ne 0 ]; then
		warn "could not kill webserver properly, kill returned '$KILL_RET'"
		exit 1
	fi
}

test_header() {
	local OUTPUT="$DIR/output"
	local HEADER="$DIR/header"
	local URL="$1"
	local SEARCH="$2"

	curl -s -o "$OUTPUT" -D "$HEADER" localhost:"$PORT""$URL"

	grep "$SEARCH" "$HEADER"
	GREP_RET=$?
	OUTPUT_CONTENT=$(cat "$OUTPUT")
	HEADER_CONTENT=$(cat "$HEADER")

	rm "$OUTPUT"
	rm "$HEADER"

	if [ $GREP_RET -ne 0 ]; then
		warn "server response header was not expected for $URL" 
		echo "Response obtained from server:"
		echo "$HEADER_CONTENT"
		echo "Expected to see:"
		echo "$SEARCH"

		stop
		exit 1
	fi
}


test_body() {
	local OUTPUT="$DIR/output"
	local HEADER="$DIR/header"
	local URL="$1"
	local SEARCH="$2"

	curl -s -o "$OUTPUT" -D "$HEADER" localhost:"$PORT""$URL"

	grep "$SEARCH" "$OUTPUT"
	GREP_RET=$?
	OUTPUT_CONTENT=$(cat "$OUTPUT")
	HEADER_CONTENT=$(cat "$HEADER")

	rm "$OUTPUT"
	rm "$HEADER"

	if [ $GREP_RET -ne 0 ]; then
		warn "server response body was not expected for $URL" 
		echo "Response obtained from server:"
		echo "$OUTPUT"
		echo "Expected to see:"
		echo "$SEARCH"

		stop
		exit 1
	fi
}

# Integration tests
start

test_header "/" "400 Bad Request"
test_header "/not/in/config" "400 Bad Request"

test_header "/static/" "404 Not Found"
test_header "/static/missing" "404 Not Found"
test_header "/static/deep/folders" "404 Not Found"

test_header "/echo" "200 OK"
test_header "/print" "200 OK"
test_header "/print/deep/folders" "200 OK"
test_header "/static/test.html" "200 OK"
test_header "/static/testing-zip.zip" "200 OK"
test_header "/static/../static/test.html" "200 OK"

test_header "/echo" "text/plain"
test_header "/static/test.html" "text/html"
test_header "/static/testing-zip.zip" "application/zip"

test_body "/echo" "GET"
test_body "/static/test.html" "<html"

stop
