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

# Copy certificates if we exceed Let's Encrypt's rate limit
if [[ ! -f "/etc/letsencrypt/live/www.koko.cs130.org/fullchain.pem" ]]; then
	mkdir --parent /etc/letsencrypt/live/www.koko.cs130.org/
	cp /tests/certs/fullchain2.pem /etc/letsencrypt/live/www.koko.cs130.org/fullchain.pem
fi
if [[ ! -f "/etc/letsencrypt/live/www.koko.cs130.org/privkey.pem" ]]; then
	mkdir --parent /etc/letsencrypt/live/www.koko.cs130.org/
	cp /tests/certs/privkey2.pem /etc/letsencrypt/live/www.koko.cs130.org/privkey.pem
fi

# Start our server
#
# Bash script becomes our server with exec command
# instead of spawning a child process. This allows signals
# to go directly to our webserver instead of geting
# engulfed by this script
exec ./webserver deploy.conf
