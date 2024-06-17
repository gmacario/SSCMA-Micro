#ifndef _MA_TRANSPORT_MQTT_H_
#define _MA_TRANSPORT_MQTT_H_

#include <vector>


#include "hv/hv.h"
#include "hv/mqtt_client.h"

#include "core/utils/ma_ringbuffer.hpp"

#include "porting/ma_osal.h"
#include "porting/ma_transport.h"

namespace ma {

class MQTT final : public Transport {
public:
    MQTT(const char* clientID, const char* txTopic, const char* rxTopic);

    ~MQTT() override;

    operator bool() const override;

    size_t available() const override;
    size_t send(const char* data, size_t length, int timeout = -1) override;
    size_t receive(char* data, size_t length, int timeout = 1) override;

    ma_err_t connect(const char* host,
                     int port,
                     const char* username = nullptr,
                     const char* password = nullptr,
                     bool useSSL          = false);
    ma_err_t disconnect();

protected:
    void onCallback(mqtt_client_t* client, int type);
    void threadEntry();

private:
    static void threadEntryStub(void* param);
    static void onCallbackStub(mqtt_client_t* client, int type);
    mqtt_client_t* m_client;
    bool m_connected;
    std::string m_txTopic;
    std::string m_rxTopic;
    std::string m_clientID;
    std::string m_username;
    std::string m_password;
    bool m_useSSL;
    Thread* m_thread;

    ring_buffer<char> m_receiveBuffer;
};

}  // namespace ma

#endif  // _MA_TRANSPORT_MQTT_H_
