/**
 * @file thingsboard_config.h
 * @brief ThingsBoard MQTT configuration macros for STM32 demo application
 * @author Leo Hsiao
 * @date 2025-09-26
 */

#ifndef __THINGSBOARD_CONFIG_H__
#define __THINGSBOARD_CONFIG_H__

#include "core_mqtt.h"

// ===== THINGSBOARD CONNECTION SETTINGS =====

/**
 * @brief Enable for MQTT TLS connector.
 * @note Need setting in Thingsboard for TLS connection.
 * @note MQTTS does not tested yet.
 */
#define ENABLE_MQTTS					0

/**
 * @brief The MQTT client identifier used in this example.
 * Each client identifier must be unique to ensure no two clients 
 * connecting to the same broker use the same client identifier.
 */
#define CLIENT_ID_PREFIX				"AWH575-001_%s"

/** @brief ThingsBoard server hostname or IP address.
 * Default demo server is "demo.thingsboard.io" for testing purposes.
 * Replace with your production ThingsBoard server ip address or domain name.
 */
#define TB_SERVER_ENDPOINT				"demo.thingsboard.io"

/** @brief MQTT broker port.
 * Port 1883 is used for plain MQTT communications (no encryption)
 * Port 8883 is used for MQTT over TLS/SSL encrypted communications.
 * Current setting uses port 8883 but ENABLE_MQTTS is 0 - this may cause connection failure.
 */
#if ENABLE_MQTTS
	#define TB_BROKER_PORT				8883	// TLS/SSL encrypted MQTT
#else
	#define TB_BROKER_PORT				1883	// Plain MQTT without encryption
#endif

/** @brief Keep-alive interval in seconds for MQTT connection.
 * This defines how often the client sends PING messages to maintain connection.
 * Recommended range: 30-300 seconds. Too short may cause unnecessary traffic,
 * too long may delay detection of connection loss.
 */
#define TB_MQTT_KEEPALIVE				60

/** @brief Number of topics we subscribe to */
#define TOPIC_COUNT						1

// ===== MQTT QOS SETTINGS =====

/** @brief QoS level for MQTT.
 * QoS0: At most once delivery (fire and forget)
 * QoS1: At least once delivery (recommended for telemetry)
 * QoS2: Exactly once delivery (highest reliability, more overhead)
 * @note QoS2 have not been tested yet.
 */
#define MQTT_QOS_LEVEL					1

#if (MQTT_QOS_LEVEL == 0)
	#define MQTT_QOS					MQTTQoS0
#elif (MQTT_QOS_LEVEL == 1) 
	#define MQTT_QOS					MQTTQoS1
#elif (MQTT_QOS_LEVEL == 2)
	#define MQTT_QOS					MQTTQoS2
#else
	#error "Invalid MQTT_QOS_LEVEL. Must be 0, 1, or 2"
#endif

// ===== MQTTS CONNECTION SETTINGS =====

#if ENABLE_MQTTS

/** @brief Root CA certificate for TLS/SSL connection to ThingsBoard.
 * This certificate is used to verify the ThingsBoard server identity.
 * Replace 'x' characters with actual certificate content when using TLS.
 * Not used when ENABLE_MQTTS is 0 (plain MQTT connection).
 */
static const char root_ca_pem[] =
"-----BEGIN CERTIFICATE-----\n"
"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
// ... (keep your certificate content here)
"-----END CERTIFICATE-----\n";

#endif /* ENABLE_MQTTS */

// ===== DEVICE ACCESS TOKENS =====

/** @brief Primary device access token from ThingsBoard.
 * This token is generated in ThingsBoard device management panel.
 * Each device must have a unique token for authentication from 
 * "Device Credentials" in Thingsborard devices.
 */
#define TB_DEVICE_TOKEN_MAIN			"YOUR_MAIN_DEVICE_TOKEN_HERE"

// ===== MQTT TOPIC DEFINITIONS =====

/** @brief MQTT topic for publishing telemetry data to ThingsBoard.
 * Standard ThingsBoard topic for time-series data (sensor readings, status updates).
 * Data published here appears in device telemetry tab and can be visualized
 * in dashboards and used in rule chains.
 */
#define TB_TOPIC_TELEMETRY				"v1/devices/me/telemetry"

/** @brief MQTT topic for publishing device attributes to ThingsBoard.
 * Used for device configuration, firmware version, and static properties.
 * Attributes are displayed in device details and can be used for device management.
 */
#define TB_TOPIC_ATTRIBUTES				"v1/devices/me/attributes"

/** @brief MQTT topic for subscribing to RPC requests from ThingsBoard.
 * Uses wildcard (+) to receive all RPC requests sent from dashboard widgets.
 * Format: v1/devices/me/rpc/request/{requestId}
 * Device must subscribe to this topic to receive control commands.
 */
#define TB_TOPIC_RPC_REQUEST			"v1/devices/me/rpc/request/+"

/** @brief MQTT topic for subscribing to RPC requests without wildcard for strig compare.
 */
#define TB_TOPIC_RPC_REQUEST_PREFIX		"v1/devices/me/rpc/request/"

/** @brief MQTT topic prefix for publishing RPC responses back to ThingsBoard.
 * Must append the requestId from incoming RPC request to complete the topic.
 * Format: v1/devices/me/rpc/response/{requestId}
 * Response confirms command execution status to the dashboard.
 */
#define TB_TOPIC_RPC_RESPONSE			"v1/devices/me/rpc/response/"

// ===== SWITCH CONTROL RPC METHOD NAMES =====

/** @brief RPC method name for Switch Control Widget #1.
 * Corresponds to main power control switch in ThingsBoard dashboard.
 * Widget "RPC set value method" field must match this exact string.
 * Payload format: {"method":"main_power_switch","params":true/false}
 */
#define TB_RPC_METHOD_SWITCH1_SET		"r1_setValue"
#define TB_RPC_METHOD_SWITCH2_SET		"r2_setValue"
#define TB_RPC_METHOD_SWITCH3_SET		"r3_setValue"

// ===== KNOB CONTROL RPC METHOD NAMES =====

/** @brief RPC method name for setting Knob Control Widget value.
 * Used by ThingsBoard knob widget to send LED dimming level (0-10).
 * Payload format: {"method":"led_setValue","params":5}
 * Parameter range: 0 (off) to 10 (maximum brightness).
 */
#define TB_RPC_METHOD_KNOB_SET			"led_setValue"

// ===== LED DIMMING CONTROL SETTINGS =====

/** @brief Minimum dimming level for LED control.
 * Level 0 corresponds to completely OFF state (0% brightness).
 * PWM duty cycle = 0% when dimmer level equals this value.
 */
#define LED_DIMMER_MIN_LEVEL			0

/** @brief Maximum dimming level for LED control.
 * Level 10 corresponds to full brightness (100% intensity).
 * This matches ThingsBoard knob widget maximum range setting.
 * PWM duty cycle = 100% when dimmer level equals this value.
 */
#define LED_DIMMER_MAX_LEVEL			10

/** @brief Default LED dimming level on system startup.
 * Initial brightness setting when device boots up.
 * Middle range value provides balanced startup illumination.
 * Corresponds to 50% brightness level.
 */
#define LED_DIMMER_DEFAULT_LEVEL		0

// ===== MQTT RPC RESPONSE CODES =====

/** @brief MQTT RPC response - Command executed successfully.
 * Device executed the RPC command without errors.
 * Response payload should contain result data or simple acknowledgment.
 * ThingsBoard dashboard widget will show successful operation state.
 * Example response: {"result": "success"} or simple value like "OK"
 */
#define TB_RPC_SUCCESS					0

/** @brief MQTT RPC response - Invalid parameter in RPC request.
 * Returned when RPC command parameters are out of range or wrong data type.
 * Examples: dimmer level > 10, switch parameter is not boolean, or missing required params.
 * Response payload: {"error": "Invalid parameter", "code": 1}
 */
#define TB_RPC_ERROR_INVALID_PARAM		1

/** @brief MQTT RPC response - RPC method not implemented on device.
 * Device firmware doesn't recognize the requested method name.
 * Indicates mismatch between ThingsBoard widget RPC method configuration and device code.
 * Response payload: {"error": "Method not found", "code": 2}
 */
#define TB_RPC_ERROR_METHOD_NOT_FOUND	2

/** @brief MQTT RPC response - Internal device error during command execution.
 * Hardware failure, GPIO error, PWM initialization failure, or other system-level problems.
 * Device received and parsed command correctly but cannot execute due to internal issues.
 * Response payload: {"error": "Internal error", "code": 3}
 */
#define TB_RPC_ERROR_INTERNAL			3

/** @brief MQTT RPC response - Command timeout or execution timeout.
 * Device received command but execution took too long or got stuck.
 * Useful for operations that involve delays or hardware initialization.
 * Response payload: {"error": "Execution timeout", "code": 4}
 */
#define TB_RPC_ERROR_TIMEOUT			4

/** @brief MQTT RPC response - Device busy, cannot process command now.
 * Device is performing another operation and cannot handle new RPC requests.
 * Widget should retry the command after some delay.
 * Response payload: {"error": "Device busy", "code": 5}
 */
#define TB_RPC_ERROR_DEVICE_BUSY		5

// ===== MQTT RPC RESPONSE FORMATS =====

/** @brief JSON key for RPC response result data.
 * Used in successful RPC responses to return command execution results.
 * For getValue commands: {"result": 7}
 * For setValue commands: {"result": "success"}
 */
#define TB_JSON_KEY_RESULT				"result"

/** @brief JSON key for RPC response error message.
 * Used in failed RPC responses to describe what went wrong.
 * Format: {"error": "description", "code": error_code}
 */
#define TB_JSON_KEY_ERROR				"error"

/** @brief JSON key for RPC response error code.
 * Numeric error code for programmatic error handling on ThingsBoard side.
 * Format: {"error": "description", "code": 1}
 */
#define TB_JSON_KEY_ERROR_CODE			"code"

// ===== JSON PAYLOAD KEYS =====

/** @brief JSON key for RPC method name in incoming requests.
 * Standard ThingsBoard RPC payload structure uses "method" field
 * to specify which device function should be executed.
 * Example: {"method":"led_dimmer_set","params":7}
 */
#define TB_JSON_KEY_METHOD				"method"

/** @brief JSON key for RPC parameters in incoming requests.
 * Contains the actual command data (boolean for switches, integer for knob).
 * Parameter type and range depends on the specific RPC method being called.
 * Example: {"method":"main_power_switch","params":true}
 */
#define TB_JSON_KEY_PARAMS				"params"

/** @brief JSON key for switch state value in responses.
 * Used when responding to getValue RPC calls for switch widgets.
 * Boolean value indicating current switch state (true=ON, false=OFF).
 */
#define TB_JSON_KEY_SWITCH_VALUE		"value"

/** @brief JSON key for knob level value in responses.
 * Used when responding to getValue RPC calls for knob widget.
 * Integer value (0-10) indicating current dimming level.
 */
#define TB_JSON_KEY_KNOB_LEVEL			"level"

// ===== DEVICE STATUS TELEMETRY KEYS =====

/** @brief Telemetry key for reporting switch #1 status.
 * Boolean telemetry data showing main power switch state.
 * Appears in ThingsBoard device telemetry tab and dashboard widgets.
 * Updated automatically when switch state changes.
 */
#define TB_TELEMETRY_SWITCH1			"r1_value"
#define TB_TELEMETRY_SWITCH2			"r2_value"
#define TB_TELEMETRY_SWITCH3			"r3_value"

/** @brief Telemetry key for reporting current LED dimming level.
 * Integer telemetry (0-10) showing actual LED brightness setting.
 * Synchronized with knob widget value for real-time feedback.
 * Useful for energy monitoring and usage analytics.
 */
#define TB_TELEMETRY_LED				"led_value"

/** @brief Telemetry key for device temperature monitoring (optional).
 * Float value in Celsius degrees from onboard temperature sensor.
 * Can be used for thermal management and environmental monitoring.
 * Typical range: -40°C to +85°C for industrial applications.
 */
#define TB_TELEMETRY_TEMPERATURE		"temperature"

/** @brief Telemetry key for device uptime tracking.
 * Integer value in seconds since last device reset/power-on.
 * Useful for reliability monitoring and maintenance scheduling.
 * Resets to 0 on device restart or power cycle.
 */
#define TB_TELEMETRY_UPTIME				"uptime_seconds"

// ===== BUFFER SIZES =====

/** @brief Maximum size for MQTT message payload buffer.
 * Allocated memory for incoming and outgoing MQTT messages.
 * Must accommodate largest expected JSON payload plus protocol overhead.
 * Typical RPC payload: ~100 bytes, telemetry batch: ~300 bytes.
 */
#define TB_MQTT_PAYLOAD_BUFFER_SIZE		512

/** @brief Maximum size for JSON response buffer.
 * Memory allocated for constructing JSON responses to RPC requests.
 * Must fit response payload including status codes and return values.
 * Typical response size: 50-150 bytes depending on data complexity.
 */
#define TB_JSON_RESPONSE_BUFFER_SIZE	256

/** @brief Maximum length for RPC request ID string.
 * ThingsBoard generates unique request IDs for RPC correlation.
 * ID format: UUID or timestamp-based string, typically 20-30 characters.
 * Used to match responses with original requests.
 */
#define TB_RPC_REQUEST_ID_MAX_LEN		32

// ===== TIMING CONFIGURATIONS =====

/** @brief Interval in milliseconds for sending telemetry updates.
 * How often device reports status to ThingsBoard (30 seconds default).
 * Balance between real-time updates and network/power consumption.
 * Range: 1000ms (1sec) to 300000ms (5min) depending on application.
 */
#define TB_TELEMETRY_SEND_INTERVAL		30000

/** @brief Timeout in milliseconds for MQTT connection attempts.
 * Maximum time to wait for broker connection establishment.
 * Prevents indefinite blocking during network connectivity issues.
 * Should be longer than typical network round-trip time.
 */
#define TB_MQTT_CONNECT_TIMEOUT			10000

/** @brief Delay in milliseconds after processing RPC before response.
 * Brief pause to ensure command execution completes before replying.
 * Allows GPIO changes and PWM updates to stabilize.
 * Prevents race conditions between command and response.
 */
#define TB_RPC_RESPONSE_DELAY			100

#endif /* __THINGSBOARD_CONFIG_H__ */
