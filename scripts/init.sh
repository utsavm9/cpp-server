#!/bin/bash

# Get certificates
mkdir --parent /logs/certbot

certbot certonly \
	--non-interactive \
	--standalone \
	--domains www.koko.cs130.org \
	--email utsavm9@g.ucla.edu \
	--agree-tos \
	>/logs/certbot/stdout.log 2>/logs/certbot/stderr.log

# Print the error logs so that they are viewable in GCloud Logs
cat /logs/certbot/stderr.log

# Start our server
#
# Bash script becomes our server with exec command
# instead of spawning a child process. This allows signals
# to go directly to our webserver instead of geting
# engulfed by this script
exec ./webserver deploy.conf
