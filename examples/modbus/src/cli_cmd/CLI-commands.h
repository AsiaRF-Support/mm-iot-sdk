#ifndef RELEASE_VERSION
#error "RELEASE_VERSION not defined"
#else
#define RELEASE_BUILD_DATE __DATE__ " " __TIME__
#endif


/** Define mmconfig key */
#define MMCONF_WLAN_SSID			"wlan.ssid"
#define MMCONF_WLAN_PASSWORD		"wlan.password"
#define MMCONF_WLAN_SECURITY		"wlan.security"
#define MMCONF_WLAN_COUNTRY			"wlan.country_code"
#define MMCONF_WLAN_POWER_SAVE		"wlan.power_save_mode"
#define MMCONF_IP_DHCP				"ip.dhcp_enabled"
#define MMCONF_IP_IPADDR			"ip.ip_addr"
#define MMCONF_IP_NETMASK			"ip.netmask"
#define MMCONF_IP_GATEWAY			"ip.gateway"
#define MMCONF_SERIAL_BAUDRATE		"serial.baudrate"
#define MMCONF_SERIAL_DATA_BIT		"serial.data_bit"
#define MMCONF_SERIAL_PARITY_BIT	"serial.parity_bit"
#define MMCONF_SERIAL_STOP_BIT		"serial.stop_bit"
#define MMCONF_SERIAL_MODBUS_PORT	"serial.modbus_port"
#define MMCONF_NULL_KEY				""

/** Default value for the configuration parameters. */
#define DEFAULT_SSID				"MM_AsiaRF-IoT"
#define DEFAULT_PASSWORD			"12345678"
#define DEFAULT_SECURITY			"sae"
#define DEFAULT_COUNTRY				"US"
#define DEFAULT_DHCP				false
#define DEFAULT_IPADDR				"192.168.3.2"
#define DEFAULT_NETMASK				"255.255.255.0"
#define DEFAULT_GATEWAY				"192.168.3.3"
#define DEFAULT_POWERSAVE			POWERSAVE_DISABLE_STR
#define DEFAULT_BAUDRATE			"9600"
#define DEFAULT_DATA_BIT			"8"
#define DEFAULT_PARITY_BIT			"none"
#define DEFAULT_STOP_BIT			"1"
#define DEFAULT_MODBUS_PORT			"502"

#define BUFFER_SIZE_LONG 			64
#define BUFFER_SIZE 				32
#define BUFFER_SIZE_SHORT 			16
#define MAX_CMD_LEN					BUFFER_SIZE

#define MAX_SSID_LEN				MMWLAN_SSID_MAXLEN
#define MAX_PASSWORD_LEN			64
#define MIN_PASSWORD_LEN			8
#define MAX_ADDR_STR_LEN			16

#define POWERSAVE_ENABLE_STR		"enabled"
#define POWERSAVE_DISABLE_STR		"disabled"

#define ENABLE_CHANGE_COUNTRY	0

typedef enum {
	CLI_SET_SSID,
	CLI_SET_PASSWORD,
	CLI_SET_SECURITY,
#if ENABLE_CHANGE_COUNTRY
	CLI_SET_COUNTRY,
#endif
	CLI_SET_DHCP,
	CLI_SET_IP,
	CLI_SET_NETMASK,
	CLI_SET_GATEWAY,
	CLI_SET_POWERSAVE,
#if MODBUS_NUM != 0
	CLI_SET_BAUDRATE,
	CLI_SET_DATA_BIT,
	CLI_SET_PARITY_BIT,
	CLI_SET_STOP_BIT,
	CLI_SET_MODBUS_PORT,
#endif
	CLI_GET_RSSI,
	CLI_SCAN,

	CLI_CMD_COUNT
} CLI_CMD_E;

typedef enum {
	PARAM_COMMAND,
	PARAM_ARG_1,
	PARAM_ARG_2,

	PARAM_MAX_COUNT
} PARAM_INDEX_E;

typedef struct {
	const char *cmd_str;
	const char *conf_key;
	CLI_CMD_E   cmd_enum;
	const int param_counts;
	const char *help_str;
} CLI_CMD_MAP_t;

/* 	key_word		mmconfig key				CMD_ENUM			PARAM_COUNTS		help string */
static const CLI_CMD_MAP_t CLI_CMD_MAP[CLI_CMD_COUNT] = {
	{ "ssid",		MMCONF_WLAN_SSID,			CLI_SET_SSID,		2,		" set ssid <ssid>\t : Set Wi-Fi HaLow SSID\r\n" },
	{ "password",	MMCONF_WLAN_PASSWORD,		CLI_SET_PASSWORD,	2,		" set password <password> : Set Wi-Fi HaLow password\r\n" },
	{ "security",	MMCONF_WLAN_SECURITY,		CLI_SET_SECURITY,	2,		" set security <mode>\t : Set security mode (sae, owe, open)\r\n" },
#if ENABLE_CHANGE_COUNTRY
	{ "country",	MMCONF_WLAN_COUNTRY,		CLI_SET_COUNTRY,	2,		" set country <country>\t : Set country code\r\n" },
#endif
	{ "dhcp",		MMCONF_IP_DHCP,				CLI_SET_DHCP,		2,		" set dhcp <0|1>\t\t : Enable/Disable DHCP (0: disable, 1: enable)\r\n" },
	{ "ip",			MMCONF_IP_IPADDR,			CLI_SET_IP,			2,		" set ip <ip>\t\t : Set static IP address (format: xxx.xxx.xxx.xxx)\r\n" },
	{ "netmask",	MMCONF_IP_NETMASK,			CLI_SET_NETMASK,	2,		" set netmask <netmask>\t : Set network mask (format: xxx.xxx.xxx.xxx)\r\n" },
	{ "gateway",	MMCONF_IP_GATEWAY,			CLI_SET_GATEWAY,	2,		" set gateway <gateway>\t : Set gateway address (format: xxx.xxx.xxx.xxx)\r\n" },
	{ "powersave",	MMCONF_WLAN_POWER_SAVE,		CLI_SET_POWERSAVE, 	2,		" set powersave <0|1>\t : Enable/Disable power save mode (0: disable, 1: enable)\r\n" },
#if MODBUS_NUM != 0
	{ "baudrate",	MMCONF_SERIAL_BAUDRATE,		CLI_SET_BAUDRATE,	3,		" set baudrate <device> <baudrate>\t : Set baudrate of the specific device (9600 - 115200)\r\n" },
	{ "data_bit",	MMCONF_SERIAL_DATA_BIT,		CLI_SET_DATA_BIT,	3,		" set data_bit <device> <data_bit>\t : Set data bit length of the specific device (7 - 9)\r\n" },
	{ "parity_bit",	MMCONF_SERIAL_PARITY_BIT,	CLI_SET_PARITY_BIT,	3,		" set parity_bit <device> <parity_bit>\t : Set parity bit of the specific device (none, even, odd)\r\n" },
	{ "stop_bit",	MMCONF_SERIAL_STOP_BIT,		CLI_SET_STOP_BIT,	3,		" set stop_bit <device> <stop_bit>\t : Set stop bit of the specific device (1, 1.5, 2)\r\n" },
	{ "modbus_port",MMCONF_SERIAL_MODBUS_PORT,	CLI_SET_MODBUS_PORT,3,		" set modbus_port <device> <port>\t : Set modbus port of the specific device (0 - 65535)\r\n" },
#endif
	{ "rssi",		MMCONF_NULL_KEY,			CLI_GET_RSSI,		1,		" get rssi\t\t : Get RSSI value\r\n" },
	{ "scan",		MMCONF_NULL_KEY,			CLI_SCAN,			0,		" scan\t\t : Scan HaLow AP\r\n" }
};

/* Morse config read write function. */
const char *get_conf_key_of_dev(CLI_CMD_E cmd, const char* dev);
int config_write_str_of_dev(CLI_CMD_E cmd, const char *value, size_t len, const char* dev);
int config_read_str_of_dev(CLI_CMD_E cmd, char *buffer, int bufsize, const char* dev);
int config_write_str(CLI_CMD_E cmd, const char *value, size_t len);
int config_read_str(CLI_CMD_E cmd, char *buffer, int bufsize);
int config_write_bool(CLI_CMD_E cmd, bool value);
int config_read_bool(CLI_CMD_E cmd, bool *value);

/* Check data function. */
int isValidSSID(const char *ssid, size_t len);
int isValidPassword(const char *password, size_t len);
int isValidSecurity(const char *security, size_t len);
int isValidCountry(const char *security, size_t len);
int isValidBool(const char *dhcp, size_t len);
int isValidADDR(const char *addr, size_t len);
int isValidDevice(const char *device, size_t len);
int isValidBaudrate(const char *baudrate, size_t len);
int isValidParityBit(const char *parity_bit, size_t len);
int isValidDataBit(const char *data_bit, size_t len);
int isValidStopBit(const char *stop_bit, size_t len);
int isValidModbusPort(const char *modbus_port, size_t len);
