/**
 * Shadow Daemon -- High-Interaction Web Honeypot
 *
 *   Copyright (C) 2014 Hendrik Buchwald <hb@zecure.org>
 *
 * This file is part of Shadow Daemon. Shadow Daemon is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "singleton.h"
#include "shared.h"

namespace swd {
	/**
	 * @brief Encapsulates and handles the configuration parsing.
	 */
	class config :
	 public swd::singleton<swd::config> {
		public:
			/**
			 * @brief Construct the config and add all possible options.
			 */
			config();

			/**
			 * @brief Parse the command line and apply it to the config.
			 *
			 * @param argc The number of command line arguments
			 * @param argv The command line arguments
			 */
			void parse_command_line(int argc, char** argv);

			/**
			 * @brief Parse a file and apply it to the config.
			 *
			 * @param file The file that gets parsed
			 */
			void parse_config_file(std::string file);

			/**
			 * @brief Validate the configuration and throw an exception if something
			 *  important is not set.
			 */
			void validate();

			/**
			 * @brief Test if the configuration value is set.
			 *
			 * defined and get are just simple wrappers for boost::program_options
			 * functionality at the moment. This way the po lib stays isolated and
			 * it is easier to replace it later if there is a reason to do so.
			 *
			 * @param key The key of the configuration value
			 */
			bool defined(std::string key) { return (vm_.count(key) > 0); }

			/**
			 * @brief Get the configuration value.
			 *
			 * @see defined
			 *
			 * @param key The key of the configuration value
			 */
			template<class T> T get(std::string key) { return vm_[key].as<T>(); }

		private:
			po::options_description od_generic_;
			po::options_description od_server_;
			po::options_description od_daemon_;
			po::options_description od_database_;
			po::variables_map vm_;
	};
}

#endif /* CONFIG_H */