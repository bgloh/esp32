/**-----------------------------------------------------------------------------

    Example code using ESP32 with SPH0645 microphone and UDA1334 audio output

    @file is modified from esp32.ino

    @brief Create a developer account at https://developers.chirp.io,
    and copy and paste your key, secret and config string for the "arduino"
    protocol into the credentials.h file.

    This example will start listening for chirps and print to the terminal
    when anything is received.

    If the EN switch is pressed on the board, a random chirp will be sent to
    the audio output.


    Copyright © 2011-2019, Asio Ltd.
    All rights reserved.

  ----------------------------------------------------------------------------*/
#include <driver/i2s.h>

#include "chirp_connect.h"
//#include "credentials.h"

#define I2SI_DATA         12     // I2S DATA IN on GPIO32
#define I2SI_BCK          14     // I2S BCLK on GPIO14
#define I2SI_LRCL         15     // I2S SELECT on GPIO15

#define I2SO_DATA         25     // I2S DATA OUT on GPIO23
#define I2SO_BCK          27     // I2S BCLK on GPIO18
#define I2SO_WSEL         26      // I2S SELECT on GPIO5

#define LED_PIN           2      // LED
#define SWITCH_PIN        0      // Switch

#define BUFFER_SIZE       512
#define MIC_CALIBRATION   13125
#define SAMPLE_RATE       16000

// 16kHz mono
#define APP_KEY     "BE9CcAc0F91dD3E5F546f7254"
#define APP_SECRET  "2bCC2406b7A506bd55c4bfCEFA475d1ADa41fBdFcCfB6BaccB"
#define APP_CONFIG  "Hx2Lrc+uTV/d26sRHUyRUb1QpwDpgtBSarQWksCKz41sMCI3UXJLuiOwojj13ISafsu+CF4uak1DcFiaJGK/OBn/5l/6X2CY8F7aK0t5tnyfBbbVdu2dS0I+E5Nrq81uc/Ls0yGJFHROJGOjAEusp24SsZWO61pHuPaxf5cTIdwDohi5QMOPl/A6eqG9hXzaHdMDjKZK5f6Dvq6RUR1gFfSO55gn0JWeO9+Q2hpAMVEPvrkfPWoKCTM3B5w+dL1Cli1UtO7aVCehXHen1XjuNbMV7v2OfKbLOiva6+MJNpiUIbMulOQQvTsRoGD9qiwyMzlFHkHVOsgbsj8KU974Nx7ApcHyPjHhmANVBx+KZ6cz39WuTtLdnrixyRBAjjKdj7AiaZy2YizPjAgatcwlEfxVBER4cUASs+aeMiLODpg5hWSv7s5ExXTIYnhxtFVy5MNFCV4JrNSJZTNqpkQPftKykAsecPNsvYkq2BF9XLkVvode6KWdGa6VY1fQ5WBzbNxWsZ9uer6AB2/0VuyZNL5y4pe0/T7YRPwQwntYDKLqnsYqkj+P+CNjegUgXVOq/CH2SSesfClFdWgAASARRodGj8fvIL/fPMSNJjkNErnZidI4AdO7Trn32hH8gIRjEd2YhXFYr4C1fszrNiMX12qnrsdCGeGe6AQYZGK2TLOX2DHVmzaWyLFYKNEb9QatqmeUK85tilaQdcDHray24sgtqZhr0hVRKIwSTg3gLIR/JOl+uwJ5g0GBRKTkCIStOSbBRXvQwVsIuSWKhvTkYeel1ZS6uj+hqKD4MvZXEvDuKJXvnXYO792aA4jJgYTwBOziL9Lrhd5WhdVhQOlLjIYQ3z6zZbozfK4KRL17jtU="


/**
   Convert I2S input data.
   Data is 18 bit signed, MSBit first, two's complement.
   The calibration value is determined using the Serial
   Plotter to centre the audio about zero.
*/
#define CONVERT_INPUT(sample) (((int32_t)(sample) >> 14) + MIC_CALIBRATION)

// Global variables -------------------------------------------
static chirp_connect_t *connect = NULL;
static chirp_connect_state_t currentState = CHIRP_CONNECT_STATE_NOT_CREATED;
static volatile bool buttonPressed = false;
static bool startTasks = false;

// Function declarations --------------------------------------
void IRAM_ATTR handleInterrupt();
void setupChirp();
void chirpErrorHandler(chirp_connect_error_code_t code);
void setupAudioInput(int sample_rate);
void setupAudioOutput(int sample_rate);

// Function definitions ---------------------------------------
void
onStateChangedCallback(void *connect, chirp_connect_state_t previous, chirp_connect_state_t current)
{
  currentState = current;
  Serial.printf("State changed from %d to %d\n", previous, current);
}

void
onReceivingCallback(void *connect, uint8_t *payload, size_t length, uint8_t channel)
{
  Serial.println("Receiving data...");
  digitalWrite(LED_PIN, HIGH);
}

void
onReceivedCallback(void *connect, uint8_t *payload, size_t length, uint8_t channel)
{
  if (payload) {
    char *data = chirp_connect_as_string((chirp_connect_t *)connect, payload, length);
    Serial.printf("data = %s\n", data);
    digitalWrite(LED_PIN, LOW);
    chirp_connect_free(data);
  } else {
    Serial.println("Decode failed.");
  }
}

// Tasks -------------------------------------------------------

void
initTask(void *parameter)
{
  setupChirp();

  uint32_t output_sample_rate = chirp_connect_set_output_sample_rate(connect, SAMPLE_RATE);
  setupAudioOutput(SAMPLE_RATE);

  uint32_t input_sample_rate = chirp_connect_set_input_sample_rate(connect, SAMPLE_RATE);
  setupAudioInput(SAMPLE_RATE);

  Serial.printf("Heap size: %u\n", ESP.getFreeHeap());
  startTasks = true;
  vTaskDelete(NULL);
}

void
processInputTask(void *parameter)
{
  esp_err_t audioError;
  chirp_connect_error_code_t chirpError;

  size_t bytesLength = 0;
  float buffer[BUFFER_SIZE] = {0};
  int32_t ibuffer[BUFFER_SIZE] = {0};

  while (currentState >= CHIRP_CONNECT_STATE_RUNNING) {
    audioError = i2s_read(I2S_NUM_0, ibuffer, BUFFER_SIZE * 4, &bytesLength, portMAX_DELAY);
    if (bytesLength) {
      for (int i = 0; i < bytesLength / 4; i++) {
        buffer[i] = (float)CONVERT_INPUT(ibuffer[i]);
      }

      uint32_t t1 = millis();
      chirpError = chirp_connect_process_input(connect, buffer, bytesLength / 4);
      if (chirpError != CHIRP_CONNECT_OK) {
        chirpErrorHandler(chirpError);
      }
    }
  }
  vTaskDelete(NULL);
}

void
processOutputTask(void *parameter)
{
  esp_err_t audioError;
  chirp_connect_error_code_t chirpError;

  size_t bytesLength = 0;
  short buffer[BUFFER_SIZE] = {0};
  int32_t ibuffer[BUFFER_SIZE] = {0};

  while (currentState >= CHIRP_CONNECT_STATE_RUNNING) {
    chirpError = chirp_connect_process_shorts_output(connect, buffer, BUFFER_SIZE);
    if (chirpError != CHIRP_CONNECT_OK) {
      chirpErrorHandler(chirpError);
    }
    for (int i = 0; i < BUFFER_SIZE; i++) {
      ibuffer[i] = (int32_t)buffer[i];
    }
    audioError = i2s_write(I2S_NUM_1, ibuffer, BUFFER_SIZE * 4, &bytesLength, portMAX_DELAY);
  }
  vTaskDelete(NULL);
}

// Main -------------------------------------------------------

void
setup()
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), handleInterrupt, FALLING);

  Serial.begin(115200);
  Serial.printf("Heap size: %u\n", ESP.getFreeHeap());

  xTaskCreate(initTask, "initTask", 16384, NULL, 1, NULL);


}

void
loop()
{
  esp_err_t audioError;
  chirp_connect_error_code_t chirpError;

  if (startTasks) {
 //   xTaskCreate(processInputTask, "processInputTask", 16384, NULL, 5, NULL);
    xTaskCreate(processOutputTask, "processOutputTask", 16384, NULL, 3, NULL);
    startTasks = false;
  }

  if (buttonPressed) {
  //  size_t payloadLength = 0;
  //  uint8_t *payload = chirp_connect_random_payload(connect, &payloadLength);
    char identifier[32] = {0,2,4,0x09};  
    uint8_t *payload = (uint8_t *)identifier; 
    size_t payloadLength = sizeof(identifier) / sizeof(identifier[0]); 
    chirp_connect_error_code_t rc = chirp_connect_send(connect, payload, payloadLength);
    //chirp_connect_send(connect, payload, payloadLength);
    Serial.println("Sending data:");
    
    if (rc != CHIRP_CONNECT_OK) {
      const char *errstr = chirp_connect_error_code_to_string(rc);
      Serial.println(errstr);
      } 


    
    buttonPressed = false;
  }
}

void
IRAM_ATTR handleInterrupt()
{
  buttonPressed = true;
}

// Chirp -------------------------------------------------------

void
setupChirp()
{
  connect = new_chirp_connect(APP_KEY, APP_SECRET);
  if (connect == NULL) {
    Serial.println("Chirp initialisation failed.");
    return;
  }

  chirp_connect_error_code_t err = chirp_connect_set_config(connect, APP_CONFIG);
  if (err != CHIRP_CONNECT_OK)
    chirpErrorHandler(err);

  chirp_connect_callback_set_t callbacks = {0};
  callbacks.on_sending = NULL;
  callbacks.on_sent = NULL;
  callbacks.on_state_changed = onStateChangedCallback;
  callbacks.on_receiving = onReceivingCallback;
  callbacks.on_received = onReceivedCallback;

  err = chirp_connect_set_callbacks(connect, callbacks);
  if (err != CHIRP_CONNECT_OK)
    chirpErrorHandler(err);

  err = chirp_connect_set_callback_ptr(connect, connect);
  if (err != CHIRP_CONNECT_OK)
    chirpErrorHandler(err);

  err = chirp_connect_start(connect);
  if (err != CHIRP_CONNECT_OK)
    chirpErrorHandler(err);

  // Set volume to 0.5 to not distort output
  chirp_connect_set_volume(connect, 0.25);

  Serial.println("Chirp Connect initialised.");

  // Chirp application info
  char *info = chirp_connect_get_info(connect);
  Serial.println(info);
  free(info);
}

void
chirpErrorHandler(chirp_connect_error_code_t code)
{
  if (code != CHIRP_CONNECT_OK) {
    const char *error_string = chirp_connect_error_code_to_string(code);
    Serial.printf("Chirp error handler : %s\n", error_string);
    chirp_connect_free((void *) error_string);
    while (true) {
      delay(1000);
      Serial.print('.');
    }
  }
}

// Audio -------------------------------------------------------

void
setupAudioInput(int sample_rate)
{
  /*
     Set up I2S audio for SPH0645 microphone
  */
  esp_err_t err;
  Serial.println("Initialising audio input driver..");

  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = sample_rate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = true
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2SI_BCK,
    .ws_io_num = I2SI_LRCL,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2SI_DATA
  };

  err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing driver: %d\n", err);
    while (true);
  }

  err = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting pin: %d\n", err);
    while (true);
  }

  err = i2s_set_sample_rates(I2S_NUM_0, sample_rate);
  if (err != ESP_OK) {
    Serial.printf("Failed to set sample rates: %d\n", err);
    while (true);
  }

  Serial.println("Audio input driver initalised.");
}


void
setupAudioOutput(int sample_rate)
{
  /*
     Set up I2S audio for UDA1334 DAC output
  */
  esp_err_t err;
  Serial.println("Initialising audio output driver..");

  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sample_rate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = true
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2SO_BCK,
    .ws_io_num = I2SO_WSEL,
    .data_out_num = I2SO_DATA,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  err = i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing driver: %d\n", err);
    while (true);
  }

  err = i2s_set_pin(I2S_NUM_1, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting pin: %d\n", err);
    while (true);
  }

  err = i2s_set_sample_rates(I2S_NUM_1, sample_rate);
  if (err != ESP_OK) {
    Serial.printf("Failed to set sample rates: %d\n", err);
    while (true);
  }

  Serial.println("Audio output driver initalised.");
}
