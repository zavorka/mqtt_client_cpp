// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "test_settings.hpp"
#include "test_server_no_tls.hpp"
#include "test_server_tls.hpp"

#include <mqtt/client.hpp>

BOOST_AUTO_TEST_SUITE(test_connect)

#if !defined(MQTT_NO_TLS)

BOOST_AUTO_TEST_CASE( tls_connect ) {
    boost::asio::io_service ios;
    test_server_tls s(ios);
    auto c = mqtt::make_tls_client(ios, broker_url, broker_tls_port);
    c->set_client_id(cid1());

    std::string path = boost::unit_test::framework::master_test_suite().argv[0];
    std::size_t pos = path.find_last_of("/\\");
    std::string base = pos == std::string::npos ? "./" : path.substr(0, pos + 1);

    c->set_ca_cert_file(base + "cacert.pem");
    c->set_clean_session(true);

    int order = 0;
    c->set_connack_handler(
        [&order, &c]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c->disconnect();
            return true;
        });
    c->set_close_handler(
        [&order, &s]
        () {
            BOOST_TEST(order++ == 1);
            s.close();
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 2);
}

BOOST_AUTO_TEST_CASE( tls_connect_no_strand ) {
    boost::asio::io_service ios;
    test_server_tls s(ios);
    auto c = mqtt::make_tls_client_no_strand(ios, broker_url, broker_tls_port);
    c->set_client_id(cid1());

    std::string path = boost::unit_test::framework::master_test_suite().argv[0];
    std::size_t pos = path.find_last_of("/\\");
    std::string base = pos == std::string::npos ? "./" : path.substr(0, pos + 1);

    c->set_ca_cert_file(base + "cacert.pem");
    c->set_clean_session(true);

    int order = 0;
    c->set_connack_handler(
        [&order, &c]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c->disconnect();
            return true;
        });
    c->set_close_handler(
        [&order, &s]
        () {
            BOOST_TEST(order++ == 1);
            s.close();
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 2);
}

#endif // !defined(MQTT_NO_TLS)

BOOST_AUTO_TEST_CASE( notls_connect ) {
    boost::asio::io_service ios;
    test_server_no_tls s(ios);
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    c->set_client_id(cid1());
    c->set_clean_session(true);

    int order = 0;
    c->set_connack_handler(
        [&order, &c]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c->disconnect();
            return true;
        });
    c->set_close_handler(
        [&order, &s]
        () {
            BOOST_TEST(order++ == 1);
            s.close();
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 2);
}

BOOST_AUTO_TEST_CASE( notls_connect_no_strand ) {
    boost::asio::io_service ios;
    test_server_no_tls s(ios);
    auto c = mqtt::make_client_no_strand(ios, broker_url, broker_notls_port);
    c->set_client_id(cid1());
    c->set_clean_session(true);

    int order = 0;
    c->set_connack_handler(
        [&order, &c]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c->disconnect();
            return true;
        });
    c->set_close_handler(
        [&order, &s]
        () {
            BOOST_TEST(order++ == 1);
            s.close();
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 2);
}

BOOST_AUTO_TEST_CASE( notls_keep_alive ) {
    boost::asio::io_service ios;
    test_server_no_tls s(ios);
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    c->set_client_id(cid1());
    c->set_clean_session(true);

    int order = 0;
    c->set_connack_handler(
        [&order]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            return true;
        });
    c->set_close_handler(
        [&order, &s]
        () {
            BOOST_TEST(order++ == 2);
            s.close();
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->set_pingresp_handler(
        [&order, &c]
        () {
            BOOST_TEST(order++ == 1);
            c->disconnect();
            return true;
        });
    c->set_keep_alive_sec(3);
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 3);
}

BOOST_AUTO_TEST_CASE( notls_connect_again ) {
    boost::asio::io_service ios;
    test_server_no_tls s(ios);
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    c->set_client_id(cid1());
    c->set_clean_session(true);

    bool first = true;
    int order = 0;

    c->set_connack_handler(
        [&first, &order, &c]
        (bool sp, std::uint8_t connack_return_code) {
            if (first) {
                BOOST_TEST(order++ == 0);
            }
            else {
                BOOST_TEST(order++ == 2);
            }
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c->disconnect();
            return true;
        });
    c->set_close_handler(
        [&first, &order, &c, &s]
        () {
            if (first) {
                BOOST_TEST(order++ == 1);
                first = false;
                c->connect();
            }
            else {
                BOOST_TEST(order++ == 3);
                s.close();
            }
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 4);
}

BOOST_AUTO_TEST_CASE( notls_nocid ) {
    boost::asio::io_service ios;
    test_server_no_tls s(ios);
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    c->set_clean_session(true);

    int order = 0;
    c->set_connack_handler(
        [&order, &c]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c->disconnect();
            return true;
        });
    c->set_close_handler(
        [&order, &s]
        () {
            BOOST_TEST(order++ == 1);
            s.close();
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 2);
}

BOOST_AUTO_TEST_CASE( notls_nocid_noclean ) {
    boost::asio::io_service ios;
    test_server_no_tls s(ios);
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);

    int order = 0;
    c->set_connack_handler(
        [&order, &c]
        (bool sp, std::uint8_t connack_return_code) {
            BOOST_TEST(order++ == 0);
            BOOST_TEST(sp == false);
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::identifier_rejected);
            return true;
        });
    c->set_close_handler(
        [&order, &s]
        () {
            BOOST_TEST(order++ == 1);
            s.close();
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 2);
}

BOOST_AUTO_TEST_CASE( notls_noclean ) {
    boost::asio::io_service ios;
    test_server_no_tls s(ios);
    auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
    c->set_client_id(cid1());

    int order = 0;
    int connect = 0;
    c->set_connack_handler(
        [&order, &connect, &c]
        (bool sp, std::uint8_t connack_return_code) {
            switch (connect) {
            case 0:
                BOOST_TEST(order++ == 0);
                BOOST_TEST(sp == false);
                break;
            case 1:
                BOOST_TEST(order++ == 2);
                BOOST_TEST(sp == true);
                break;
            case 2:
                BOOST_TEST(order++ == 4);
                BOOST_TEST(sp == false);
                break;
            case 3:
                BOOST_TEST(order++ == 6);
                BOOST_TEST(sp == false);
                break;
            }
            BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
            c->disconnect();
            return true;
        });
    c->set_close_handler(
        [&order, &connect, &c, &s]
        () {
            switch (connect) {
            case 0:
                BOOST_TEST(order++ == 1);
                c->connect();
                ++connect;
                break;
            case 1:
                BOOST_TEST(order++ == 3);
                c->set_clean_session(true);
                c->connect();
                ++connect;
                break;
            case 2:
                BOOST_TEST(order++ == 5);
                c->set_clean_session(false);
                c->connect();
                ++connect;
                break;
            case 3:
                BOOST_TEST(order++ == 7);
                s.close();
                break;
            }
        });
    c->set_error_handler(
        []
        (boost::system::error_code const&) {
            BOOST_CHECK(false);
        });
    c->connect();
    ios.run();
    BOOST_TEST(order++ == 8);
}

BOOST_AUTO_TEST_SUITE_END()
