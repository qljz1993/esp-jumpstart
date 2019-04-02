/* Unified Provisioning Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/* This file is mostly a boiler-plate code that applications can use without much change */

#include <stdio.h>
#include <string.h>
#include <esp_err.h>
#include <esp_log.h>

#include <esp_wifi.h>
#include <tcpip_adapter.h>

#include <wifi_provisioning/wifi_config.h>

#include "conn_mgr_prov_priv.h"

static const char *TAG = "conn_mgr_prov_handler";

extern wifi_prov_config_handlers_t wifi_prov_handlers;

/* Provide definition of wifi_prov_ctx_t */
struct wifi_prov_ctx {
    wifi_config_t wifi_cfg;
};

static wifi_config_t *get_config(wifi_prov_ctx_t **ctx)
{
    return (*ctx ? & (*ctx)->wifi_cfg : NULL);
}

static wifi_config_t *new_config(wifi_prov_ctx_t **ctx)
{
    free(*ctx);
    (*ctx) = (wifi_prov_ctx_t *) calloc(1, sizeof(wifi_prov_ctx_t));
    return get_config(ctx);
}

static void free_config(wifi_prov_ctx_t **ctx)
{
    free(*ctx);
    *ctx = NULL;
}

static esp_err_t get_status_handler(wifi_prov_config_get_data_t *resp_data, wifi_prov_ctx_t **ctx)
{
    /* Initialize to zero */
    memset(resp_data, 0, sizeof(wifi_prov_config_get_data_t));

    if (wifi_prov_get_wifi_state(&resp_data->wifi_state) != ESP_OK) {
        ESP_LOGW(TAG, "Prov app not running");
        return ESP_ERR_INVALID_STATE;
    }

    if (resp_data->wifi_state == WIFI_PROV_STA_CONNECTED) {
        ESP_LOGI(TAG, "Connected state");

        /* IP Addr assigned to STA */
        tcpip_adapter_ip_info_t ip_info;
        tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
        char *ip_addr = ip4addr_ntoa(&ip_info.ip);
        strcpy(resp_data->conn_info.ip_addr, ip_addr);

        /* AP information to which STA is connected */
        wifi_ap_record_t ap_info;
        esp_wifi_sta_get_ap_info(&ap_info);
        memcpy(resp_data->conn_info.bssid, (char *)ap_info.bssid, sizeof(ap_info.bssid));
        memcpy(resp_data->conn_info.ssid,  (char *)ap_info.ssid,  sizeof(ap_info.ssid));
        resp_data->conn_info.channel   = ap_info.primary;
        resp_data->conn_info.auth_mode = ap_info.authmode;
        wifi_prov_done();
    } else if (resp_data->wifi_state == WIFI_PROV_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected state");

        /* If disconnected, convey reason */
        wifi_prov_get_wifi_disconnect_reason(&resp_data->fail_reason);
    } else {
        ESP_LOGI(TAG, "Connecting state");
    }
    return ESP_OK;
}

static esp_err_t set_config_handler(const wifi_prov_config_set_data_t *req_data, wifi_prov_ctx_t **ctx)
{
    wifi_config_t *wifi_cfg = get_config(ctx);
    if (wifi_cfg) {
        free_config(ctx);
    }

    wifi_cfg = new_config(ctx);
    if (!wifi_cfg) {
        ESP_LOGE(TAG, "Unable to alloc wifi config");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "WiFi Credentials Received : \n\tssid %s \n\tpassword %s",
             req_data->ssid, req_data->password);
    memcpy((char *) wifi_cfg->sta.ssid, req_data->ssid,
           strnlen(req_data->ssid, sizeof(wifi_cfg->sta.ssid)));
    memcpy((char *) wifi_cfg->sta.password, req_data->password,
           strnlen(req_data->password, sizeof(wifi_cfg->sta.password)));
    return ESP_OK;
}

static esp_err_t apply_config_handler(wifi_prov_ctx_t **ctx)
{
    wifi_config_t *wifi_cfg = get_config(ctx);
    if (!wifi_cfg) {
        ESP_LOGE(TAG, "WiFi config not set");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = wifi_prov_configure_sta(wifi_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi Credentials Applied");
    } else {
        ESP_LOGE(TAG, "Failed to apply WiFi Credentials");
    }

    free_config(ctx);
    return ret;
}

wifi_prov_config_handlers_t wifi_prov_handlers = {
    .get_status_handler   = get_status_handler,
    .set_config_handler   = set_config_handler,
    .apply_config_handler = apply_config_handler,
    .ctx = NULL
};
