#include <mechatronics_project_3_inferencing.h>
#define EIDSP_QUANTIZE_FILTERBANK   0

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include <math.h>
#include <ESP32Servo.h>

/* I2S MIC */
#define I2S_PORT   I2S_NUM_0
#define I2S_SCK    40
#define I2S_WS     42
#define I2S_SD     41

/* LED NOISE - chân 9 */
#define LED_NOISE  9

/* SERVO - chân 6 */
#define SERVO_PIN  6

/* AUDIO */
#define SAMPLE_RATE     16000
#define BUFFER_SIZE     2048
#define MIN_CONFIDENCE  0.60   // TĂNG LÊN 0.60 để loại bỏ nhiễu

// DEBOUNCE
#define COMMAND_COOLDOWN  2000   // 2 giây giữa các lệnh (ms)
#define SAME_COMMAND_DELAY 3000  // 3 giây mới được lặp lại cùng lệnh

typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

static inference_t inference;
static signed short sampleBuffer[BUFFER_SIZE];
static bool record_status = true;

/* STATE */
static bool servo_running = false;
static int servo_direction = 0;

/* DEBOUNCE STATE */
static unsigned long last_command_time = 0;
static char last_command[10] = "";

/* FILTER */
static float x_prev = 0;
static float y_prev = 0;
static float noise_floor = 50.0;
static const float a0 = 0.9;

/* SERVO OBJECT */
Servo myservo;

/* ================= LED ================= */
void updateLED() {
    digitalWrite(LED_NOISE, !servo_running);
}

/* ================= SERVO CONTROL ================= */
void stopServo() {
    if(servo_running) {
        myservo.attach(SERVO_PIN);
        delay(15);
        myservo.write(90);
        delay(300);
        myservo.detach();
        servo_running = false;
        Serial.println(">>> SERVO DỪNG HẲN (góc 90°) <<<");
        updateLED();
    }
}

void startServo(int direction) {
    if(servo_running && servo_direction == direction) {
        return;
    }
    
    myservo.attach(SERVO_PIN);
    delay(15);
    myservo.write(direction);
    servo_running = true;
    servo_direction = direction;
    
    if(direction == 0) {
        Serial.println(">>> SERVO QUAY THUẬN (ON - góc 0°) <<<");
    } else if(direction == 180) {
        Serial.println(">>> SERVO QUAY NGƯỢC (ONE - góc 180°) <<<");
    }
    
    updateLED();
}

/* ================= COMMAND WITH DEBOUNCE ================= */
void executeCommand(const char* cmd, float conf) {
    unsigned long now = millis();
    
    // Kiểm tra cooldown chung
    if(now - last_command_time < COMMAND_COOLDOWN) {
        // Chỉ in debug nếu không phải noise
        if(strcmp(cmd, "noise") != 0) {
            Serial.printf("IGNORED %s (%.2f) - cooldown %dms\n", cmd, conf, COMMAND_COOLDOWN);
        }
        return;
    }
    
    // Kiểm tra lệnh lặp lại quá nhanh
    if(strcmp(cmd, last_command) == 0 && (now - last_command_time) < SAME_COMMAND_DELAY) {
        if(strcmp(cmd, "noise") != 0) {
            Serial.printf("IGNORED %s (%.2f) - same command within %dms\n", cmd, conf, SAME_COMMAND_DELAY);
        }
        return;
    }
    
    // Chấp nhận lệnh
    Serial.printf(">>> EXECUTE: %s (%.2f) <<<\n", cmd, conf);
    
    if(strcmp(cmd, "on") == 0) {
        startServo(0);
        strcpy(last_command, "on");
        last_command_time = now;
    }
    else if(strcmp(cmd, "one") == 0) {
        startServo(180);
        strcpy(last_command, "one");
        last_command_time = now;
    }
    else if(strcmp(cmd, "off") == 0) {
        stopServo();
        strcpy(last_command, "off");
        last_command_time = now;
    }
    else if(strcmp(cmd, "noise") == 0) {
        // Noise: không reset timer, chỉ in log nếu cần debug
        // Serial.print(".");
    }
}

/* ================= AUDIO PROCESS ================= */
void processAudio(int16_t* samples, size_t len) {
    float energy = 0;

    for(size_t i = 0; i < len; i++) {
        float x = samples[i];
        float y = a0 * (y_prev + x - x_prev);
        y_prev = y;
        x_prev = x;

        float gain;
        if(noise_floor < 50) gain = 10.0;
        else if(noise_floor < 200) gain = 6.0;
        else gain = 3.0;

        y *= gain;

        if(y > 30000) y = 30000;
        if(y < -30000) y = -30000;

        samples[i] = (int16_t)y;
        energy += fabs(y);
    }

    energy /= len;
    noise_floor = noise_floor * 0.9 + energy * 0.1;
}

/* ================= I2S ================= */
static int i2s_init(uint32_t rate) {
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .dma_buf_count = 6,
        .dma_buf_len = 512
    };

    i2s_pin_config_t pin = {
        .bck_io_num = I2S_SCK,
        .ws_io_num  = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_SD
    };

    if(i2s_driver_install(I2S_PORT, &cfg, 0, NULL) != ESP_OK) return -1;
    if(i2s_set_pin(I2S_PORT, &pin) != ESP_OK) return -1;

    i2s_set_clk(I2S_PORT, rate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
    return 0;
}

/* ================= CALLBACK ================= */
static void audio_callback(uint32_t n_bytes) {
    for(int i = 0; i < n_bytes >> 1; i++) {
        inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

        if(inference.buf_count >= inference.n_samples) {
            inference.buf_select ^= 1;
            inference.buf_count = 0;
            inference.buf_ready = 1;
        }
    }
}

/* ================= CAPTURE ================= */
static void capture_task(void *arg) {
    size_t bytes_read;

    while(record_status) {
        if(i2s_read(I2S_PORT, sampleBuffer, BUFFER_SIZE, &bytes_read, portMAX_DELAY) == ESP_OK) {
            processAudio(sampleBuffer, bytes_read / 2);
            audio_callback(bytes_read);
        }
    }
}

/* ================= MIC ================= */
static bool mic_start(uint32_t n_samples) {
    inference.buffers[0] = (int16_t*)malloc(n_samples * sizeof(int16_t));
    inference.buffers[1] = (int16_t*)malloc(n_samples * sizeof(int16_t));

    if(!inference.buffers[0] || !inference.buffers[1]) return false;

    inference.buf_count = 0;
    inference.buf_ready = 0;
    inference.n_samples = n_samples;

    if(i2s_init(SAMPLE_RATE) != 0) return false;

    xTaskCreatePinnedToCore(capture_task, "cap", 4096, NULL, 10, NULL, 0);
    return true;
}

static bool mic_record(void) {
    int timeout = 0;
    while(!inference.buf_ready && timeout < 200) {
        vTaskDelay(1);
        timeout++;
    }
    inference.buf_ready = 0;
    return timeout < 200;
}

static int get_audio(size_t offset, size_t length, float *out) {
    for(size_t i = 0; i < length; i++)
        out[i] = inference.buffers[inference.buf_select ^ 1][offset + i];
    return 0;
}

/* ================= SETUP ================= */
void setup() {
    Serial.begin(115200);
    delay(200);

    pinMode(LED_NOISE, OUTPUT);
    
    myservo.attach(SERVO_PIN);
    myservo.write(90);
    delay(300);
    myservo.detach();
    servo_running = false;
    
    updateLED();

    run_classifier_init();

    if(!mic_start(EI_CLASSIFIER_SLICE_SIZE)) {
        Serial.println("Lỗi khởi tạo MIC!");
        while(1);
    }

    Serial.println("\n========== READY ==========");
    Serial.println("Lệnh: on | off | one");
    Serial.println("Confidence threshold: 0.60");
    Serial.println("Cooldown: 2s giữa các lệnh");
    Serial.println("Same command delay: 3s");
    Serial.println("===========================\n");
}

/* ================= LOOP ================= */
void loop() {
    if(!mic_record()) return;

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &get_audio;

    ei_impulse_result_t result = {0};

    if(run_classifier_continuous(&signal, &result, false) != EI_IMPULSE_OK)
        return;

    float best = 0;
    int idx = 0;

    for(int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if(result.classification[i].value > best) {
            best = result.classification[i].value;
            idx = i;
        }
    }

    const char* label = result.classification[idx].label;

    // Tăng threshold lên 0.60 để tránh false positive
    if(best > MIN_CONFIDENCE) {
        executeCommand(label, best);
    }
}