#include "logger.h"
#include <boost/log/common.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace keywords = boost::log::keywords;
namespace sinks = boost::log::sinks;

//initialise logger to be used in the program
void init_logger(){
	boost::log::register_simple_formatter_factory< boost::log::trivial::severity_level, char >("Severity"); //register Severity attribute to allow its use in formatter string

	logging::add_file_log
    (
        keywords::file_name = "./logs/SERVER_LOG_%Y-%m-%d_%N.log", //path where logs are stored
        keywords::rotation_size = 10 * 1024 * 1024, //10MB
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0), //rotate at midnight
        keywords::format = "[%TimeStamp%] [%ThreadID%] [%ProcessID%] (%Severity%) : %Message%", 
        keywords::open_mode = std::ios_base::app,
        keywords::auto_flush = true                            
    );
    logging::add_console_log(std::cout, keywords::format = "=> %Message%"); //allow console logging as well

	logging::add_common_attributes();
}
