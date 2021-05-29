# Define builder stage
FROM koko:base as builder

# Share work directory
COPY . /usr/src/project
WORKDIR /usr/src/project/build_coverage

# Build and make coverage report
RUN cmake -DCMAKE_BUILD_TYPE=Coverage ..
RUN make -j 4
RUN make coverage -j 4