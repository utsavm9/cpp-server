#include "handler.h"

#include <boost/beast/http.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>

#include "gtest/gtest.h"
#include "logger.h"
#include "parser.h"
#include "server.h"

namespace fs = boost::filesystem;

TEST(Handler, StaticUnitTests) {
	EXPECT_EQ(RequestHandler::bad_request().result(), http::status::bad_request);
	EXPECT_EQ(RequestHandler::not_found_error().result(), http::status::not_found);
	EXPECT_EQ(RequestHandler::internal_server_error().result(), http::status::internal_server_error);
}