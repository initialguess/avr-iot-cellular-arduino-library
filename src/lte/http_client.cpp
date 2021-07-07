#include "http_client.h"
#include "sequans_controller.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// We only use profile 0 to keep things simple we also stick with spId 1
// TODO/INPUT WANTED: Should we allow for more profiles and different spId?
#define HTTP_CONFIGURE "AT+SQNHTTPCFG=0,\"%s\",%u,0,\"\",\"\",%u,120,1,1"

// Command without any data in it (with parantheses): 36 bytes
// Max length of doman name: 127 bytes
// Max length of port number: 5 bytes (0-65535)
// TLS enabled: 1 byte
// Termination: 1 byte
// This results in 36 + 127 + 5 + 1 + 1 = 170
#define HTTP_CONFIGURE_SIZE 170

#define HTTP_SEND    "AT+SQNHTTPSND=0,%u,\"%s\",%u"
#define HTTP_RECEIVE "AT+SQNHTTPRCV=0,%u"
#define HTTP_QUERY   "AT+SQNHTTPQRY=0,%u,\"%s\""

#define HTTP_POST_METHOD   0
#define HTTP_PUT_METHOD    1
#define HTTP_GET_METHOD    0
#define HTTP_HEAD_METHOD   1
#define HTTP_DELETE_METHOD 2

#define HTTP_RECEIVE_LENGTH          32
#define HTTP_RECEIVE_START_CHARACTER '<'

#define HTTP_RESPONSE_MAX_LENGTH         128
#define HTTP_RESPONSE_STATUS_CODE_INDEX  1
#define HTTP_RESPONSE_STATUS_CODE_LENGTH 3
#define HTTP_RESPONSE_DATA_SIZE_INDEX    3
#define HTTP_RESPONSE_DATA_SIZE_LENGTH   16

// These are limitations from the Sequans module, so the range of bytes we can
// receive with one call to the read body AT command has to be between these
// values. One thus has to call the function multiple times if the data size is
// greater than the max size
#define HTTP_BODY_BUFFER_MIN_SIZE 64
#define HTTP_BODY_BUFFER_MAX_SIZE 1500

/**
 * @brief Waits for the HTTP response (which can't be requested) puts it into a
 * buffer.
 *
 * Since we can't query the response, and it will arrive as a single line of
 * string, we do the trick of sending a single AT command after we first see
 * that the receive buffer is not empty. The AT command will only give "OK" in
 * response, but we can use that as a termination for the HTTP response.
 *
 * @param buffer Buffer to place the HTTP response in.
 * @param buffer_size Size of buffer to place HTTP response in.
 *
 * @return Relays the return code from sequansControllerFlushResponse().
 *         SEQUANS_CONTROLLER_RESPONSE_OK if ok.
 */
static ResponseResult waitAndRetrieveHttpResponse(char *buffer,
                                                  const size_t buffer_size) {
    // Wait until the receive buffer is filled with something from the HTTP
    // response
    while (!sequansControllerIsRxReady()) {}

    // Send single AT command in order to receive an OK which will later will be
    // searched for as the termination in the HTTP response
    sequansControllerWriteCommand("AT");

    return sequansControllerReadResponse(buffer, buffer_size);
}

/**
 * @brief Generic method for sending data via HTTP, either with POST or PUT.
 * Issues an AT command to the LTE modem.
 *
 * @param endpoint Destination of payload, part after host name in URL.
 * @param data Payload to send.
 * @param method POST(0) or PUT(1).
 */
static HttpResponse
sendData(const char *endpoint, const char *data, const uint8_t method) {

    HttpResponse httpResponse = {0, 0};

    // Clear the receive buffer to be ready for the response
    while (sequansControllerIsRxReady()) { sequansControllerFlushResponse(); }

    // Setup and transmit SEND command before sending the data
    const uint32_t data_lenth = strlen(data);
    const uint32_t digits_in_data_length = trunc(log10(data_lenth)) + 1;

    char command[strlen(HTTP_SEND) + strlen(endpoint) + digits_in_data_length];
    sprintf(command, HTTP_SEND, method, endpoint, data_lenth);
    sequansControllerWriteCommand(command);

    // Now we deliver the payload
    sequansControllerWriteCommand(data);
    if (sequansControllerFlushResponse() != OK) {
        return httpResponse;
    }

    char http_response[HTTP_RESPONSE_MAX_LENGTH] = "";
    if (waitAndRetrieveHttpResponse(http_response, HTTP_RESPONSE_MAX_LENGTH) !=
        OK) {
        return httpResponse;
    }

    char http_status_code_buffer[HTTP_RESPONSE_STATUS_CODE_LENGTH + 1] = "";

    bool got_response_code = sequansControllerExtractValueFromCommandResponse(
        http_response,
        HTTP_RESPONSE_STATUS_CODE_INDEX,
        http_status_code_buffer,
        HTTP_RESPONSE_STATUS_CODE_LENGTH + 1);

    char data_size_buffer[HTTP_RESPONSE_DATA_SIZE_LENGTH] = "";

    bool got_data_size = sequansControllerExtractValueFromCommandResponse(
        http_response,
        HTTP_RESPONSE_DATA_SIZE_INDEX,
        data_size_buffer,
        HTTP_RESPONSE_DATA_SIZE_LENGTH);

    if (got_response_code) {
        httpResponse.status_code = atoi(http_status_code_buffer);
    }

    if (got_data_size) {
        httpResponse.data_size = atoi(data_size_buffer);
    }

    return httpResponse;
}

/**
 * @brief Generic method for retrieving data via HTTP, either with HEAD, GET or
 * DELETE.
 *
 * @param endpoint Destination of retrieve, part after host name in URL.
 * @param method GET(0), HEAD(1) or DELETE(2).
 */
static HttpResponse queryData(const char *endpoint, const uint8_t method) {

    HttpResponse httpResponse = {0, 0};

    // Clear the receive buffer to be ready for the response
    while (sequansControllerIsRxReady()) { sequansControllerFlushResponse(); }

    // Set up and send the query
    char command[strlen(HTTP_QUERY) + strlen(endpoint)];
    sprintf(command, HTTP_QUERY, method, endpoint);
    sequansControllerWriteCommand(command);

    if (sequansControllerFlushResponse() != OK) {
        return httpResponse;
    }

    char http_response[HTTP_RESPONSE_MAX_LENGTH] = "";
    if (waitAndRetrieveHttpResponse(http_response, HTTP_RESPONSE_MAX_LENGTH) !=
        OK) {
        return httpResponse;
    }

    char http_status_code_buffer[HTTP_RESPONSE_STATUS_CODE_LENGTH + 1] = "";

    bool got_response_code = sequansControllerExtractValueFromCommandResponse(
        http_response,
        HTTP_RESPONSE_STATUS_CODE_INDEX,
        http_status_code_buffer,
        HTTP_RESPONSE_STATUS_CODE_LENGTH + 1);

    char data_size_buffer[HTTP_RESPONSE_DATA_SIZE_LENGTH] = "";

    bool got_data_size = sequansControllerExtractValueFromCommandResponse(
        http_response,
        HTTP_RESPONSE_DATA_SIZE_INDEX,
        data_size_buffer,
        HTTP_RESPONSE_DATA_SIZE_LENGTH);

    if (got_response_code) {
        httpResponse.status_code = atoi(http_status_code_buffer);
    }

    if (got_data_size) {
        httpResponse.data_size = atoi(data_size_buffer);
    }

    return httpResponse;
}

bool httpClientConfigure(const char *host,
                         const uint16_t port,
                         const bool enable_tls) {

    char command[HTTP_CONFIGURE_SIZE] = "";
    sprintf(command, HTTP_CONFIGURE, host, port, enable_tls);
    sequansControllerWriteCommand(command);

    return (sequansControllerFlushResponse() == OK);
}

HttpResponse httpClientPost(const char *endpoint, const char *data) {
    return sendData(endpoint, data, HTTP_POST_METHOD);
}

HttpResponse httpClientPut(const char *endpoint, const char *data) {
    return sendData(endpoint, data, HTTP_PUT_METHOD);
}

HttpResponse httpClientGet(const char *endpoint) {
    return queryData(endpoint, HTTP_GET_METHOD);
}

HttpResponse httpClientHead(const char *endpoint) {
    return queryData(endpoint, HTTP_HEAD_METHOD);
}

HttpResponse httpClientDelete(const char *endpoint) {
    return queryData(endpoint, HTTP_DELETE_METHOD);
}

int16_t httpClientReadResponseBody(char *buffer, const uint32_t buffer_size) {

    // Safeguard against the limitation in the Sequans AT command parameter
    // for the response receive command.
    if (buffer_size < HTTP_BODY_BUFFER_MIN_SIZE ||
        buffer_size > HTTP_BODY_BUFFER_MAX_SIZE) {
        return -1;
    }

    // Clear the receive buffer to be ready for the response
    while (sequansControllerIsRxReady()) { sequansControllerFlushResponse(); }

    // We send the buffer size with the receive command so that we only
    // receive that. The rest will be flushed from the modem.
    char command[HTTP_RECEIVE_LENGTH] = "";

    sprintf(command, HTTP_RECEIVE, buffer_size);
    sequansControllerWriteCommand(command);

    // We receive three start bytes of the character '<', so we wait for them
    uint8_t start_bytes = 3;

    // Wait for first byte in receive buffer
    while (!sequansControllerIsRxReady()) {}

    while (start_bytes > 0) {

        // This will block until we receive
        if (sequansControllerReadByte() == HTTP_RECEIVE_START_CHARACTER) {
            start_bytes--;
        }
    }

    // Now we are ready to receive the payload. We only check for error and not
    // overflow in the receive buffer in comparison to our buffer as we know the
    // size of what we want to receive
    if (sequansControllerReadResponse(buffer, buffer_size) == ERROR) {

        return 0;
    }

    size_t response_length = strlen(buffer);

    // Remove extra <CR><LF> from command response
    memset(buffer + response_length - 2, 0, 2);

    return response_length - 2;
}