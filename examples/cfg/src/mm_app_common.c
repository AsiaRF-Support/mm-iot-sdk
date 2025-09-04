/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/mbedtls_config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#ifdef MBEDTLS_THREADING_ALT
#include "threading_alt.h"
#endif

#include "mmosal.h"
#include "mmwlan.h"
#include "mmipal.h"
#include "mmconfig.h"
#include "mm_app_loadconfig.h"
#include "mm_app_common.h"
#include "mmping.h"
#include "main.h"

#if defined(PLATFORM_HALOW_HAS_LED)
#include "arf_led.h"
#endif


/** Maximum number of DNS servers to attempt to retrieve from config store. */
#ifndef DNS_MAX_SERVERS
#define DNS_MAX_SERVERS                 2
#endif

/** Led structure for display. */
#if defined(PLATFORM_HALOW_HAS_LED)
led_config_t led_msg = {
    .port = PLATFORM_HALOW_LED_PORT,
    .pin  = PLATFORM_HALOW_LED_PIN,
    .active_level = GPIO_ACTIVE_LOW
};
#endif

/** Binary semaphore used to start user_main() once the link comes up. */
static struct mmosal_semb *link_established = NULL;

/** Ping session ID for gateway ping */
static uint16_t gateway_ping_session_id = 0;

/**
 * WLAN station status callback, invoked when WLAN STA state changes.
 *
 * @param sta_state  The new STA state.
 */
static void sta_status_callback(enum mmwlan_sta_state sta_state)
{
    switch (sta_state)
    {
    case MMWLAN_STA_DISABLED:
        printf("WLAN STA disabled\n");
        break;

    case MMWLAN_STA_CONNECTING:
        printf("WLAN STA connecting\n");
        break;

    case MMWLAN_STA_CONNECTED:
        printf("WLAN STA connected\n");
        break;
    }
#if defined(PLATFORM_HALOW_HAS_LED)
	sta_state == MMWLAN_STA_CONNECTED ? led_on(&led_msg) : led_off(&led_msg);
#endif
}


/**
 * Load DNS server IP addresses from mmconfig and update the mmipal configuration accordingly.
 *
 * @param config_key_format     Format string for the mmconfig keys. This must include a single
 *                              %u format specifier for the server number.
 *
 * @returns true if one or more DNS servers were successfully configured, else false.
 */
static bool set_dns_servers(const char *config_key_format)
{
    uint8_t ii;
    enum mmipal_status status;
    char addr_str[MMIPAL_IPADDR_STR_MAXLEN];
    char key[MMCONFIG_MAX_KEYLEN];
    bool dns_servers_set = false;

    /* Load DNS server addresses from mmconfig, if specified. This will override the servers
     * set by DHCP (if any).  */
    for (ii = 0; ii < DNS_MAX_SERVERS; ii++)
    {
        snprintf(key, sizeof(key), config_key_format, ii);
        if (((mmconfig_read_string(key, addr_str, sizeof(addr_str))) > 0) &&
            addr_str[0] != '\0')
        {
            (void)mmipal_set_dns_server(ii, addr_str);
        }

        status = mmipal_get_dns_server(ii, addr_str);
        if (status == MMIPAL_SUCCESS && addr_str[0] != '\0')
        {
            printf("DNS server %d: %s\n", ii, addr_str);
            dns_servers_set = true;
        }
    }

    return dns_servers_set;
}

/**
 * Link status callback
 *
 * @param link_status   Current link status info.
 */
static void link_status_callback(const struct mmipal_link_status *link_status)
{
    uint32_t time_ms = mmosal_get_time_ms();
    if (link_status->link_state == MMIPAL_LINK_UP)
    {
        size_t ii;
        bool dns_servers_set = false;

        /* First try to load the DNS configuration from config keys `ip.dns_serverX`. If no
         * matching keys found then fall back to `dns.serverX`. Note `dns.serverX` is for
         * legacy support and should not be used in new designs. */
        dns_servers_set = set_dns_servers("ip.dns_server%u");
        if (!dns_servers_set)
        {
            dns_servers_set = set_dns_servers("dns.server%u");
        }

        printf("Link is up. Time: %lu ms, ", time_ms);
        printf("IP: %s, ", link_status->ip_addr);
        printf("Netmask: %s, ", link_status->netmask);
        printf("Gateway: %s", link_status->gateway);
        for (ii = 0; ii < DNS_MAX_SERVERS; ii++)
        {
            enum mmipal_status status;
            char addr_str[MMIPAL_IPADDR_STR_MAXLEN];

            status = mmipal_get_dns_server(ii, addr_str);
            if (status == MMIPAL_SUCCESS && addr_str[0] != '\0')
            {
                printf(", DNS server %u: %s", ii, addr_str);
            }
        }
        printf("\n");

        /* Start ping to gateway to keep IP active */
        if (strlen(link_status->gateway) > 0) {
            struct mmping_args ping_args = MMPING_ARGS_DEFAULT;

            /* Configure ping parameters */
            strcpy(ping_args.ping_target, link_status->gateway);
            strcpy(ping_args.ping_src, link_status->ip_addr);
            ping_args.ping_interval_ms = 10000; /* 10 seconds */
            ping_args.ping_count = 0;  /* Continuous ping */
            ping_args.ping_size = MMPING_DEFAULT_DATA_SIZE;

            /* Start the ping session */
            gateway_ping_session_id = mmping_start(&ping_args);
            if (gateway_ping_session_id > 0) {
                //__DBG("Started mm_ping to %s (session ID: %u)" ENDL,
                //       link_status->gateway, gateway_ping_session_id);
            } else {
                __DBG("Failed to start gateway ping" ENDL);
            }
        }

        mmosal_semb_give(link_established);
    }
    else
    {
        printf("Link is down. Time: %lu ms\n", time_ms);

        /* Stop gateway ping when link goes down */
        if (gateway_ping_session_id > 0) {
            mmping_stop();
            //__DBG("Stopped mm_ping (session ID: %u)" ENDL, gateway_ping_session_id);
            gateway_ping_session_id = 0;
        }
    }
}

void app_print_version_info(void)
{
    enum mmwlan_status status;
    struct mmwlan_version version;
    struct mmwlan_bcf_metadata bcf_metadata;

    printf("-----------------------------------\n");

    status = mmwlan_get_bcf_metadata(&bcf_metadata);
    if (status == MMWLAN_SUCCESS)
    {
        printf("  BCF API version:         %u.%u.%u\n",
               bcf_metadata.version.major, bcf_metadata.version.minor, bcf_metadata.version.patch);
        if (bcf_metadata.build_version[0] != '\0')
        {
            printf("  BCF build version:       %s\n", bcf_metadata.build_version);
        }
        if (bcf_metadata.board_desc[0] != '\0')
        {
            printf("  BCF board description:   %s\n", bcf_metadata.board_desc);
        }
    }
    else
    {
        printf("  !! BCF metadata retrival failed !!\n");
    }

    status = mmwlan_get_version(&version);
    if (status != MMWLAN_SUCCESS)
    {
        printf("  !! Error occured whilst retrieving version info !!\n");
    }
    printf("  Morselib version:        %s\n", version.morselib_version);
    printf("  Morse firmware version:  %s\n", version.morse_fw_version);
    printf("  Morse chip ID:           0x%04lx\n", version.morse_chip_id);
    printf("-----------------------------------\n");

    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);
}

void app_wlan_init(void)
{
    /* Ensure we don't call twice */
    MMOSAL_ASSERT(link_established == NULL);
    link_established = mmosal_semb_create("link_established");

    /* Initialize mbedTLS threading (required if MBEDTLS_THREADING_ALT is defined) */
#ifdef MBEDTLS_THREADING_ALT
    mbedtls_platform_threading_init();
#endif

    /* Initialize MMWLAN interface */
    mmwlan_init();
    mmwlan_set_channel_list(load_channel_list());

    /* Boot the WLAN interface so that we can retrieve the firmware version. */
    struct mmwlan_boot_args boot_args = MMWLAN_BOOT_ARGS_INIT;
    (void)mmwlan_boot(&boot_args);
    app_print_version_info();


    /* Load IP stack settings from config store, or use defaults if no entry found in
     * config store. */
    struct mmipal_init_args mmipal_init_args = MMIPAL_INIT_ARGS_DEFAULT;
    load_mmipal_init_args(&mmipal_init_args);

    /* Initialize IP stack. */
    if (mmipal_init(&mmipal_init_args) != MMIPAL_SUCCESS)
    {
        printf("Error initializing network interface.\n");
        MMOSAL_ASSERT(false);
    }

    mmipal_set_link_status_callback(link_status_callback);
}

void app_wlan_start(void)
{
    enum mmwlan_status status;

    /* Load Wi-Fi settings from config store */
    struct mmwlan_sta_args sta_args = MMWLAN_STA_ARGS_INIT;
    load_mmwlan_sta_args(&sta_args);
    load_mmwlan_settings();

    printf("Attempting to connect to %s ", sta_args.ssid);
    if (sta_args.security_type == MMWLAN_SAE)
    {
        printf("with passphrase %s", sta_args.passphrase);
    }
    printf("\n");
    printf("This may take some time (~30 seconds)\n");

    status = mmwlan_sta_enable(&sta_args, sta_status_callback);
    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);

    /* Wait for link status callback.
    * Use a binary semaphore to block us until Link is up.
    */
    mmosal_semb_wait(link_established, UINT32_MAX);

    /* Wi-Fi link is now established, return to caller */
}

void app_wlan_stop(void)
{
    /* Shutdown wlan interface */
    mmwlan_shutdown();
}
