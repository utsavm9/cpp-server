### Build/test container ###
# Define builder stage
FROM koko:base as builder

# Share work directory
COPY . /usr/src/project
WORKDIR /usr/src/project/build

# Build and test
RUN cmake ..
RUN make -j 4
RUN ctest -j 4 --output-on_failure


### Deploy container ###
# Define deploy stage
FROM koko:base as deploy

# Copy files to server root directory
COPY --from=builder /usr/src/project/build/bin/webserver /
COPY --from=builder /usr/src/project/conf/deploy.conf /
COPY --from=builder /usr/src/project/data /
COPY --from=builder /usr/src/project/scripts/init.sh /
COPY --from=builder /usr/src/project/tests/certs /tests/certs

# Expose some port(s)
EXPOSE 80
EXPOSE 443

# Use ENTRYPOINT to specify the binary name
ENTRYPOINT ["/init.sh"]
