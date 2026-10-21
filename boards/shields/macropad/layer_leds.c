#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#define LOCK_LAYER 4

static const struct gpio_dt_spec leds[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(led4), gpios),
};

static struct k_work_delayable bt_blink_work;
static bool is_connected = false;
static bool bt_led_state = false;

static void update_layer_led(void) {
    uint8_t layer = zmk_keymap_highest_layer_active();

    // 1~4번 레이어 LED 제어
    for (int i = 0; i < 4; i++) {
        if (device_is_ready(leds[i].port)) {
            if (layer == LOCK_LAYER) {
                // 키 7번 잠금 상태: 모든 레이어 LED 소등
                gpio_pin_set_dt(&leds[i], 0);
            } else {
                // 현재 활성화된 레이어 번호의 LED만 점등
                gpio_pin_set_dt(&leds[i], (i == layer) ? 1 : 0);
            }
        }
    }

    // 잠금 상태일 때는 5번 LED(블루투스 표시)도 강제 소등
    if (layer == LOCK_LAYER && device_is_ready(leds[4].port)) {
        gpio_pin_set_dt(&leds[4], 0);
    }
}

// 블루투스 페어링 대기(미연결) 시 1초 간격 점멸 처리
static void bt_blink_handler(struct k_work *work) {
    if (is_connected) {
        if (device_is_ready(leds[4].port)) {
            gpio_pin_set_dt(&leds[4], 0);
        }
        bt_led_state = false;
        return;
    }

    uint8_t layer = zmk_keymap_highest_layer_active();
    if (layer == LOCK_LAYER) {
        if (device_is_ready(leds[4].port)) {
            gpio_pin_set_dt(&leds[4], 0);
        }
    } else {
        bt_led_state = !bt_led_state;
        if (device_is_ready(leds[4].port)) {
            gpio_pin_set_dt(&leds[4], bt_led_state ? 1 : 0);
        }
    }

    k_work_schedule(&bt_blink_work, K_MSEC(1000));
}

static void on_connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        return;
    }
    is_connected = true;
    k_work_cancel_delayable(&bt_blink_work);
    bt_led_state = false;
    if (device_is_ready(leds[4].port)) {
        gpio_pin_set_dt(&leds[4], 0);
    }
    update_layer_led();
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason) {
    is_connected = false;
    bt_led_state = false;
    k_work_schedule(&bt_blink_work, K_NO_WAIT);
    update_layer_led();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};

static int layer_leds_init(void) {
    for (int i = 0; i < 5; i++) {
        if (device_is_ready(leds[i].port)) {
            gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
        }
    }

    k_work_init_delayable(&bt_blink_work, bt_blink_handler);
    k_work_schedule(&bt_blink_work, K_NO_WAIT);
    update_layer_led();
    return 0;
}

static int layer_event_listener(const zmk_event_t *eh) {
    update_layer_led();
    return 0;
}

ZMK_LISTENER(layer_leds, layer_event_listener);
ZMK_SUBSCRIPTION(layer_leds, zmk_layer_state_changed);

SYS_INIT(layer_leds_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
