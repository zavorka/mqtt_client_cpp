// Copyright Takatoshi Kondo 2017
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_SERVER_NO_TLS_HPP)
#define MQTT_TEST_SERVER_NO_TLS_HPP

#include <iostream>
#include <set>

#include <boost/lexical_cast.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/identity.hpp>

#include <mqtt_server_cpp.hpp>
#include "test_settings.hpp"

namespace mi = boost::multi_index;
namespace as = boost::asio;


class test_server_no_tls {
public:
    test_server_no_tls(as::io_service& ios)
        : server_(as::ip::tcp::endpoint(as::ip::tcp::v4(), broker_notls_port), ios) {
        server_.set_error_handler(
            [](boost::system::error_code const& ec) {
                std::cout << "error: " << ec.message() << std::endl;
            }
        );

        server_.set_accept_handler(
            [&](con_t& ep) {
                std::cout << "accept" << std::endl;
                auto sp = ep.shared_from_this();
                ep.start_session(
                    [sp] // keeping ep's lifetime as sp until session finished
                    (boost::system::error_code const& ec) {
                        std::cout << "session end: " << ec.message() << std::endl;
                    }
                );

                // set connection (lower than MQTT) level handlers
                ep.set_close_handler(
                    [&]
                    (){
                        std::cout << "closed." << std::endl;
                        close_proc(ep);
                    });
                ep.set_error_handler(
                    [&]
                    (boost::system::error_code const& ec){
                        std::cout << "error: " << ec.message() << std::endl;
                        close_proc(ep);
                    });

                // set MQTT level handlers
                ep.set_connect_handler(
                    [&]
                    (std::string const& client_id,
                     boost::optional<std::string> const& username,
                     boost::optional<std::string> const& password,
                     boost::optional<mqtt::will>,
                     bool clean_session,
                     std::uint16_t keep_alive) {
                        std::cout << "client_id    : " << client_id << std::endl;
                        std::cout << "username     : " << (username ? username.get() : "none") << std::endl;
                        std::cout << "password     : " << (password ? password.get() : "none") << std::endl;
                        std::cout << "clean_session: " << std::boolalpha << clean_session << std::endl;
                        std::cout << "keep_alive   : " << keep_alive << std::endl;
                        if (client_id.empty() && !clean_session) {
                            ep.connack(false, mqtt::connect_return_code::identifier_rejected);
                            return false;
                        }
                        cons_.insert(ep.shared_from_this());
                        if (clean_session) {
                            ep.connack(false, mqtt::connect_return_code::accepted);
                            sessions_.erase(client_id);
                            subsessions_.erase(client_id);
                        }
                        else {
                            {
                                auto it = sessions_.find(client_id);
                                if (it == sessions_.end()) {
                                    ep.connack(false, mqtt::connect_return_code::accepted);
                                }
                                else {
                                    sessions_.erase(it);
                                    ep.connack(true, mqtt::connect_return_code::accepted);
                                }
                            }
                            auto it = subsessions_.find(client_id);
                            if (it != subsessions_.end()) {
                                ep.connack(true, mqtt::connect_return_code::accepted);
                                for (auto const& d : it->s->data) {
                                    ep.publish(
                                        d.topic,
                                        d.contents,
                                        d.qos,
                                        d.retain
                                    );
                                }
                                subsessions_.erase(it);
                            }
                        }
                        return true;
                    }
                );
                ep.set_disconnect_handler(
                    [&]
                    (){
                        std::cout << "disconnect received." << std::endl;
                        close_proc(ep);
                    });
                ep.set_puback_handler(
                    [&]
                    (std::uint16_t packet_id){
                        std::cout << "puback received. packet_id: " << packet_id << std::endl;
                        return true;
                    });
                ep.set_pubrec_handler(
                    [&]
                    (std::uint16_t packet_id){
                        std::cout << "pubrec received. packet_id: " << packet_id << std::endl;
                        return true;
                    });
                ep.set_pubrel_handler(
                    [&]
                    (std::uint16_t packet_id){
                        std::cout << "pubrel received. packet_id: " << packet_id << std::endl;
                        return true;
                    });
                ep.set_pubcomp_handler(
                    [&]
                    (std::uint16_t packet_id){
                        std::cout << "pubcomp received. packet_id: " << packet_id << std::endl;
                        return true;
                    });
                ep.set_publish_handler(
                    [&]
                    (std::uint8_t header,
                     boost::optional<std::uint16_t> packet_id,
                     std::string topic_name,
                     std::string contents){
                        std::uint8_t qos = mqtt::publish::get_qos(header);
                        bool is_retain = mqtt::publish::is_retain(header);
                        std::cout << "publish received."
                                  << " dup: " << std::boolalpha << mqtt::publish::is_dup(header)
                                  << " qos: " << mqtt::qos::to_str(qos)
                                  << " retain: " << is_retain << std::endl;
                        if (packet_id)
                            std::cout << "packet_id: " << *packet_id << std::endl;
                        std::cout << "topic_name: " << topic_name << std::endl;
                        std::cout << "contents: " << contents << std::endl;
                        {
                            auto const& idx = subs_.get<tag_topic>();
                            auto r = idx.equal_range(topic_name);
                            for (; r.first != r.second; ++r.first) {
                                r.first->con->publish(
                                    topic_name,
                                    contents,
                                    std::min(r.first->qos, qos),
                                    is_retain
                                );
                            }
                        }
                        {
                            auto const& idx = subsessions_.get<tag_topic>();
                            auto r = idx.equal_range(topic_name);
                            for (; r.first != r.second; ++r.first) {
                                r.first->s->data.emplace_back(
                                    topic_name,
                                    contents,
                                    std::min(r.first->qos, qos),
                                    is_retain
                                );
                            }
                        }
                        if (is_retain) {
                            if (contents.empty()) {
                                retains_.erase(topic_name);
                            }
                            else {
                                retains_.emplace(topic_name, qos, contents);
                            }
                        }
                        return true;
                    });
                ep.set_subscribe_handler(
                    [&]
                    (std::uint16_t packet_id,
                     std::vector<std::tuple<std::string, std::uint8_t>> entries) {
                        std::cout << "subscribe received. packet_id: " << packet_id << std::endl;
                        std::vector<std::uint8_t> res;
                        res.reserve(entries.size());
                        ep.suback(packet_id, res);
                        for (auto const& e : entries) {
                            std::string const& topic = std::get<0>(e);
                            std::uint8_t qos = std::get<1>(e);
                            std::cout << "topic: " << topic  << " qos: " << static_cast<int>(qos) << std::endl;
                            res.emplace_back(qos);
                            subs_.emplace(topic, ep.shared_from_this(), qos);
                            auto it = retains_.find(topic);
                            if (it != retains_.end()) {
                                ep.publish(topic, it->contents, std::min(it->qos, qos), true);
                            }
                        }
                        return true;
                    }
                );
                ep.set_unsubscribe_handler(
                    [&]
                    (std::uint16_t packet_id,
                     std::vector<std::string> topics) {
                        std::cout << "unsubscribe received. packet_id: " << packet_id << std::endl;
                        for (auto const& topic : topics) {
                            subs_.erase(topic);
                        }
                        ep.unsuback(packet_id);
                        return true;
                    }
                );
                ep.set_pingreq_handler(
                    [&] {
                        ep.pingresp();
                        return true;
                    }
                );
            }
        );
        server_.listen();
    }

    void close() {
        server_.close();
    }

private:
    using con_t = mqtt::server<>::endpoint_t;
    using con_sp_t = std::shared_ptr<con_t>;

    void close_proc(con_t& con) {
        auto cs = con.clean_session();
        auto client_id = con.client_id();

        cons_.erase(con.shared_from_this());
        {
            auto& idx = subs_.get<tag_con>();
            auto r = idx.equal_range(con.shared_from_this());
            if (cs) {
                idx.erase(r.first, r.second);
            }
            else {
                sessions_.emplace(client_id);
                auto s = std::make_shared<session>(client_id);
                while (r.first != r.second) {
                    subsessions_.emplace(r.first->topic, s, r.first->qos);
                    r.first = idx.erase(r.first);
                }
            }
        }
    }

private:

    struct tag_topic {};
    struct tag_con {};
    struct tag_client_id {};

    struct sub_con {
        sub_con(std::string const& topic, con_sp_t const& con, std::uint8_t qos)
            :topic(topic), con(con), qos(qos) {}
        std::string topic;
        con_sp_t con;
        std::uint8_t qos;
    };
    using mi_sub_con = mi::multi_index_container<
        sub_con,
        mi::indexed_by<
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_MEMBER(sub_con, std::string, topic)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_con>,
                BOOST_MULTI_INDEX_MEMBER(sub_con, con_sp_t, con)
            >
        >
    >;

    struct retain {
        retain(std::string const& topic, std::uint8_t qos, std::string const& contents)
            :topic(topic), qos(qos), contents(contents) {}
        std::string topic;
        std::uint8_t qos;
        std::string contents;
    };
    using mi_retain = mi::multi_index_container<
        retain,
        mi::indexed_by<
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_MEMBER(retain, std::string, topic)
            >
        >
    >;

    struct session_data {
        session_data(std::string const& topic, std::string const& contents, std::uint8_t qos, bool retain)
            : topic(topic), contents(contents), qos(qos), retain(retain) {}
        std::string topic;
        std::string contents;
        std::uint8_t qos;
        bool retain;
    };
    struct session {
        explicit session(std::string const& client_id)
            :client_id(client_id) {}
        std::string client_id;
        std::vector<session_data> data;
    };
    struct sub_session {
        sub_session(std::string const& topic, std::shared_ptr<session> const& s, std::uint8_t qos)
            :topic(topic), s(s), qos(qos) {}
        std::string const& get_client_id() const {
            return s->client_id;
        }
        std::string topic;
        std::shared_ptr<session> s;
        std::uint8_t qos;
    };
    using mi_sub_session = mi::multi_index_container<
        sub_session,
        mi::indexed_by<
            mi::ordered_non_unique<
                mi::tag<tag_client_id>,
                BOOST_MULTI_INDEX_CONST_MEM_FUN(sub_session, std::string const&, sub_session::get_client_id)
            >,
            mi::ordered_non_unique<
                mi::tag<tag_topic>,
                BOOST_MULTI_INDEX_MEMBER(sub_session, std::string, topic)
            >
        >
    >;

    mqtt::server<> server_;
    std::set<con_sp_t> cons_;
    mi_sub_con subs_;
    std::set<std::string> sessions_;
    mi_sub_session subsessions_;
    mi_retain retains_;
};

#endif // MQTT_TEST_SERVER_NO_TLS_HPP
