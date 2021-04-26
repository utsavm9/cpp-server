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
		server {
			listen $PORT;
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

test_nc() {
	local OUTPUT="$DIR/output"
	local EXPECTED="$DIR/expected"
	local REQUEST="$1"
	local RESPONSE="$2"

	printf "$REQUEST" | nc -q0 localhost $PORT >"$OUTPUT"
	printf "$RESPONSE" >"$EXPECTED"

	DIFF_OUT=$(diff "$EXPECTED" "$OUTPUT")
	DIFF_RET=$?
	OUTPUT_CONTENT=$(cat "$OUTPUT")

	rm "$OUTPUT"
	rm "$EXPECTED"

	if [ $DIFF_RET -ne 0 ]; then
		warn "server response did not match expected response"
		echo "Request passed to server:"
		echo "$REQUEST"
		echo "Response obtained from server:"
		echo "$OUTPUT_CONTENT"
		echo "Diff <expected >actual"
		echo "$DIFF_OUT"

		stop
		exit 1
	fi
}

test_curl() {
	local OUTPUT="$DIR/output"
	local EXPECTED="$DIR/expected"
	local BODY="$1"

	curl -o "$OUTPUT" -s localhost:"$PORT" --data "$BODY"

	grep "$BODY" "$OUTPUT"
	GREP_RET=$?
	OUTPUT_CONTENT=$(cat "$OUTPUT")

	rm "$OUTPUT"

	if [ $GREP_RET -ne 0 ]; then
		warn "server response did not echo the body"
		echo "Response obtained from server:"
		echo "$OUTPUT_CONTENT"

		stop
		exit 1
	fi
}

# Integration tests
start

REQUEST="Missing headers"
RESPONSE="HTTP/1.1 200 OK\r
Content-Type: text/plain\r
Content-Length: 15\r
\r
Missing headers"
test_nc "$REQUEST" "$RESPONSE"

REQUEST="\n"
RESPONSE="HTTP/1.1 200 OK\r
Content-Type: text/plain\r
Content-Length: 1\r
\r

"
test_nc "$REQUEST" "$RESPONSE"

REQUEST="GET / HTTP/1.1"
RESPONSE="HTTP/1.1 200 OK\r
Content-Type: text/plain\r
Content-Length: 14\r
\r
GET / HTTP/1.1"
test_nc "$REQUEST" "$RESPONSE"

BODY="Full-fledged HTTP request body"
test_curl "$BODY"

stop
