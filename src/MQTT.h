#pragma once

#include <string>
#include <functional>

#include <mosquittopp.h>
#include <msgpack.hpp>
#include <iostream>
#include <thread>
#include <utility>

namespace MQTT {
    static const uint16_t k_std_mqtt_port = 1883;
    static const uint16_t k_std_mqtt_ssl_port = 8883;

    enum QOS{
      AT_MOST_ONCE = 0,
      AT_LEAST_ONCE = 1,
      EXACTLY_ONCE = 2
    };

    class MQTTMessage {
    public:
        std::string message;
    };

    struct MQTTServerSettings {
        MQTTServerSettings(const std::string &host, const int port, const int qos) : host(host),
                                                                                     port(port),
                                                                                     QoS(qos)
        {}
        std::string host;
        int port = k_std_mqtt_port;
        int QoS = QOS::AT_LEAST_ONCE;
    };


    template<class MessageType>
    class MQTTSubscriber : public mosqpp::mosquittopp {

    public:
        MQTTSubscriber(const std::string& clientId,
                       const MQTTServerSettings& server,
                       const std::string& topic,
                       std::function<void(std::shared_ptr<MessageType>&)> function)
                       : mosquittopp(clientId.c_str())
                       , clientId_(clientId)
                       , serverSettings_(server)
                       , topic_(topic)
                       , receive_callback_(function)
        {
            connect_async(serverSettings_.host.c_str(), serverSettings_.port);
            loop_start();
        }

        ~MQTTSubscriber() override = default;

        void setCallback(std::function<void(std::shared_ptr<MessageType>&)> function) {
            receive_callback_ = function;
        }

        void unsubscribe() {
            mosquittopp::unsubscribe(nullptr, topic_.c_str());
        }

        /**
         * Subscriber will subscribe to the new topic
         * @param topic Topic to which to subscribe
         */
        void subscribe(const std::string& topic) {
            unsubscribe();
            topic_ = topic;
            subscribe();
        }

        void setErrorHandler(std::function<bool(const std::string &)> error_handler){
            error_handler_ = error_handler;
        }

        void resetServerSettings(const MQTTServerSettings& server){
          unsubscribe();
          disconnect();
          serverSettings_ = server;

          connect_async(serverSettings_.host.c_str(), serverSettings_.port);
          subscribe();
        }


    private:
        void on_connect(int rc) override {
            mosquittopp::subscribe(nullptr, topic_.c_str());
        }

        void on_message(const struct mosquitto_message* message) override {
            msgpack::object_handle result;
            try {
                msgpack::unpack(result, static_cast<const char *>(message->payload), message->payloadlen);
                msgpack::object obj = result.get();

                std::shared_ptr<MessageType> msgPtr(new MessageType());
                obj.convert(*msgPtr.get());
                receive_callback_(msgPtr);
            }
            catch ( msgpack::type_error &){
                if (error_handler_ && error_handler_("Message has wrong type") == 1) {
                    //error handler wants to supress the error
                    return;
                }

                throw std::bad_cast(); //was not able to cast message
            }
        }

        /**
         * Resubscriber to the the previously subsrcibed topic
         */
        void subscribe() {
          mosquittopp::subscribe(nullptr, topic_.c_str());
        }

        std::string clientId_;
        MQTTServerSettings serverSettings_;
        std::string topic_;
        std::function<void(std::shared_ptr<MessageType>&)> receive_callback_;
        std::function<bool(const std::string &)> error_handler_;
    };
}
