/*
 * Copyright 2025 AsiaRF
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file	demo.c
 * @brief	ThingsBoard STM32 MQTT Demo by Wi-Fi HaLow. 
 * A solution by AsiaRF demonstrating MQTT communication using Wi-Fi HaLow for AWH575-001 
 * integration with ThingsBoard.io.
 * 
 * @note	It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 * 
 * @ref		demo.c is an example application that demonstrates how to use coreMQTT to connect to an MQTT 
 * broker and publish/subscribe to messages.  This example was written based on the tutorial at 
 * https://www.freertos.org/mqtt/basic-mqtt-example.html.
 * 
 * In this example we attempt to connect to the ThingsBoard public MQTT broker at @c demo.thingsboard.io
 * on port 1883. TLS is have not been tested yet, but you can enable it by setting @ref ENABLE_MQTTS to 1.
 * 
 * @note	This example is single threaded and so publishes & subscribes in lock step in a single thread.
 * If asynchronous publishes/subscribes are required, then this code will have to be modified to use a
 * message queue and a single thread to handle the MQTT publishes/subscribes. Alternatively you may use
 * the coreMQTT-Agent module which does this for you.
 * 
 * 
 * # Getting Started
 *
 * ## Using the app only
 * 
 * Simply compiling and running the application will show the application connecting to
 * the ThingsBoard MQTT broker and then publishing inittial telemetry and attribute messages.
 * The device also subscribes to the RPC request topic, so if all is well you should see
 * the messages being displayed on the console.
 * 
 * ## Configuration
 * 
 * See @ref APP_COMMON_API for details of WLAN and IP stack configuration. Additional
 * configuration options for this application can be found in the config.hjson file.
 * The ThingsBoard specific configuration options are in thingsboard_config.h.
 * 
 * # Troubleshooting
 *
 * ## Connecting to server socket failed with code 7
 *
 * The most common cause of this issue is AP configuration problems. 
 * Check if your device has access to the internet via your HaLow AP.
 *
 * Another possible cause is a tcp socket timeout, which can occur due to issues on
 * the broker side. If this occurs when using FreeRTOS+TCP as your
 * IP stack, try increasing the @c ipconfigSOCK_DEFAULT_RECEIVE_BLOCK_TIME to allow
 * the socket more time to receive a response.
 * 
 * If this error keeps occurring, try setting up a local MQTT broker
 * and changing @ref TB_SERVER_ENDPOINT to your computer's IP address.
 * 
 * ## Creating MQTT connection with broker failed with code 7
 * 
 * The main cause of this issue is issues with the Mosquitto test server, which can sometimes
 * fail to respond before @ref TB_MQTT_CONNECT_TIMEOUT. Try increasing the timeout, or if
 * the problem persists, try setting up a local MQTT broker and changing @ref TB_SERVER_ENDPOINT
 * to your computer's IP address.
 */


#include <string.h>
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"
#include "mmipal.h"
#include "mbedtls/build_info.h"
#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "core_mqtt.h"
#include "mm_app_common.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "main.h"
#include "arf_relay.h"
#include "arf_pwm.h"

#include "thingsboard_config.h"

#ifdef MQTT_QOS_LEVEL
	#if (MQTT_QOS_LEVEL < 0) || (MQTT_QOS_LEVEL > 2)
	#error "Invalid MQTT_QOS_LEVEL. Must be 0, 1, or 2"
	#endif
#endif


/** @brief Override the default FreeRTOS + TCP receive socket timeouts,
 *  as the test server can be slow to respond.
 */
#ifdef ipconfigSOCK_DEFAULT_RECEIVE_BLOCK_TIME
#define ipconfigSOCK_DEFAULT_RECEIVE_BLOCK_TIME		(10000)
#endif

/**
 * @brief Delay in ms between publishes
 * @note This is a single threaded demo, so increasing this delay will cause the
 *       application to sleep for extended periods of time and not respond to other messages.
 */
#define DELAY_BETWEEN_PUBLISHES			1000


/** Length of MAC address string (i.e., "XX:XX:XX:XX:XX:XX") including terminator. */
#define MAC_ADDR_STR_LEN				(18)


/** Hardware **/
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

static relay_config_t r1, r2, r3;
static relay_config_t *pr1 = &r1;
static relay_config_t *pr2 = &r2;
static relay_config_t *pr3 = &r3;

static pwm_config_t pwm;
static pwm_config_t *ppwm = &pwm;

/** Statically allocated buffer for MQTT */
static unsigned char buf[1024];


#if (MQTT_QOS_LEVEL != 0)

#define OUTGOING_PUBLISH_COUNT    10
#define INCOMING_PUBLISH_COUNT    10
static MQTTPubAckInfo_t outgoingPublishRecords[OUTGOING_PUBLISH_COUNT];
static MQTTPubAckInfo_t incomingPublishRecords[INCOMING_PUBLISH_COUNT];

#endif


MQTTStatus_t CreateMQTTConnectionToBroker(MQTTContext_t *pxMQTTContext, NetworkContext_t *pxNetworkContext, char *clientID, char *username);
MQTTStatus_t MQTTSubscribe(MQTTContext_t *pxMQTTContext, const char *topic);
MQTTStatus_t MQTTUnsubscribeFromTopic(MQTTContext_t *pxMQTTContext, const char *topic);
MQTTStatus_t MQTTPublishToTopic(MQTTContext_t *pxMQTTContext, const char *topic, void *payload, size_t payloadLength);
void mqtt_Dispatch(MQTTContext_t *pxMQTTContext, char *topic, char *payload);
void sub_topic(MQTTContext_t *pxMQTTContext, char *topic);
void pub_to_topic(MQTTContext_t *pxMQTTContext, char *topic, char *message);


/**
 * @brief This callback gets called when a published message matches
 *        one of our subscribed topics.
 * @param pxPublishInfo The received message
 */
static void MQTTProcessIncomingPublish(MQTTContext_t *pxMQTTContext, MQTTPublishInfo_t *pxPublishInfo)
{
	/* Strings are not zero terminated, so we need to explicitly copy and terminate them */
	static char tmptopic[80];
	static char tmppayload[128];
	size_t topic_name_length;
	size_t payload_length;

	topic_name_length = pxPublishInfo->topicNameLength;
	if (topic_name_length >= sizeof(tmptopic))
	{
		topic_name_length = sizeof(tmptopic) - 1;
	}

	strncpy(tmptopic, pxPublishInfo->pTopicName, topic_name_length);
	tmptopic[topic_name_length] = '\0';

	payload_length = pxPublishInfo->payloadLength;
	if (payload_length >= sizeof(tmppayload))
	{
		payload_length = sizeof(tmppayload) - 1;
	}
	strncpy(tmppayload, (char*)pxPublishInfo->pPayload, payload_length);
	tmppayload[payload_length] = '\0';

	printf( "Incoming Topic:    \"%s\"\n"
			"Incoming Message : \"%s\"\n",
			tmptopic,
			tmppayload);

	if (strncmp(tmptopic, TB_TOPIC_RPC_REQUEST_PREFIX, strlen(TB_TOPIC_RPC_REQUEST_PREFIX)) != 0) {
		UNUSED(pxMQTTContext);
		return;
	}
	mqtt_Dispatch(pxMQTTContext, tmptopic, tmppayload);
}

/**
 * @brief This callback gets called whenever we receive an @c ACK from the server.
 * @param pxIncomingPacket The incoming packet
 * @param usPacketId The packet ID
 */
static void MQTTProcessResponse(MQTTPacketInfo_t *pxIncomingPacket,
								uint16_t usPacketId)
{
	MQTTStatus_t xResult = MQTTSuccess;
	uint8_t * pucPayload = NULL;
	size_t ulSize = 0;

	(void) usPacketId;

	switch (pxIncomingPacket->type)
	{
        case MQTT_PACKET_TYPE_PUBACK: // 0x40U
            /* A PUBACK from the broker, containing the packet ID of a QoS1
             * publish, has been received.  The PUBACK will be matched with a
             * record in #xTopicFilterContext to ensure that it is valid. */
            //printf("Received PUBACK packet.\n");
            break;

		case MQTT_PACKET_TYPE_SUBACK: // 0x90U
			/* A SUBACK from the broker, containing the server response to our
			 * subscription request, has been received.  It contains the status
			 * code indicating server approval/rejection for the subscription to
			 * the single topic requested. The SUBACK will be parsed to obtain
			 * the status code, and this status code will be stored in
			 * #xTopicFilterContext. */
			xResult = MQTT_GetSubAckStatusCodes(pxIncomingPacket,
												&pucPayload,
												&ulSize);

			/* MQTT_GetSubAckStatusCodes always returns success if called with
			 * packet info from the event callback and non-NULL parameters. */
			MMOSAL_ASSERT(xResult == MQTTSuccess);
			break;

		case MQTT_PACKET_TYPE_UNSUBACK: // 0xB0U
			/* We should check which topic was unsubscribed to by looking at the packetid */
			printf("Unsubscribed from requested topic.\n");
			break;

		case MQTT_PACKET_TYPE_PINGRESP: // 0xD0U
			/* Nothing to be done from application as library handles
			 * PINGRESP with the use of MQTT_ProcessLoop API function. */
			printf("WARNING: PINGRESP should not be handled by the application "
					"callback when using MQTT_ProcessLoop.\n");
			break;

			/* Any other packet type is invalid. */
		default:
			printf("MQTTProcessResponse() called with unknown packet type:(%02X).\n",
					pxIncomingPacket->type);
	}
}

/**
 * @brief This is a callback from MQTT_Process whenever a packet is received from the server.
 * @param pxMQTTContext The MQTT context
 * @param pxPacketInfo The packet info
 * @param pxDeserializedInfo The de-serialized packet info
 */
static void EventCallback(	MQTTContext_t *pxMQTTContext,
							MQTTPacketInfo_t *pxPacketInfo,
							MQTTDeserializedInfo_t *pxDeserializedInfo)
{
	/* The MQTT context is not used for this demo. */
	(void) pxMQTTContext;

	if ((pxPacketInfo->type & 0xF0U) == MQTT_PACKET_TYPE_PUBLISH)
	{
		MQTTProcessIncomingPublish(pxMQTTContext, pxDeserializedInfo->pPublishInfo);
	}
	else
	{
		MQTTProcessResponse(pxPacketInfo, pxDeserializedInfo->packetIdentifier);
	}
}

/**
 * @brief Initializes an MQTT connection with the server
 * @param pxMQTTContext The MQTT context
 * @param pxNetworkContext The network context (socket)
 * @param clientID Our unique Client ID string
 * @return Returns @c MQTTSuccess on success, else returns error code
 */
MQTTStatus_t CreateMQTTConnectionToBroker(	MQTTContext_t *pxMQTTContext,
											NetworkContext_t *pxNetworkContext,
											char *clientID,
											char *username)
{
	MQTTStatus_t xResult;
	MQTTConnectInfo_t xConnectInfo;
	bool xSessionPresent;
	TransportInterface_t xTransport;
	MQTTFixedBuffer_t xBuffer;

    memset((void *)pxMQTTContext, 0x00, sizeof(MQTTContext_t));

	xBuffer.pBuffer = buf;
	xBuffer.size = sizeof(buf);

	/* Fill in Transport Interface send and receive function pointers. */
	memset(&xTransport, 0, sizeof(xTransport));
	xTransport.pNetworkContext = pxNetworkContext;
	xTransport.send = transport_send;
	xTransport.recv = transport_recv;

	/* Initialize MQTT library. */
	xResult = MQTT_Init(pxMQTTContext,
			            &xTransport,
			            mmosal_get_time_ms,
			            EventCallback,
			            &xBuffer);
	if (xResult != MQTTSuccess)  {
		return xResult;
	}
#if (MQTT_QOS_LEVEL != 0)
    else {
        /* Initialize Stateful QoS library. */
        xResult = MQTT_InitStatefulQoS( pxMQTTContext,
                                        outgoingPublishRecords,
                                        OUTGOING_PUBLISH_COUNT,
                                        incomingPublishRecords,
                                        INCOMING_PUBLISH_COUNT );
    }
#endif


	/* Many fields not used in this demo so start with everything at 0. */
	(void) memset((void *) &xConnectInfo, 0x00, sizeof(xConnectInfo));

	/* Start with a clean session i.e. direct the MQTT broker to discard any
	 * previous session data. Also, establishing a connection with clean
	 * session will ensure that the broker does not store any data when this
	 * client gets disconnected. */
	xConnectInfo.cleanSession = true;

	/* The client identifier is used to uniquely identify this MQTT client to
	 * the MQTT broker. In a production device the identifier can be something
	 * unique, such as a device serial number. */
	xConnectInfo.pClientIdentifier = clientID;
	xConnectInfo.clientIdentifierLength = (uint16_t) strlen(clientID);

	/* Set MQTT keep-alive period. It is the responsibility of the application
	 * to ensure that the interval between Control Packets being sent does not
	 * exceed the Keep Alive value.  In the absence of sending any other
	 * Control Packets, the Client MUST send a PINGREQ Packet. */
	xConnectInfo.keepAliveSeconds = TB_MQTT_KEEPALIVE;

	/* Set MQTT username and password. This is for thigsboard device access token */
	xConnectInfo.pUserName = username;
	xConnectInfo.userNameLength = (uint16_t) strlen(username);
    xConnectInfo.pPassword = NULL;
    xConnectInfo.passwordLength = 0;

	/* Send MQTT CONNECT packet to broker. LWT is not used in this demo, so it
	 * is passed as NULL. */
	xResult = MQTT_Connect(	pxMQTTContext,
							&xConnectInfo,
							NULL,
							TB_MQTT_CONNECT_TIMEOUT,
							&xSessionPresent);
	return xResult;
}

/**
 * @brief Subscribes to the specified topic
 * @param pxMQTTContext The MQTT context
 * @param topic The topic to subscribe to
 * @return Returns @c MQTTSuccess on success, else returns error code
 */
MQTTStatus_t MQTTSubscribe(	MQTTContext_t *pxMQTTContext,
							const char *topic)
{
	MQTTStatus_t xResult = MQTTSuccess;
	MQTTSubscribeInfo_t xMQTTSubscription[ TOPIC_COUNT ];
	uint16_t usSubscribePacketIdentifier;

	/* Some fields not used by this demo so start with everything at 0. */
	(void) memset((void *) &xMQTTSubscription, 0x00, sizeof(xMQTTSubscription));

	/* Each packet requires a unique ID. */
	usSubscribePacketIdentifier = MQTT_GetPacketId(pxMQTTContext);

	/* Subscribe to the pcExampleTopic topic filter. This example subscribes
	 * to only one topic and uses QoS0. */
	xMQTTSubscription[ 0 ].qos = MQTT_QOS;
	xMQTTSubscription[ 0 ].pTopicFilter = topic;
	xMQTTSubscription[ 0 ].topicFilterLength = strlen(topic);

	/* The client is already connected to the broker. Subscribe to the topic
	 * as specified in pcExampleTopic by sending a subscribe packet then
	 * waiting for a subscribe acknowledgment (SUBACK). */
	xResult = MQTT_Subscribe(	pxMQTTContext,
								xMQTTSubscription,
								1, /* Only subscribing to one topic. */
								usSubscribePacketIdentifier);
	if (xResult != MQTTSuccess) {
		return xResult;
	}

	/* Process incoming packet from the broker. After sending the
	 * subscribe, the client may receive a publish before it receives a
	 * subscribe ack. Therefore, call generic incoming packet processing
	 * function. Since this demo is subscribing to the topic to which no
	 * one is publishing, probability of receiving Publish message before
	 * subscribe ack is zero; but application must be ready to receive any
	 * packet.  This demo uses the generic packet processing function
	 * everywhere to highlight this fact. Note there is a separate demo that
	 * shows how to use coreMQTT in a thread safe way – in which case the
	 * MQTT protocol runs in the background and this call is not required. */
	xResult = MQTT_ProcessLoop(pxMQTTContext);
	return xResult;
}

/**
 * @brief Unsubscribes from the specified topic
 * @param pxMQTTContext The MQTT context
 * @param topic The topic to unsubscribe from
 * @return Returns @c MQTTSuccess on success, else returns error code
 */
MQTTStatus_t MQTTUnsubscribeFromTopic(	MQTTContext_t *pxMQTTContext,
										const char *topic)
{
	MQTTStatus_t xResult;
	MQTTSubscribeInfo_t xMQTTSubscription[ TOPIC_COUNT ];
	uint16_t usUnsubscribePacketIdentifier;

	/* Some fields not used by this demo so start with everything at 0. */
	(void) memset((void *) &xMQTTSubscription, 0x00, sizeof(xMQTTSubscription));

	/* Subscribe to the pcExampleTopic topic filter. This example subscribes
	 * to only one topic and uses QoS0. */
	xMQTTSubscription[ 0 ].qos = MQTT_QOS;
	xMQTTSubscription[ 0 ].pTopicFilter = topic;
	xMQTTSubscription[ 0 ].topicFilterLength = (uint16_t) strlen(topic);

	/* Each packet requires a unique ID. */
	usUnsubscribePacketIdentifier = MQTT_GetPacketId(pxMQTTContext);

	/* Send UNSUBSCRIBE packet. */
	xResult = MQTT_Unsubscribe(	pxMQTTContext,
								xMQTTSubscription,
								sizeof(xMQTTSubscription) / sizeof(MQTTSubscribeInfo_t),
								usUnsubscribePacketIdentifier);

	return xResult;
}

/**
 * @brief Publish a message to the specified MQTT topic
 * @param pxMQTTContext The MQTT context
 * @param topic The topic top publish to
 * @param payload A pointer to the binary or text data to publish
 * @param payloadLength The length of the data to publish
 * @return Returns @c MQTTSuccess on success, else returns error code
 */
MQTTStatus_t MQTTPublishToTopic(MQTTContext_t *pxMQTTContext,
								const char *topic,
								void *payload, size_t payloadLength)
{
	MQTTStatus_t xResult;
	MQTTPublishInfo_t xMQTTPublishInfo;
#if (MQTT_QOS_LEVEL != 0)
    uint16_t packetId;
#endif

	/* Some fields are not used by this demo so start with everything at 0. */
	(void) memset((void *) &xMQTTPublishInfo, 0x00, sizeof(xMQTTPublishInfo));

	/* This demo uses QoS0. */
	xMQTTPublishInfo.qos = MQTT_QOS;
	xMQTTPublishInfo.retain = false;
	xMQTTPublishInfo.pTopicName = topic;
	xMQTTPublishInfo.topicNameLength = (uint16_t) strlen(topic);
	xMQTTPublishInfo.pPayload = payload;
	xMQTTPublishInfo.payloadLength = payloadLength;

#if (MQTT_QOS_LEVEL != 0)
	/* Get a new packet id. */
	packetId = MQTT_GetPacketId( pxMQTTContext );

	/* For QoS1 and QoS2, the packet id is used to identify the publish
	 * and acknowledge it. */
	xResult = MQTT_Publish(pxMQTTContext, &xMQTTPublishInfo, packetId);
#else
	/* Send PUBLISH packet. Packet ID is not used for a QoS0 publish. */
	xResult = MQTT_Publish(pxMQTTContext, &xMQTTPublishInfo, 0U);
#endif

	return xResult;
}


/* Find string value for a JSON key in format: {"key":"value", ...} */
char *json_get_string(const char *json, const char *key)
{
	static char value[64];
	char pattern[32];
	snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);

	char *p = strstr(json, pattern);
	if (!p) return NULL;
	p += strlen(pattern);

	char *end = strchr(p, '"');
	if (!end) return NULL;

	size_t len = end - p;
	if (len >= sizeof(value)) {
		len = sizeof(value) - 1;
	}
	strncpy(value, p, len);
	value[len] = '\0';

	return value;
}


/* Find boolean value for a JSON key, returns true if key is "true", false otherwise */
bool json_get_bool(const char *json, const char *key)
{
	char pattern[32];
	snprintf(pattern, sizeof(pattern), "\"%s\":", key);

	char *p = strstr(json, pattern);
	if (!p) return false;
	p += strlen(pattern);

	// Skip quotes (if as sttrig)
	while (*p == ' ' || *p == '"') p++;

	if (strncmp(p, "true", 4) == 0) {
		return true;
	}

	return false;
}


/* Get integer value for a JSON key, like {"params":5} */
int json_get_int(const char *json, const char *key)
{
	char pattern[32];
	snprintf(pattern, sizeof(pattern), "\"%s\":", key);

	char *p = strstr(json, pattern);
	if (!p) return 0;
	p += strlen(pattern);

	// Skip spaces and quotes
	while (*p == ' ' || *p == '"') {
		p++;
	}

	return atoi(p);
}


/* Input: topic string, like "v1/devices/me/rpc/request/123"
Output: pointer to request id substring ("123") */
const char *extract_request_id(const char *topic)
{
	const char *prefix = TB_TOPIC_RPC_REQUEST_PREFIX;
	const char *p = strstr(topic, prefix);

	if (!p) return NULL;
	p += strlen(prefix);

	return p;
}

void mqtt_Dispatch(MQTTContext_t *pxMQTTContext, char *pTopic, char *pPayload)
{
	char message[128];
	char tel_resp[64];

	strncpy(message, pPayload, sizeof(message));

	if (strncmp(pTopic, TB_TOPIC_RPC_REQUEST, strlen(TB_TOPIC_RPC_REQUEST_PREFIX)) == 0) {
		// For thingsboard rpc request
		char *method = json_get_string(message, TB_JSON_KEY_METHOD);

		// If params is boolean or integer, use json_get_<type>
		if (strcmp(method, TB_RPC_METHOD_SWITCH1_SET) == 0) {
			bool b_onoff = json_get_bool(message, TB_JSON_KEY_PARAMS);
			if (b_onoff) {
				relay_on(pr1);
				snprintf(tel_resp, sizeof(tel_resp), "{\"%s\":%d}", TB_TELEMETRY_SWITCH1, 1);
			}
			else {
				relay_off(pr1);
				snprintf(tel_resp, sizeof(tel_resp), "{\"%s\":%d}", TB_TELEMETRY_SWITCH1, 0);
			}
		}
		else if (strcmp(method, TB_RPC_METHOD_SWITCH2_SET) == 0) {
			bool b_onoff = json_get_bool(message, TB_JSON_KEY_PARAMS);
			if (b_onoff) {
				relay_on(pr2);
				snprintf(tel_resp, sizeof(tel_resp), "{\"%s\":%d}", TB_TELEMETRY_SWITCH2, 1);
			}
			else {
				relay_off(pr2);
				snprintf(tel_resp, sizeof(tel_resp), "{\"%s\":%d}", TB_TELEMETRY_SWITCH2, 0);
			}
		}
		else if (strcmp(method, TB_RPC_METHOD_SWITCH3_SET) == 0) {
			bool b_onoff = json_get_bool(message, TB_JSON_KEY_PARAMS);
			if (b_onoff) {
				relay_on(pr3);
				snprintf(tel_resp, sizeof(tel_resp), "{\"%s\":%d}", TB_TELEMETRY_SWITCH3, 1);
			}
			else {
				relay_off(pr3);
				snprintf(tel_resp, sizeof(tel_resp), "{\"%s\":%d}", TB_TELEMETRY_SWITCH3, 0);
			}
		}
		else if (strcmp(method, TB_RPC_METHOD_KNOB_SET) == 0) {
			int level = json_get_int(message, TB_JSON_KEY_PARAMS);
            //printf("knob level=%d\n", pwm_level);            

			if (level >= LED_DIMMER_MIN_LEVEL && level <= LED_DIMMER_MAX_LEVEL) {
				int pwm_level = level * 10;
				pwm_set_level(ppwm, pwm_level);
			}
			snprintf(tel_resp, sizeof(tel_resp), "{\"%s\":%d}", TB_TELEMETRY_LED, level);
		}
	}

	// Reponse RPC
	char rpc_resp[64];
	snprintf(rpc_resp, sizeof(rpc_resp), "{\"%s\":%d}", TB_JSON_KEY_RESULT, 1);

	// Topic need to add requestId: extract the tail requestId from topic
	char rpc_resp_topic[96];
	sprintf(rpc_resp_topic, "%s%s", TB_TOPIC_RPC_RESPONSE, extract_request_id(pTopic));

	pub_to_topic(pxMQTTContext, rpc_resp_topic, rpc_resp);

	// Reponse status
	pub_to_topic(pxMQTTContext, TB_TOPIC_TELEMETRY, tel_resp);
    printf("\r\n");
}

void sub_topic(MQTTContext_t *pxMQTTContext, char *topic)
{
	MQTTStatus_t xMQTTStatus = MQTTSubscribe(pxMQTTContext, topic);

	if (xMQTTStatus != MQTTSuccess) {
		printf("failed with code %d\n", xMQTTStatus);
		return;
	}
	printf("Sub \"%s\" ok\n", topic);
}

void pub_to_topic(MQTTContext_t *pxMQTTContext, char *topic, char *message)
{
	MQTTStatus_t xMQTTStatus = MQTTPublishToTopic(pxMQTTContext, topic, message, strlen(message));

	if (xMQTTStatus != MQTTSuccess) {
		printf("failed with code %d\n", xMQTTStatus);
		return;
	}
	printf("Publish: \"%s\":\"%s\" ok\n", topic, message);
}

#if 0
#include "lwip/sockets.h"
#include "lwip/netdb.h"
void dns_test()
{
	printf("Start DNS test\n");
	//struct hostent *he = gethostbyname("www.google.com");
	struct hostent *he = gethostbyname(TB_SERVER_ENDPOINT);
	if (he && he->h_addr_list && he->h_addr_list[0]) {
		uint8_t *addr = (uint8_t*)he->h_addr_list[0];
		printf("DNS resolved %s: %u.%u.%u.%u\n",
				TB_SERVER_ENDPOINT, addr[0], addr[1], addr[2], addr[3]);
	} else {
		printf("DNS resolve failed!\n");
	}
}
#endif

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
	printf("\n\nAsiaRF ThingsBoard Demo (Built " __DATE__ " " __TIME__ ")\n\n");

	/* Initialize and connect to Wi-Fi, blocks till connected */
	app_wlan_init();
	app_wlan_start();

	NetworkContext_t xNetworkContext = { 0 };
	MQTTContext_t xMQTTContext;
	MQTTStatus_t xMQTTStatus;
	TransportStatus_t xNetworkStatus;

	/* Save space on stack by allocating static, no need to make this global */
	static char client_id[48];
	static char server[80];
	static char message[80];
	uint32_t port = TB_BROKER_PORT;
	uint32_t publish_delay = DELAY_BETWEEN_PUBLISHES;

	/* Generate Client ID & topic from MAC */
	uint8_t mac_addr[MMWLAN_MAC_ADDR_LEN] = { 0 };
	char mac_address_str[MAC_ADDR_STR_LEN];
	enum mmwlan_status status = mmwlan_get_mac_addr(mac_addr);
	if (status != MMWLAN_SUCCESS)
	{
		printf("Failed to read MAC address (status code %d)\n", status);
		return;
	}
//	snprintf(mac_address_str, sizeof(mac_address_str), "%02x:%02x:%02x:%02x:%02x:%02x",
//			mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    snprintf(mac_address_str, sizeof(mac_address_str), "%02x:%02x:%02x", mac_addr[3], mac_addr[4], mac_addr[5]);
	snprintf(client_id, sizeof(client_id), CLIENT_ID_PREFIX, mac_address_str);

	strncpy(server, TB_SERVER_ENDPOINT, sizeof(server));

	/*************************** Hardware driver. *********************************/

#define BISTABLE		0
#define MONOSTABLE		1

#if 0
#define RELAY_STABLE	BISTABLE
#else
#define RELAY_STABLE	MONOSTABLE
#endif

#if (RELAY_STABLE == BISTABLE)
	r1 = relay_config(&htim4, TIM_CHANNEL_1, RL1_RST_GPIO_Port, RL1_RST_Pin, BISTABLE_RELAY, GPIO_ACTIVE_NONE);
	r2 = relay_config(&htim4, TIM_CHANNEL_2, RL2_RST_GPIO_Port, RL2_RST_Pin, BISTABLE_RELAY, GPIO_ACTIVE_NONE);
	r3 = relay_config(&htim4, TIM_CHANNEL_3, RL3_RST_GPIO_Port, RL3_RST_Pin, BISTABLE_RELAY, GPIO_ACTIVE_NONE);
#elif (RELAY_STABLE == MONOSTABLE)
	r1 = relay_config(&htim4, TIM_CHANNEL_1, RL1_RST_GPIO_Port, RL1_RST_Pin, MONOSTABLE_RELAY, GPIO_ACTIVE_HIGH);
	r2 = relay_config(&htim4, TIM_CHANNEL_2, RL2_RST_GPIO_Port, RL2_RST_Pin, MONOSTABLE_RELAY, GPIO_ACTIVE_HIGH);
	r3 = relay_config(&htim4, TIM_CHANNEL_3, RL3_RST_GPIO_Port, RL3_RST_Pin, MONOSTABLE_RELAY, GPIO_ACTIVE_HIGH);
#endif

	pwm = pwm_config(&htim3, TIM_CHANNEL_3);
	pwm_start(ppwm);
	relay_off(pr1);
	relay_off(pr2);
	relay_off(pr3);

	/*************************** Connect. *********************************/

#if 0
	dns_test();
#endif

#if ENABLE_MQTTS
	/* Attempt to connect to the MQTT broker.  The socket is returned in
	 * the network context structure. We set NetworkCredentials to NULL to connect in the clear.
	 * Set this parameter if you wish to connect with TLS */

	NetworkCredentials_t xNetworkCredentials = {
		.pAlpnProtos	= NULL,				// or using alpn_protos
		.disableSni		= 0,				// 0 for enable SNI (mostly broker need this config)
		.pRootCa		= (const uint8_t *)root_ca_pem,
		.rootCaSize		= sizeof(root_ca_pem),
		.pClientCert	= NULL,				// or (const uint8_t *)client_cert_pem
		.clientCertSize	= 0,				// or sizeof(client_cert_pem)
		.pPrivateKey	= NULL,				// or (const uint8_t *)client_key_pem
		.privateKeySize	= 0					// or sizeof(client_key_pem)
	};
#endif /* ENABLE_MQTTS */

	printf(	"\r\n\r\n"
			"-----------------------------------\r\n"
			"  Start MQTT service\r\n"
			"-----------------------------------\r\n");
	printf("Connecting to server socket on %s:%ld...", server, port);

	while(1) {
#if ENABLE_MQTTS
		xNetworkStatus = transport_connect(&xNetworkContext, server, (uint16_t) port, &xNetworkCredentials);
#else
		xNetworkStatus = transport_connect(&xNetworkContext, server, (uint16_t) port, NULL);
#endif /* ENABLE_MQTTS */

		if (xNetworkStatus == TRANSPORT_SUCCESS) {
			printf(" ok\r\n");
			break;
		} else {
			printf("Plain MQTT connection failed with code %d\n", xNetworkStatus);
			printf("Wait for 10 seconds...\r\n");
			mmosal_task_sleep(10 * 1000);
			printf("Retry to connect.\r\n\r\n");
		}
	}

	/* Connect to the MQTT broker using the already connected TCP socket. */
	printf("Client \"%s\" Creating MQTT connection with broker...", client_id);

	while(1) {
		xMQTTStatus = CreateMQTTConnectionToBroker(&xMQTTContext, &xNetworkContext, client_id, TB_DEVICE_TOKEN_MAIN);
		if (xMQTTStatus == MQTTSuccess) {
			printf(" ok\r\n\r\n");
			break;
		}
		else {
			printf("failed with code %d\n", xMQTTStatus);
			//transport_disconnect(&xNetworkContext);

			printf("Wait for 10 seconds...\n");
			mmosal_task_sleep(10 * 1000);
			printf("Retry to connect to broker.\r\n\r\n");
		}
	}

	/**************************** Subscribe. ******************************/

	/** Discovery message */
	printf("--- Start to subscribe to topic.\n");

	/* Subscribe to the awh575-001 device on ThingsBoard. */
	sub_topic(&xMQTTContext, TB_TOPIC_RPC_REQUEST);
	printf("\n");

	/******************* Publish and Keep Alive Loop. *********************/

	/** Publish initial state */
	printf("--- Start to initial state message.\n");    

	/** Publish relay 1/2/3 state message */
	snprintf(message, sizeof(message), "{\"%s\":%d}", TB_TELEMETRY_SWITCH1, 0);
	pub_to_topic(&xMQTTContext, TB_TOPIC_TELEMETRY, message);
	snprintf(message, sizeof(message), "{\"%s\":%d}", TB_TELEMETRY_SWITCH2, 0);
	pub_to_topic(&xMQTTContext, TB_TOPIC_TELEMETRY, message);
	snprintf(message, sizeof(message), "{\"%s\":%d}", TB_TELEMETRY_SWITCH3, 0);
	pub_to_topic(&xMQTTContext, TB_TOPIC_TELEMETRY, message);

	/** Publish LED PWM state message */
	snprintf(message, sizeof(message), "{\"%s\":%d}", TB_TELEMETRY_LED, LED_DIMMER_DEFAULT_LEVEL);
	pub_to_topic(&xMQTTContext, TB_TOPIC_TELEMETRY, message);
	printf("\n");

	/** Start loop */
	printf("--- MQTT Looping\n");

	while (1) {
		/* Process the incoming publish echo. Since the application subscribed
		 * to the same topic, the broker will send the same publish message
		 * back to the application.  Note there is a separate demo that
		 * shows how to use coreMQTT in a thread safe way - in which case the
		 * MQTT protocol runs in the background and this call is not
		 * required. */

		xMQTTStatus = MQTT_ProcessLoop(&xMQTTContext);
		if (xMQTTStatus != MQTTSuccess)
		{
			printf("MQTT_ProcessLoop() failed with code %d\n", xMQTTStatus);
		}

		/* Leave the connection idle for some time. */
		mmosal_task_sleep(publish_delay);
	}

	/******************** Unsubscribe from the topic. *********************/

	xMQTTStatus = MQTTUnsubscribeFromTopic(&xMQTTContext, TB_TOPIC_RPC_REQUEST);
	if (xMQTTStatus != MQTTSuccess)
	{
		printf("MQTTUnsubscribeFromTopic() failed with code %d\n", xMQTTStatus);
		goto quit;
	}

	/* Process the incoming packet from the broker.  Note there is a separate
	 * demo that shows how to use coreMQTT in a thread safe way - in which case
	 * the MQTT protocol runs in the background and this call is not required. */
	xMQTTStatus = MQTT_ProcessLoop(&xMQTTContext);
	if (xMQTTStatus != MQTTSuccess)
	{
		printf("MQTT_ProcessLoop() failed with code %d\n", xMQTTStatus);
		goto quit;
	}

	/**************************** Disconnect. *****************************/

quit:
	/* Disconnect from broker. */
	printf("Disconnecting from server and closing socket.\n");
	xMQTTStatus = MQTT_Disconnect(&xMQTTContext);
	if (xMQTTStatus != MQTTSuccess)
	{
		printf("MQTT_Disconnect() failed with code %d\n", xMQTTStatus);
	}

	/* Close the network connection. */
	transport_disconnect(&xNetworkContext);
}
