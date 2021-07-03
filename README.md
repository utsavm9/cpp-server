

<h1 align="center">Koko Webserver</h1>

An async webserver written in C++ using Boost Beast library. It can:
* Serve static files
* Compress responses
* Act as proxy
* Supports HTTPS (SSL)
* Reuse connection using `keep-alive`

---

## 1. Build, Run and Test


### Dependencies
```bash
$ sudo apt-get install --yes \
    build-essential \
    cmake \
    gcovr \
    libboost-log-dev \
    libboost-regex-dev \
    libboost-system-dev \
    libboost-iostreams-dev \
    libgtest-dev \
    libssl-dev \
    zlib1g-dev
```
 
### Build
 
Perform an out of source build using
 
```
$ mkdir build
$ cd build
$ cmake ..
$ make
```
 
### Run
 
The executable is found at `build/bin/webserver`. Run
 
```
./bin/webserver ../conf/default.conf
```
to start the server. You can use any configuration file as the argument.
 
### Test
 
use `ctest` or `make test` to run test cases
```
$ ctest --output-on-failure
```
or
```
$ make test
```
### Test Coverage 
 
Perform an out of source build in a new directory called build_coverage:
```
$ mkdir build_coverage
$ cd build_coverage
$ cmake -DCMAKE_BUILD_TYPE=Coverage ..
$ make coverage
```
This will generate coverage reports in `${REPO}/build_coverage/report/index.html`
 

## 2. Ports

The following port numbers are used by various server instances than can be lauched
during testing or productions. The pair of ports correspond to the HTTP and HTTPS port
used by the server instance. The instances that can possibly be run in parallel need to
have different port numbers.

* 80, 443: Production server deployed to GCloud, uses `conf/deploy.conf`.
* 8080, 8081: Manual testing server that can be launched using `conf/default.conf`.
* 8000, 8001: `ServerTest.ServeForver` Unit test's server, ensuring that the server does not crashes instantly.
* 8100, 8101: `ServerTest.MultiThreadTest` Unit test's server, for testing that the server can use multiple threads.
* 8080, 8081: Integration test's primary server
* 8082, 8083: Integration test's proxy server
 
## 3. Source Code Overview
 
This is a basic web server that performs different services, such as echoing a request and serving static files. It is built to be extensible by having external developers easily add more handlers.
### Folders
 
* `conf/:` Configuration files for deployment and local debugging
* `data/:` Static files to be served
* `docker/:` dockerfiles for building deployment image
* `include/:` header files defining classes
* `src/:` `.cc` files containing class implementations and main method
* `tests/:` test cases for source code
 
### Handlers
 
* `Handler:` parent class for Handlers. Pure virtual so cannot be instantiated.
* `EchoHandler:` Simple Handler that echoes request on the specified url
* `FileHandler:` Handler for serving static content from specific linux directory on request url
 
## 4. Add Request Handler to Server
 
To add your own Handler follow these steps:
 
* First, create a subclass of `RequestHandler`. If you want you can define your own constructor. You must implement the `handle_request` method of this class. Make sure you define the class in a header file and put it in the `include/` folder while the `.cc` file goes in the `src/` folder.
 
* Once done implementing your Handler, in the `server.cc` file, there is a method called `create_handler`. Within that you should add code to check if your handler is required to be created and do so while adding the new Handler to a map from url to Handler pointer. For example the `NotFoundHandler` is created like this:
 
```
// In server::create_handler()
   else if (handler_name == "NotFoundHandler") {
       TRACE << "registering not found handler for url prefix: " << url_prefix;
       return new NotFoundService(url_prefix, subconfig);
   }
```
* After that you can add a location to the config that uses your new defined handler. These locations are configurable and you can specify your desired configurations and parse them in your handler using the `NginxConfig` object passed to it. 
 
Here is an example of a `FileHandler` Type location:
```
location “/files” FileHandler {
	root “/data/static_data”;
}
```
 
* In `CMakeLists.txt` make sure to add your handler’s `.cc` file, `your_handler.cc` for example, to this line:
 
```
add_library(requestHandler src/requestHandler.cc src/echoHandler.cc src/fileHandler.cc src/notfoundHandler.cc src/yourHandler.cc)
```
 
* To add unit tests add them to the `handler_test.cc` file.
 
* A well-documented header and source file example can be found at `src/notFoundHandler.cc` and `include/notFoundHandler.h`

## 5. Contribution Guidelines

We use the following CLang code formatting style:
```json
{
	BasedOnStyle: Google,  
	IndentWidth: 4,  
	IndentCaseLabels: false,  
	TabWidth: 4, 
	UseTab: ForIndentation,  
	ColumnLimit: 0
}
```
Editors can be configured to pass the above style to clang to override its default settings. For example: In VSCode Preferences → Settings page and under the right tab (User/Remote), the above style can be filled in the `C_Cpp: Clang_format_style` setting.