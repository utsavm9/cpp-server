#!/bin/bash
TESTDRIVER="$0"
WEBSERVER="$1"
PORT="$2"
PORT_HTTPS="$3"
PROXY_PORT="$4"
PROXY_PORT_HTTPS="$5"
CERTIFICATE="$6"
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
	echo "Usage: ${0} <path-to-webserver> <port-num> <proxy-port-num> <certificate>"
	warn "webserver executable not passed in for integration tests, got '${WEBSERVER}' instead "
	exit 1
fi

NUM_REGEX='^[0-9]+$'
if [[ ! "$PORT" =~ $NUM_REGEX ]]; then
	echo "Usage: ${0} <path-to-webserver> <port-num> <https-port-num> <proxy-port-num> <proxy-port-https-num> <certificate>"
	warn "got '$PORT' as the HTTP port number, integer expected"
	exit 1
fi

if [[ ! "$PROXY_PORT" =~ $NUM_REGEX ]]; then
	echo "Usage: ${0} <path-to-webserver> <port-num> <https-port-num> <proxy-port-num> <proxy-port-https-num> <certificate>"
	warn "got '$PROXY_PORT' as the proxy's HTTP port number, integer expected"
	exit 1
fi

if [[ ! "$PORT_HTTPS" =~ $NUM_REGEX ]]; then
	echo "Usage: ${0} <path-to-webserver> <port-num> <https-port-num> <proxy-port-num> <proxy-port-https-num> <certificate>"
	warn "got '$PORT_HTTPS' as the HTTPS port number, integer expected"
	exit 1
fi

if [[ ! "$PROXY_PORT_HTTPS" =~ $NUM_REGEX ]]; then
	echo "Usage: ${0} <path-to-webserver> <port-num> <https-port-num> <proxy-port-num> <proxy-port-https-num> <certificate>"
	warn "got '$PROXY_PORT_HTTPS' as the proxy's HTTPS port number, integer expected"
	exit 1
fi

if [ ! -f "$CERTIFICATE" ]; then
	warn "got certificate filepath '$CERTIFICATE', which is not a file"
	exit 1
fi

# Server setup and teardown
# stop() should clear the files left by start()
start() {
	local CONFIG="
		port $PORT;
		httpsPort $PORT_HTTPS;
		certificate ../tests/certs/fullchain.pem;
		privateKey ../tests/certs/privkey.pem;

		location /static StaticHandler {
			root ../data/static_data;
		}

		location \"/echo/\" EchoHandler {
		}

		location /print EchoHandler {
		}

		location / NotFoundHandler {
		}

		location /status StatusHandler {
		}

		location /health HealthHandler {
		}

		location /proxy ProxyRequestHandler {
			dest \"localhost\";
			port $PROXY_PORT;
		}

		location \"/bitlyproxy\" ProxyRequestHandler {
			dest \"www.bit.ly\";
			port 80;
		}

		location /sleep SleepEchoHandler {
		}

		location /keepalive EchoHandler {
			keep-alive 1;
		}
	"

	local PROXY_CONFIG="
		port $PROXY_PORT;
		httpsPort $PROXY_PORT_HTTPS;
		certificate ../tests/certs/fullchain.pem;
		privateKey ../tests/certs/privkey.pem;

		location \"/proxystatic\" StaticHandler {
			root ../data/static_data;
		}

		location \"/proxyecho/\" EchoHandler {
		}

		location / NotFoundHandler {
		}

		location /proxystatus StatusHandler {

		}

		location \"/bitlynested\" ProxyRequestHandler {
			dest \"www.bit.ly\";
			port 80;
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

	# Check if port is free
	PROXY_PORT_FREE=$(
		nc -z localhost $PROXY_PORT
		echo $?
	)
	if [ $PROXY_PORT_FREE -ne 1 ]; then
		warn "proxy port $PROXY_PORT not free, stopping test"
		exit 1
	fi

	CONFIG_PATH="${DIR}/default.conf"
	echo "$CONFIG" >"$CONFIG_PATH"

	"${WEBSERVER}" "${CONFIG_PATH}" &
	PID="$!"
	sleep 1

	PROXY_CONFIG_PATH="${DIR}/proxy.conf"
	echo "$PROXY_CONFIG" >"$PROXY_CONFIG_PATH"

	"${WEBSERVER}" "${PROXY_CONFIG_PATH}" &
	PROXY_PID="$!"
	sleep 1

	ps
}

stop() {
	kill ${PID}
	KILL_RET=$?

	kill ${PROXY_PID}
	PROXY_KILL_RET=$?

	rm "$CONFIG_PATH"
	rm "$PROXY_CONFIG_PATH"

	if [ $KILL_RET -ne 0 ] || [ $PROXY_KILL_RET -ne 0 ]; then
		warn "could not kill webservers properly, kill returned '$KILL_RET' and proxy kill returned '$PROXY_KILL'"
		exit 1
	fi
}

test_header_http() {
	local OUTPUT="$DIR/output"
	local HEADER="$DIR/header"
	local URL="$1"
	local SEARCH="$2"

	curl -s -o "$OUTPUT" -D "$HEADER" http://localhost:"$PORT""$URL"

	grep "$SEARCH" "$HEADER"
	GREP_RET=$?
	OUTPUT_CONTENT=$(cat "$OUTPUT")
	HEADER_CONTENT=$(cat "$HEADER")

	rm "$OUTPUT"
	rm "$HEADER"

	if [ $GREP_RET -ne 0 ]; then
		warn "server response header was not expected for http://localhost:${PORT}${URL}"
		echo "Response obtained from server:"
		echo "$HEADER_CONTENT"
		echo "Expected to see:"
		echo "$SEARCH"

		stop
		exit 1
	fi
}

test_header_https() {
	local OUTPUT="$DIR/output"
	local HEADER="$DIR/header"
	local URL="$1"
	local SEARCH="$2"

	curl -s -o "$OUTPUT" -D "$HEADER" https://localhost:"$PORT_HTTPS""$URL" --cacert "$CERTIFICATE"

	grep "$SEARCH" "$HEADER"
	GREP_RET=$?
	OUTPUT_CONTENT=$(cat "$OUTPUT")
	HEADER_CONTENT=$(cat "$HEADER")

	rm "$OUTPUT"
	rm "$HEADER"

	if [ $GREP_RET -ne 0 ]; then
		warn "server response header was not expected for https://localhost:${PORT_HTTPS}${URL}"
		echo "Response obtained from server:"
		echo "$HEADER_CONTENT"
		echo "Expected to see:"
		echo "$SEARCH"

		stop
		exit 1
	fi
}

test_header() {
	local URL="$1"
	local SEARCH="$2"

	test_header_http "$URL" "$SEARCH"
	test_header_https "$URL" "$SEARCH"
}

test_body() {
	local OUTPUT="$DIR/output"
	local HEADER="$DIR/header"
	local URL="$1"
	local SEARCH="$2"

	curl -s -o "$OUTPUT" -D "$HEADER" http://localhost:"$PORT""$URL"

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

test_body_content() {
	local OUTPUT="$DIR/output"
	local HEADER="$DIR/header"
	local URL="$1"
	local FILEPATH="$2"

	curl -s -o "$OUTPUT" -D "$HEADER" http://localhost:"$PORT""$URL"

	diff -s "$FILEPATH" "$OUTPUT"
	DIFF_RET=$?

	HEADER_CONTENT=$(cat "$HEADER")
	rm "$OUTPUT"
	rm "$HEADER"

	if [ $DIFF_RET -ne 0 ]; then
		warn "server response body not exactly as expected for $URL"
		echo "Header of response obtained from server:"
		echo "$HEADER_CONTENT"
		echo "To reproduce, run"
		echo curl -s -o "$OUTPUT" -D "$HEADER" localhost:"$PORT""$URL"
		echo "and match the response with: $FILEPATH"

		stop
		exit 1
	fi
}

test_bad_req() {
	local OUTPUT="file.txt"
	local HEADER="$DIR/header"
	local REQUEST="$1"
	local SEARCH="$2"

	printf "$REQUEST" | nc localhost "$PORT" -W 1 >file.txt

	OUTPUT_CONTENT=$(cat file.txt)
	grep "$SEARCH" file.txt
	GREP_RET=$?

	if [ $GREP_RET -ne 0 ]; then
		warn "server response was not expected"
		echo "Response obtained from server:"
		echo "$OUTPUT_CONTENT"
		echo "Expected to see:"
		echo "$SEARCH"
		echo "For Request:"
		echo "$REQUEST"

		stop
		exit 1
	fi
}

test_keep_alive() {
	local URL1="$1" #URL with keep alive
	local URL2="$2" #URL without keep alive

	curl -v localhost:"$PORT""$URL1" localhost:"$PORT""$URL2" >file.txt 2>&1

	OUTPUT_CONTENT=$(cat file.txt)
	grep "Connection #0 to host localhost left intact" file.txt
	GREP_RET=$?

	if [ $GREP_RET -ne 0 ]; then
		warn "server response was not expected"
		echo "Response obtained from server:"
		echo "$OUTPUT_CONTENT"
		echo "Expected to see: Connection #0 to host localhost left intact"
		echo "For Path:"
		echo "$URL1"

		stop
		exit 1
	fi

	grep "Re-using existing connection! (#0) with host localhost" file.txt

	if [ $GREP_RET -ne 0 ]; then
		warn "server response was not expected"
		echo "Response obtained from server:"
		echo "$OUTPUT_CONTENT"
		echo "Expected to see: Re-using existing connection! (#0) with host localhost"
		echo "For Path:"
		echo "$URL1"

		stop
		exit 1
	fi

	grep "Closing connection 0" file.txt

	if [ $GREP_RET -ne 0 ]; then
		warn "server response was not expected"
		echo "Response obtained from server:"
		echo "$OUTPUT_CONTENT"
		echo "Expected to see: V"
		echo "For Path:"
		echo "$URL2"

		stop
		exit 1
	fi

}

# Integration tests
start

test_header "/" "404 Not Found"
test_header "/not/in/config" "404 Not Found"
test_header "/echoContinued" "404 Not Found"

test_header "/static/" "404 Not Found"
test_header "/static/missing" "404 Not Found"
test_header "/static/deep/folders" "404 Not Found"

test_header "/echo" "200 OK"
test_header "/print" "200 OK"
test_header "/print/deep/folders" "200 OK"
test_header "/static/test.html" "200 OK"
test_header "/static/testing-zip.zip" "200 OK"
test_header "/static/../static/test.html" "200 OK"
test_header "/status" "200 OK"
test_header "/health" "200 OK"

test_header "/echo" "text/plain"
test_header "/static/test.html" "text/html"
test_header "/static/testing-zip.zip" "application/zip"
test_header "/status" "text/html"
test_header "/health" "text/plain"

test_header "/proxy/proxystatic/samueli.jpg" "200 OK"
test_header "/proxy/proxystatic/samueli.jpg" "Content-Type: image/jpeg"
test_header "/proxy/" "404 Not Found"
test_header "/proxy/not/in/proxyconfig" "404 Not Found"
test_header "/proxy/proxyecho" "200 OK"
test_header "/proxy/proxystatus" "text/html"
test_header "/bitlyproxy/3hlhXsh" "301 Moved Permanently" # Arbitrary bitly link that never expires
test_header "/proxy/bitlynested/3hlhXsh" "301 Moved Permanently"
test_header "/bitlyproxy/3hlhXsh" "Location: http://bit.ly/3hlhXsh"

test_body "/echo" "GET"
test_body "/static/test.html" "<html"
test_body "/status" "<html"
test_body "/status" "<td>/static/missing</td><td>404"
test_body "/health" "OK"

test_body "/proxy/proxyecho" "GET"
test_body "/proxy/proxystatic/test.html" "<html"
test_body "/proxy/proxystatus" "<html"

test_body_content "/static/samueli.jpg" "../data/static_data/samueli.jpg"
test_body_content "/proxy/proxystatic/samueli.jpg" "../data/static_data/samueli.jpg"

test_bad_req "GET /in HTTP/1.1\r\n\n\n" "400 Bad Request"
test_keep_alive "/keepalive" "/print"

stop
