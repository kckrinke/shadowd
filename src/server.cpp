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

#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>

#include "server.h"
#include "config.h"
#include "log.h"
#include "shared.h"

swd::server::server() :
 signals_(io_service_),
 acceptor_(io_service_),
 context_(boost::asio::ssl::context::sslv23) {
	/**
	 * Register to handle the signals that indicate when the server should exit.
	 * It is safe to register for the same signal multiple times in a program,
	 * provided all registration for the specified signal is made through asio.
	 */
	signals_.add(SIGINT);
	signals_.add(SIGTERM);
#if defined(SIGQUIT)
	signals_.add(SIGQUIT);
#endif /* defined(SIGQUIT) */

	signals_.async_wait(
		boost::bind(&swd::server::handle_stop, this)
	);
}

void swd::server::init() {
	/**
	 * We try to open the tcp port. If asio throws an error one of the core
	 * components doesn't work and there is no need to continue in that case.
	 */
	try {
		if (swd::config::i()->defined("ssl")) {
			context_.set_options(
				boost::asio::ssl::context::default_workarounds
				| boost::asio::ssl::context::no_sslv2
				| boost::asio::ssl::context::single_dh_use
			);

			context_.use_certificate_chain_file(
				swd::config::i()->get<std::string>("ssl-cert")
			);

			context_.use_private_key_file(
				swd::config::i()->get<std::string>("ssl-key"),
				boost::asio::ssl::context::pem
			);

			context_.use_tmp_dh_file(
				swd::config::i()->get<std::string>("ssl-dh")
			);
		}

		/* Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR). */
		boost::asio::ip::tcp::resolver resolver(io_service_);

		boost::asio::ip::tcp::resolver::query query(
			swd::config::i()->get<std::string>("address"),
			swd::config::i()->get<std::string>("port")
		);

		boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

		acceptor_.open(endpoint.protocol());
		acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
		acceptor_.bind(endpoint);
		acceptor_.listen();
	} catch (boost::system::system_error &e) {
		throw swd::exceptions::core_exception(e.what());
	}

	start_accept();
}

void swd::server::start(std::size_t thread_pool_size) {
	/**
	 * In some cases the compiler can't determine which overload of run was intended
	 * at the bind, resulting in a compilation error.
	 *
	 * This solution for the problem is based on the answer from Dave S at:
	 * https://stackoverflow.com/questions/13476201/difference-between-boostthread-and-stdthread
	 */
	typedef std::size_t (boost::asio::io_service::*signature_type)();
	signature_type run_ptr = &boost::asio::io_service::run;

	/* Create a pool of threads to run all of the io_services. */
	std::vector<boost::shared_ptr<boost::thread> > threads;

	for (std::size_t i = 0; i < thread_pool_size; ++i) {
		boost::shared_ptr<boost::thread> thread(
			new boost::thread(
				boost::bind(run_ptr, &io_service_)
			)
		);

		threads.push_back(thread);
	}

	/* Wait for all threads in the pool to exit. */
	for (std::size_t i = 0; i < threads.size(); ++i) {
		threads[i]->join();
	}
}

void swd::server::start_accept() {
	bool ssl = swd::config::i()->defined("ssl");

	new_connection_.reset(
		new swd::connection(io_service_, context_, ssl)
	);

	acceptor_.async_accept(
		(ssl ? new_connection_->ssl_socket() : new_connection_->socket()),
		boost::bind(
			&swd::server::handle_accept,
			this,
			boost::asio::placeholders::error
		)
	);
}

void swd::server::handle_accept(const boost::system::error_code& e) {
	/**
	 * Try to process the connection, but do not stop the complete server if
	 * something from asio doesn't work out.
	 */
	try {
		if (!e) {
			new_connection_->start();
		}
	} catch (boost::system::system_error &e) {
		swd::log::i()->send(swd::uncritical_error, e.what());
	}

	/* Reset the connection and wait for the next client. */
	start_accept();
}

void swd::server::handle_stop() {
	io_service_.stop();
}