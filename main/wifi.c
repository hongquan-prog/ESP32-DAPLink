/*
 * Copyright (c) 2023-2023, lihongquan
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-4-20     lihongquan   WiFi configuration via web page
 */
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "wifi.h"

static const char *TAG = "wifi";
static const char *NVS_NAMESPACE = "wifi_config";
static const char *KEY_WIFI_SSID = "WIFI_SSID";
static const char *KEY_WIFI_PASSWORD = "WIFI_PASSWORD";

bool wifi_config_exists(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    char ssid[32] = {0};
    size_t ssid_len = sizeof(ssid);

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_get_str(nvs_handle, KEY_WIFI_SSID, ssid, &ssid_len);
    nvs_close(nvs_handle);

    if (err == ESP_ERR_NVS_NOT_FOUND || ssid_len == 0 || strlen(ssid) == 0)
    {
        ESP_LOGI(TAG, "No WiFi SSID found in NVS");
        return false;
    }

    ESP_LOGI(TAG, "WiFi SSID found in NVS: %s", ssid);
    return true;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    wifi_event_ap_staconnected_t *connected_event = (wifi_event_ap_staconnected_t *)event_data;
    wifi_event_ap_stadisconnected_t *disconnected_event = (wifi_event_ap_stadisconnected_t *)event_data;

    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(connected_event->mac), connected_event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(disconnected_event->mac), disconnected_event->aid);
    }
}

void wifi_init_softap(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    uint8_t mac[6] = {0};
    static char ap_ssid[32] = {0};

    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(ap_ssid, sizeof(ap_ssid), "DAPLink-%02X%02X", mac[4], mac[5]);

    wifi_config_t wifi_config = {
        .ap = {
            .channel = 6,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                    .required = true,
            },
        },
    };

    memcpy(wifi_config.ap.ssid, ap_ssid, sizeof(ap_ssid));
    wifi_config.ap.ssid_len = strlen(ap_ssid);
    esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s", ap_ssid);
}

void wifi_connect_sta(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    char ssid[32] = {0};
    char password[64] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t password_len = sizeof(password);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_get_str(nvs_handle, KEY_WIFI_SSID, ssid, &ssid_len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return;
    }

    /* Password is optional: open networks do not require a password */
    err = nvs_get_str(nvs_handle, KEY_WIFI_PASSWORD, password, &password_len);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGI(TAG, "No password stored, connecting to open network");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to get password: %s", esp_err_to_name(err));
        }
        password[0] = '\0';
        password_len = 0;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

    wifi_config_t sta_config = {
        .sta = {
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = "",
        },
    };

    memcpy(sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    if (password_len > 0 && strlen(password) > 0)
    {
        memcpy(sta_config.sta.password, password, sizeof(sta_config.sta.password));
    }

    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
