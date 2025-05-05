#include <driver/i2s.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiManager.h> 
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <vector>

// WiFi configuration
// Instead of hardcoded SSID/password, we'll use WiFiManager
const char* serverUrl = "http://192.168.199.46:5055/uploads"; // Change to your server IP/hostname
const char* AP_NAME = "SMARTNOTE_AP"; // Access point name for configuration portal
const char* AP_PASSWORD = "abc12345"; // Password for the configuration portal (can be empty for open network)
#define WIFI_CONFIG_TIMEOUT 180 // Timeout for config portal in seconds

// Button pins
#define BUTTON_UP 25      // Pin untuk tombol atas
#define BUTTON_DOWN 27    // Pin untuk tombol bawah
#define BUTTON_OK 26      // Pin untuk tombol OK

// OLED Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C // Typical I2C address for SSD1306 OLED
// Initialize ThingPulse SSD1306 display
SSD1306Wire display(0x3c, 5, 4);

// I2S configurations
#define I2S_WS 22       // Word Select pin (also called LRC or LRCLK)
#define I2S_SD 32       // Serial Data pin (also called DOUT or DATA)
#define I2S_SCK 14      // Serial Clock pin (also called BCLK)
#define I2S_PORT I2S_NUM_0

// SD Card pins for SPI
#define SD_CS 15         // SD Card chip select
#define SD_SCK 18       // SD Card SPI clock
#define SD_MISO 19      // SD Card SPI MISO
#define SD_MOSI 23      // SD Card SPI MOSI

// Recording parameters
#define SAMPLE_RATE 16000           // Audio sample rate in Hz
#define SAMPLE_BITS 16              // Sample bits
#define CHANNELS 1                  // Number of channels (1 for mono)
#define BUFFER_SIZE 512             // DMA buffer size

// Test parameters
#define MIC_TEST_DURATION_MS 2000   // Duration for the microphone test in milliseconds
#define MIC_AMPLITUDE_THRESHOLD 500 // Minimum amplitude to consider microphone working

// Menu states
enum MenuState {
  MENU_MAIN,
  MENU_RECORDING,
  MENU_UPLOADING,
  MENU_TESTING,
  MENU_WIFI_SETTINGS,  // Sub-menu untuk WiFi settings
  MENU_FILE_SELECT,
  MENU_FILE_ACTION,
};

// Global variables
bool isRecording = false;
File audioFile;
unsigned long recordingStartTime = 0;
int fileCounter = 0;
bool sdCardOK = false;
bool microphoneOK = false;
bool wifiConnected = false;
String currentFilename = "";
unsigned long recordingDuration = 0;
MenuState currentMenu = MENU_MAIN;
unsigned long lastJoystickCheck = 0;
unsigned long lastDisplayUpdate = 0;
int selectedOption = 0;
const int numMainOptions = 4; // start, upload, test, wifi settings
bool backOptionSelected = false;
bool firstBoot = true;  // Flag untuk menandai first boot
int fileActionSelection = 0; // 0: Upload, 1: Hapus
bool inFileActionMenu = false;
int wifiOption = 0;
bool isRunningDiagnostics = false; // Tambahkan di bagian global variables
bool wifiConfigCancelled = false;
unsigned long wifiConfigStartTime = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; // ms

std::vector<String> fileList;
int selectedFileIndex = 0;
MenuState previousMenu;
bool fileListGenerated = false;

// Button state variables
bool btnUpPressed = false;
bool btnDownPressed = false;
bool btnOkPressed = false;
bool prevBtnUp = false;
bool prevBtnDown = false;
bool prevBtnOk = false;

// WiFi manager variables
WiFiManager wifiManager;
bool portalRunning = false;

// WAV file header structure
struct wav_header_t {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t chunk_size;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t fmt_size = 16;
  uint16_t audio_format = 1;  // PCM
  uint16_t num_channels = CHANNELS;
  uint32_t sample_rate = SAMPLE_RATE;
  uint32_t byte_rate = SAMPLE_RATE * CHANNELS * (SAMPLE_BITS / 8);
  uint16_t block_align = CHANNELS * (SAMPLE_BITS / 8);
  uint16_t bits_per_sample = SAMPLE_BITS;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t data_size;
};

// Function prototypes
bool deleteFile(String filename);
void runDiagnostics();
void setupWiFi();
void updateMainMenu();
void checkButtonInput();
void processAudioData();
void updateRecordingScreen();
void testSDCard();
void testMicrophone();
void handleMainMenuInput();
void stopRecording();
void executeSelectedOption();
void showErrorMessage(String title, String message);
void startRecording();
void showMessage(String title, String message);
void uploadRecording(String filename);
void initI2S();
void startWiFiConfiguration();
void wifiManagerPortalCallback(WiFiManager *myWiFiManager);
int findHighestFileNumber();
void generateFileList();
void updateFileSelectMenu();
void handleFileSelectMenuInput();
void showSplashScreen();
void handleFileActionInput();
void deleteSelectedFile();
void showFileActionMenu();
void updateFileActionMenu();
void disconnectWiFi();
void updateWiFiConfigMenu(int selected);
void updateWiFiSettingsMenu(int selectedOption);
void handleWiFiSettingsInput();
void forgetWiFiNetworks();
uint64_t getSDCardFreeSpace();

void setup() {
  Serial.begin(115200);
  delay(1000); // Give time for Serial Monitor to open
  
  Serial.println("\n\n----- ESP32 INMP441 Recording to SD Card with WiFi Upload -----");
  
  // Initialize buttons
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_OK, INPUT_PULLUP);
  
  // Initialize OLED display - ThingPulse version
  display.init();
  display.setFont(ArialMT_Plain_10); // Set smaller font for small display
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  
  // Tampilkan splash screen terlebih dahulu
  showSplashScreen();

   // Tampilkan splash screen terlebih dahulu
   runDiagnostics();
  
  // Show initial menu
  updateMainMenu();

  // Initialize fileCounter based on existing files
  if (sdCardOK) {
    fileCounter = findHighestFileNumber();
    Serial.println("Memulai dengan nomor file: " + String(fileCounter));
  }
}

void loop() {
  checkButtonInput();
  
  if (isRecording) {
    processAudioData();
    
    if (millis() - lastDisplayUpdate > 500) {
      updateRecordingScreen();
      lastDisplayUpdate = millis();
    }
  }
  
  delay(10);
}

void wifiManagerPortalCallback(WiFiManager *myWiFiManager) {
  Serial.println("WiFi configuration portal started");
  Serial.println("AP SSID: " + String(AP_NAME));
  Serial.println("AP IP Address: " + WiFi.softAPIP().toString());
  
  // Show connection instructions on OLED
  display.clear();
  display.drawString(0, 0, "WiFi Config Mode");
  display.drawString(0, 10, "----------------");
  display.drawString(0, 20, "Connect to WiFi:");
  display.drawString(0, 30, AP_NAME);
  display.drawString(0, 40, "Open browser at:");
  display.drawString(0, 50, WiFi.softAPIP().toString());
  display.display();
}

void setupWiFi() {
  display.clear();
  display.drawString(0, 0, "Connecting to WiFi");
  display.display();
  
  Serial.println("Setting up WiFi connection...");
  
  // Configure WiFiManager
  wifiManager.setDebugOutput(true);
  wifiManager.setAPCallback(wifiManagerPortalCallback);
  
  // Jika sedang diagnostics, jangan buka portal
  if (isRunningDiagnostics) {
    wifiManager.setEnableConfigPortal(false); // Nonaktifkan config portal
  } else {
    wifiManager.setEnableConfigPortal(true); // Aktifkan config portal
  }
  
  bool connected = wifiManager.autoConnect(AP_NAME, AP_PASSWORD);
  
  if (connected) {
    display.clear();
    wifiConnected = true;
    Serial.println("\n✅ WiFi connected!");
    Serial.print("   IP address: ");
    Serial.println(WiFi.localIP());
    
    display.drawString(0, 20, "Connected!");
    display.drawString(0, 30, WiFi.localIP().toString());
    display.display();
    delay(3000);
    
    wifiConnected = true;
    portalRunning = false;
  } else {
    display.clear();
    wifiConnected = false;
    Serial.println("\n❌ Failed to connect to WiFi.");
    
    // Jika sedang diagnostics, tampilkan pesan singkat
    if (isRunningDiagnostics) {
      display.clear();
      display.drawString(0, 20, "WiFi: Not Connected");
      display.display();
      delay(1000);
    } else {
      display.clear();
      display.drawString(0, 20, "Connection Failed!");
      display.drawString(0, 30, "Check settings");
      display.display();
      delay(1000);
    }
    
    wifiConnected = false;
    portalRunning = false;
  }
  
  // Reset pengaturan portal setelah selesai
  wifiManager.setEnableConfigPortal(true);
}

// Start WiFi configuration portal
void startWiFiConfiguration() {
  // Reset stored WiFi settings
  // wifiManager.resetSettings();
  delay(1000);
  
  // Add custom parameter if needed
  WiFiManagerParameter custom_text("Note:", "Silahkan pilih WiFi baru", "", 50);
  wifiManager.addParameter(&custom_text);

  // Set timeout
  wifiManager.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT);

  // Show message on OLED
  display.clear();
  display.drawString(0, 0, "WiFi Configuration");
  display.drawString(0, 10, "-------------------");
  display.drawString(0, 20, "1. Pilih WiFi baru");
  display.drawString(0, 30, "2. Masukkan password");
  display.drawString(0, 40, "-------------------");
  display.drawString(0, 50, "Buka browser di:");
  display.display();
  display.drawString(0, 60, "192.168.4.1");
  display.display();

  // Start config portal
  if (!wifiManager.startConfigPortal(AP_NAME, AP_PASSWORD)) {
    Serial.println("Failed to connect or timeout");
    display.clear();
    display.drawString(0, 0, "Config Failed");
    display.drawString(0, 10, "Timeout reached");
    display.display();
    delay(2000);
  } else {
    // If successfully connected
    Serial.println("Connected to new WiFi!");
    display.clear();
    display.drawString(0, 0, "Connected to:");
    display.drawString(0, 10, WiFi.SSID());
    display.display();
    delay(2000);
  }
}

void runDiagnostics() {
  isRunningDiagnostics = true; // Set flag diagnostics
  
  display.clear();
  display.drawString(0, 0, "Running diagnostics...");
  display.display();
  
  Serial.println("\n----- Running Hardware Diagnostics -----");
  
  // Test SD card
  testSDCard();
  
  // Test microphone
  testMicrophone();
    
  // Report overall status
  Serial.println("\n----- Diagnostic Results -----");
  Serial.print("SD Card: ");
  
  display.clear();
  display.drawString(0, 0, "Diagnostic Results:");
  
  String sdStatus = "SD Card: ";
  if (sdCardOK) {
    uint64_t freeSpaceMB = getSDCardFreeSpace();
    uint64_t totalSpaceMB = SD.totalBytes() / (1024 * 1024);
    
    Serial.printf("GOOD ✓ (Free: %lluMB/%lluMB)\n", freeSpaceMB, totalSpaceMB);
    display.drawString(0, 10, sdStatus + "GOOD");
    display.drawString(0, 20, "Free: " + String((long)freeSpaceMB) + "MB/" + String((long)totalSpaceMB) + "MB");
  } else {
    Serial.println("FAILED ✗");
    display.drawString(0, 10, sdStatus + "FAILED");
  }
  
  Serial.print("Microphone: ");
  String micStatus = "Microphone: ";
  if (microphoneOK) {
    Serial.println("GOOD ✓");
    display.drawString(0, 30, micStatus + "GOOD");
  } else {
    Serial.println("FAILED ✗");
    display.drawString(0, 30, micStatus + "FAILED");
  }
  
  Serial.print("WiFi: ");
  String wifiStatus = "WiFi: ";
  if (wifiConnected) {
    Serial.println("CONNECTED ✓");
    display.drawString(0, 40, wifiStatus + "CONNECTED");
    display.drawString(0, 50, WiFi.SSID());
  } else {
    Serial.println("DISCONNECTED ✗");
    display.drawString(0, 40, wifiStatus + "DISCONNECTED");
  }
  
  display.display();
  delay(4000);  // Beri waktu lebih untuk membaca informasi
  
  Serial.println("-------------------------------");
  
  isRunningDiagnostics = false; // Reset flag diagnostics
}

void showSplashScreen() {
  display.clear();
  display.setFont(ArialMT_Plain_16); // Use larger font for splash screen
  
  // Calculate center position for text
  int16_t textWidth = display.getStringWidth("SMARTNOTE");
  int16_t x = (SCREEN_WIDTH - textWidth) / 2;
  int16_t y = (SCREEN_HEIGHT - 16) / 2; // 16 is the height of ArialMT_Plain_16
  
  display.drawString(x, y, "SMARTNOTE");
  display.display();
  delay(2000);  // Display for 2 seconds
  
  // Return to normal font
  display.setFont(ArialMT_Plain_10);
  firstBoot = false;
}

void testSDCard() {
  display.drawString(0, 10, "Testing SD card...");
  display.display();
  Serial.println("Testing SD card...");
  
  // Initialize SPI
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  
  // Try to initialize the SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD card initialization failed! Check connections.");
    display.drawString(0, 20, "SD init failed!");
    display.display();
    sdCardOK = false;
    return;
  }
  
  // Check card type
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("❌ No SD card detected! Check if card is inserted.");
    display.drawString(0, 20, "No SD card found!");
    display.display();
    sdCardOK = false;
    return;
  }
  
  String cardTypeStr = "UNKNOWN";
  if (cardType == CARD_MMC) cardTypeStr = "MMC";
  else if (cardType == CARD_SD) cardTypeStr = "SDSC";
  else if (cardType == CARD_SDHC) cardTypeStr = "SDHC";
  
  // Get card info
  uint64_t cardSizeMB = SD.cardSize() / (1024 * 1024);
  uint64_t freeSpaceMB = getSDCardFreeSpace();
  
  // Check R/W by writing and reading a test file
  const char* testFileName = "/test.txt";
  const char* testData = "ESP32 SD Card Test";
  
  // Write test file
  File testFile = SD.open(testFileName, FILE_WRITE);
  if (!testFile) {
    Serial.println("❌ Failed to open test file for writing!");
    display.drawString(0, 20, "File write failed!");
    display.display();
    sdCardOK = false;
    return;
  }
  
  if (testFile.print(testData)) {
    // Write successful
  } else {
    Serial.println("❌ Failed to write to test file!");
    testFile.close();
    sdCardOK = false;
    return;
  }
  testFile.close();
  
  // Read test file
  testFile = SD.open(testFileName);
  if (!testFile) {
    Serial.println("❌ Failed to open test file for reading!");
    sdCardOK = false;
    return;
  }
  
  String readData = testFile.readString();
  testFile.close();
  
  if (readData != testData) {
    Serial.println("❌ Data verification failed!");
    sdCardOK = false;
    return;
  }
  
  // Clean up
  SD.remove(testFileName);
  
  Serial.printf("✅ SD card working properly!\n   Type: %s, Size: %lluMB, Free: %lluMB\n", 
               cardTypeStr.c_str(), cardSizeMB, freeSpaceMB);
  
  display.drawString(0, 20, "SD card OK!");
  display.display();
  
  sdCardOK = true;
}

void testMicrophone() {
  display.drawString(0, 40, "Testing microphone...");
  display.drawString(0, 50, "Make some noise!");
  display.display();
  
  Serial.println("Testing microphone...");
  
  // Initialize I2S
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)SAMPLE_BITS,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  
  // Try to install the I2S driver
  esp_err_t result = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (result != ESP_OK) {
    Serial.println("❌ Failed to install I2S driver!");
    microphoneOK = false;
    return;
  }
  
  result = i2s_set_pin(I2S_PORT, &pin_config);
  if (result != ESP_OK) {
    Serial.println("❌ Failed to set I2S pins!");
    i2s_driver_uninstall(I2S_PORT);
    microphoneOK = false;
    return;
  }
  
  // Start collecting audio data
  uint8_t buffer[BUFFER_SIZE];
  size_t bytes_read;
  
  Serial.println("Please make some noise (speak, clap, tap mic)...");
  
  int16_t maxAmplitude = 0;
  int16_t minAmplitude = 0;
  unsigned long startTime = millis();
  
  // Collect audio samples for the test duration
  while (millis() - startTime < MIC_TEST_DURATION_MS) {
    // Read audio data from I2S
    i2s_read(I2S_PORT, buffer, sizeof(buffer), &bytes_read, 0);
    
    if (bytes_read > 0) {
      // Process the buffer as 16-bit samples to find amplitude
      int16_t* samples = (int16_t*)buffer;
      int samplesRead = bytes_read / 2; // Each sample is 2 bytes
      
      for (int i = 0; i < samplesRead; i++) {
        // Update max and min amplitude
        if (samples[i] > maxAmplitude) maxAmplitude = samples[i];
        if (samples[i] < minAmplitude) minAmplitude = samples[i];
      }
    }
    
    // Show progress dots
    static unsigned long lastDotTime = 0;
    if (millis() - lastDotTime >= 200) {
      Serial.print(".");
      static int dotCount = 0;
      display.drawString(10 + (dotCount * 6), 60, ".");
      display.display();
      dotCount++;
      lastDotTime = millis();
    }
  }
  
  // Clean up I2S
  i2s_driver_uninstall(I2S_PORT);
  
  // Calculate peak-to-peak amplitude
  int16_t peakToPeak = maxAmplitude - minAmplitude;
  
  Serial.println("");
  Serial.printf("   Peak-to-peak amplitude: %d\n", peakToPeak);
  
  display.clear();
  display.drawString(0, 0, "Microphone Test");
  display.drawString(0, 10, "Amplitude: " + String(peakToPeak));
  display.display();
  
  if (peakToPeak > MIC_AMPLITUDE_THRESHOLD) {
    Serial.println("✅ Microphone detected audio input!");
    display.drawString(0, 20, "Mic OK!");
    display.display();
    microphoneOK = true;
  } else {
    Serial.println("❌ Low or no audio detected. Check microphone connections.");
    display.drawString(0, 20, "Mic failed!");
    display.display();
    microphoneOK = false;
  }
}

void checkButtonInput() {
  // Only check input every debounceDelay ms to prevent bouncing
  if (millis() - lastDebounceTime < debounceDelay) {
    return;
  }
  
  lastDebounceTime = millis();
  
  // Read button status
  bool btnUp = !digitalRead(BUTTON_UP);
  bool btnDown = !digitalRead(BUTTON_DOWN);
  bool btnOk = !digitalRead(BUTTON_OK);
  
  // Detect button state changes (edge detection)
  btnUpPressed = (btnUp && !prevBtnUp);
  btnDownPressed = (btnDown && !prevBtnDown);
  btnOkPressed = (btnOk && !prevBtnOk);
  
  // Save current status for next detection
  prevBtnUp = btnUp;
  prevBtnDown = btnDown;
  prevBtnOk = btnOk;
  
  // Handle input based on current menu
  switch (currentMenu) {
    case MENU_MAIN:
      handleMainMenuInput();
      break;
    case MENU_RECORDING:
      if (btnOkPressed) {
        stopRecording();
        currentMenu = MENU_MAIN;
        updateMainMenu();
      }
      break;
    case MENU_WIFI_SETTINGS:
      handleWiFiSettingsInput();
      break;
    case MENU_UPLOADING:
      // No input during upload
      break;
    case MENU_TESTING:
      // No input during testing
      break;
    case MENU_FILE_SELECT:
      handleFileSelectMenuInput();
      break;
  }
}

void handleMainMenuInput() {
  // Handle up navigation
  if (btnUpPressed) {
    selectedOption = (selectedOption > 0) ? selectedOption - 1 : 0;
    updateMainMenu();
  }
  // Handle down navigation
  else if (btnDownPressed) {
    selectedOption = (selectedOption < numMainOptions - 1) ? selectedOption + 1 : numMainOptions - 1;
    updateMainMenu();
  }
  // Handle selection
  else if (btnOkPressed) {
    executeSelectedOption();
  }
}

void executeSelectedOption() {
  switch (selectedOption) {
    case 0: // Start Recording
      if (!sdCardOK || !microphoneOK) {
        showErrorMessage("Hardware error!", "Check SD & mic");
      } else if (!isRecording) {
        startRecording();
      }
      break;
    case 1: // Upload Recording
      if (!sdCardOK) {
        showErrorMessage("Hardware error!", "Check SD card");
      } else {
        // Switch to file selection menu
        previousMenu = MENU_MAIN;
        currentMenu = MENU_FILE_SELECT;
        generateFileList();
        updateFileSelectMenu();
      }
      break;
    case 2: // Run Diagnostics
      if (isRecording) {
        showErrorMessage("Stop recording", "before testing");
      } else {
        currentMenu = MENU_TESTING;
        runDiagnostics();
        setupWiFi();
        currentMenu = MENU_MAIN;
        updateMainMenu();
      }
      break;

    case 3: // WiFi Settings
      currentMenu = MENU_WIFI_SETTINGS;
      wifiOption = 0; // Reset to first option
      updateWiFiSettingsMenu(wifiOption);
      break;
  }
}

int findHighestFileNumber() {
  int highestNumber = -1;
  File root = SD.open("/");
  
  if (!root) {
    Serial.println("Failed to open root directory");
    return 0;
  }
  
  Serial.println("Scanning SD card for existing recordings...");
  
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      // No more files
      break;
    }
    
    String filename = String(entry.name());
    entry.close();
    
    // Check if this is a recording file
    if (filename.startsWith("recording_") && filename.endsWith(".wav")) {
      // Extract the number part
      int underscorePos = filename.indexOf('_');
      int dotPos = filename.lastIndexOf('.');
      
      if (underscorePos >= 0 && dotPos > underscorePos) {
        String numberPart = filename.substring(underscorePos + 1, dotPos);
        int fileNumber = numberPart.toInt();
        
        Serial.println("Found recording file: " + filename + " (number: " + String(fileNumber) + ")");
        
        // Update highest number if this is larger
        if (fileNumber > highestNumber) {
          highestNumber = fileNumber;
        }
      }
    }
  }
  
  root.close();
  return (highestNumber + 1); // Return next available number
}

// Function to generate list of recording files from SD card
void generateFileList() {
  fileList.clear();
  selectedFileIndex = 0;
  
  File root = SD.open("/");
  if (!root) {
    Serial.println("Failed to open root directory");
    return;
  }
  
  Serial.println("Looking for recording files on SD card...");
  
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      // No more files
      break;
    }
    
    String filename = String(entry.name());
    entry.close();
    
    // Only add WAV files to the list
    if (filename.endsWith(".wav")) {
      fileList.push_back(filename);
      Serial.println("Found file: " + filename);
    }
  }
  
  root.close();
  
  // Sort file list (optional)
  // std::sort(fileList.begin(), fileList.end());
  
  fileListGenerated = true;
  Serial.println("Total files found: " + String(fileList.size()));
}

void disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect(true);  // true = turn off WiFi
    WiFi.mode(WIFI_OFF);
    wifiConnected = false;
    
    Serial.println("WiFi disconnected!");
    showMessage("WiFi Status", "Disconnected");
    delay(2000);
  } else {
    showMessage("WiFi Status", "Already disconnected");
    delay(1000);
  }
}

void updateFileActionMenu() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  int y = 0;
  display.drawString(0, y, "Pilih Aksi:");
  y += 10;
  display.drawString(0, y, fileList[selectedFileIndex]);
  y += 10;
  display.drawString(0, y, "-------------------");
  y += 12;
  
  // Use ternary operator to select the prefix
  display.drawString(0, y, fileActionSelection == 0 ? "> Upload" : "  Upload");
  y += 10;
  display.drawString(0, y, fileActionSelection == 1 ? "> Delete" : "  Delete");
  y += 10;
  display.drawString(0, y, fileActionSelection == 2 ? "> Back" : "  Back");
  
  display.display();
}

// Fungsi untuk menampilkan menu pemilihan file
void updateFileSelectMenu() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  int y = 0;
  display.drawString(0, y, "PILIH FILE");
  y += 10;
  display.drawString(0, y, "-------------------");
  y += 12;
  
  if (fileList.size() == 0) {
    display.drawString(0, y, "Tidak ada file");
    y += 10;
    display.drawString(0, y, "rekaman ditemukan!");
  } else {
    int startIdx = max(0, selectedFileIndex);
    int endIdx = min((int)fileList.size() - 1, startIdx + 2);
    
    for (int i = startIdx; i <= endIdx; i++) {
      String prefix = (i == selectedFileIndex && !backOptionSelected) ? "> " : "  ";
      String displayName = fileList[i];
      if (displayName.length() > 16) {
        displayName = displayName.substring(0, 13) + "...";
      }
      display.drawString(0, y, prefix + displayName);
      y += 10;
    }
  }
  
  y += 2;
  display.drawString(0, y, "-------------------");
  y += 12;
  
  String backPrefix = backOptionSelected ? "> " : "  ";
  display.drawString(0, y, backPrefix + "Back");
  y += 12;
  
  display.drawString(0, y, "-------------------");
  y += 12;
  display.drawString(0, y, "OK: Pilih File");
  
  display.display();
}

// Fungsi untuk menangani input pada menu pemilihan file
void handleFileSelectMenuInput() {
  if (inFileActionMenu) {
    // Handle menu aksi file
    if (btnUpPressed) {
      // Navigate up through options
      fileActionSelection = (fileActionSelection > 0) ? fileActionSelection - 1 : 0;
      updateFileActionMenu();
    }
    else if (btnDownPressed) {
      // Navigate down through options (now with 3 options)
      fileActionSelection = (fileActionSelection < 2) ? fileActionSelection + 1 : 2;
      updateFileActionMenu();
    }
    else if (btnOkPressed) {
      if (fileActionSelection == 0) {
        // Upload
        currentMenu = MENU_UPLOADING;
        inFileActionMenu = false;
        showMessage("Uploading...", "Mohon tunggu");
        uploadRecording("/" + fileList[selectedFileIndex]);
        delay(1000);
      } else if (fileActionSelection == 1) {
        // Hapus
        if (SD.remove("/" + fileList[selectedFileIndex])) {
          showMessage("Sukses", "File dihapus");
          generateFileList();
        } else {
          showErrorMessage("Gagal", "Hapus file");
        }
        inFileActionMenu = false;
      } else { // fileActionSelection == 2
        // Back to file select menu
        inFileActionMenu = false;
      }
      currentMenu = MENU_FILE_SELECT;
      updateFileSelectMenu();
    }
  } else {
    // Handle menu pilih file biasa
    // (Rest of this function remains unchanged)
    if (btnUpPressed) {
      if (backOptionSelected) {
        backOptionSelected = false;
        if (fileList.size() > 0) {
          selectedFileIndex = fileList.size() - 1;
        }
      } else if (selectedFileIndex > 0) {
        selectedFileIndex--;
      }
      updateFileSelectMenu();
    }
    else if (btnDownPressed) {
      if (!backOptionSelected) {
        if (selectedFileIndex < fileList.size() - 1) {
          selectedFileIndex++;
        } else {
          backOptionSelected = true;
        }
      }
      updateFileSelectMenu();
    }
    else if (btnOkPressed) {
      if (backOptionSelected) {
        currentMenu = MENU_MAIN;
        backOptionSelected = false;
        updateMainMenu();
      } else if (fileList.size() > 0) {
        inFileActionMenu = true;
        fileActionSelection = 0; // Reset selection to first option when entering menu
        updateFileActionMenu();
      }
    }
  }
}


bool deleteFile(String filename) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  int y = 0;
  display.drawString(0, y, "Menghapus file:");
  y += 10;
  display.drawString(0, y, filename);
  display.display();
  
  if (SD.remove(filename)) {
    Serial.println("File deleted: " + filename);
    y += 20;
    display.drawString(0, y, "Sukses dihapus!");
    display.display();
    delay(1000);
    return true;
  } else {
    Serial.println("Failed to delete: " + filename);
    y += 20;
    display.drawString(0, y, "Gagal menghapus!");
    display.display();
    delay(1000);
    return false;
  }
}

void handleFileActionInput() {
  static int actionSelection = 0; // 0: Upload, 1: Delete, 2: Cancel
  
  if (btnUpPressed) {
    actionSelection = (actionSelection > 0) ? actionSelection - 1 : 0;
    updateFileActionMenu();
  }
  else if (btnDownPressed) {
    actionSelection = (actionSelection < 2) ? actionSelection + 1 : 2;
    updateFileActionMenu();
  }
  else if (btnOkPressed) {
    switch (actionSelection) {
      case 0: // Upload
        currentMenu = MENU_UPLOADING;
        showMessage("Uploading...", "Please wait");
        uploadRecording("/" + fileList[selectedFileIndex]);
        delay(1000);
        currentMenu = MENU_MAIN;
        updateMainMenu();
        break;
      case 1: // Delete
        deleteSelectedFile();
        break;
      case 2: // Cancel
        currentMenu = MENU_FILE_SELECT;
        updateFileSelectMenu();
        break;
    }
  }
}

void deleteSelectedFile() {
  String filename = "/" + fileList[selectedFileIndex];
  
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  int y = 0;
  display.drawString(0, y, "Delete File:");
  y += 10;
  display.drawString(0, y, filename);
  y += 10;
  display.drawString(0, y, "Are you sure?");
  y += 10;
  display.drawString(0, y, "> Yes");
  y += 10;
  display.drawString(0, y, "  No");
  display.display();
  
  // Tunggu konfirmasi
  bool confirmed = false;
  bool cancelled = false;
  int confirmSelection = 0;
  
  while (!confirmed && !cancelled) {
    if (btnUpPressed || btnDownPressed) {
      confirmSelection = !confirmSelection;
      
      display.clear();
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      
      int y = 0;
      display.drawString(0, y, "Delete File:");
      y += 10;
      display.drawString(0, y, filename);
      y += 10;
      display.drawString(0, y, "Are you sure?");
      y += 10;
      display.drawString(0, y, confirmSelection == 0 ? "> Yes" : "  Yes");
      y += 10;
      display.drawString(0, y, confirmSelection == 1 ? "> No" : "  No");
      display.display();
    }
    
    if (btnOkPressed) {
      if (confirmSelection == 0) {
        confirmed = true;
      } else {
        cancelled = true;
      }
    }
    
    delay(100);
  }
  
  if (confirmed) {
    if (SD.remove(filename)) {
      showMessage("Success", "File deleted");
      // Perbarui daftar file
      generateFileList();
    } else {
      showErrorMessage("Error", "Delete failed");
    }
  }
  
  currentMenu = MENU_FILE_SELECT;
  updateFileSelectMenu();
}

void updateMainMenu() {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  int y = 0;
  display.drawString(0, y, "SMARTNOTE");
  y += 10;
  display.drawString(0, y, "-------------------");
  y += 12;
  
  // ... (status indicators jika ada)
  
  // Display menu options
  for (int i = 0; i < numMainOptions; i++) {
    String prefix = (i == selectedOption) ? "> " : "  ";
    String menuText;
    
    switch (i) {
      case 0: menuText = "Start Recording"; break;
      case 1: menuText = "Upload Recording"; break;
      case 2: menuText = "Run Diagnostics"; break;
      case 3: menuText = "WiFi Settings"; break;  // Opsi baru
    }
    
    display.drawString(0, y, prefix + menuText);
    y += 10;
  }
  
  // ... (info rekaman terakhir jika ada)
  
  display.display();
}

void updateRecordingScreen() {
  unsigned long elapsedMillis = millis() - recordingStartTime;
  unsigned long totalSeconds = elapsedMillis / 1000;
  unsigned long hours = totalSeconds / 3600;
  unsigned long minutes = (totalSeconds % 3600) / 60;
  unsigned long seconds = totalSeconds % 60;
  unsigned long millisecs = elapsedMillis % 1000;

  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  int y = 0;
  display.drawString(0, y, "RECORDING");
  y += 10;
  display.drawString(0, y, "-------------------");
  y += 12;

  String timeStr = "Time: ";
  if (hours < 10) timeStr += '0';
  timeStr += String(hours) + ':';
  if (minutes < 10) timeStr += '0';
  timeStr += String(minutes) + ':';
  if (seconds < 10) timeStr += '0';
  timeStr += String(seconds) + '.';
  if (millisecs < 100) timeStr += '0';
  if (millisecs < 10) timeStr += '0';
  timeStr += String(millisecs);
  display.drawString(0, y, timeStr);
  y += 10;

  display.drawString(0, y, "File: " + currentFilename.substring(1));
  y += 20;

  display.drawString(0, y, "Press button to stop");

  // Flashing recording indicator
  if ((millis() / 500) % 2) {
    display.fillCircle(120, 10, 5);
  }

  display.display();
}

void updateWiFiSettingsMenu(int selectedOption) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  int y = 0;
  display.drawString(0, y, "WiFi Settings");
  y += 10;

  
  if (wifiConnected) {
    display.drawString(0, y, "SSID: " + WiFi.SSID());
    y += 10;
  }
  
  display.drawString(0, y, "-------------------");
  y += 12;
  
  // Opsi menu dengan highlight
  display.drawString(0, y, selectedOption == 0 ? 
                   "> " + String(wifiConnected ? "Disconnect" : "Connect") :
                   "  " + String(wifiConnected ? "Disconnect" : "Connect"));
  y += 10;
  
  display.drawString(0, y, selectedOption == 1 ? "> Change Network" : "  Change Network");
  y += 10;
  
  display.drawString(0, y, selectedOption == 2 ? "> Back" : "  Back");
  
  display.display();
}

void handleWiFiSettingsInput() {
  static int wifiOption = 0;
  
  if (btnUpPressed) {
    wifiOption = (wifiOption > 0) ? wifiOption - 1 : 2;
    updateWiFiSettingsMenu(wifiOption);
  }
  else if (btnDownPressed) {
    wifiOption = (wifiOption < 2) ? wifiOption + 1 : 0;
    updateWiFiSettingsMenu(wifiOption);
  }
  else if (btnOkPressed) {
    switch (wifiOption) {
      case 0: // Connect/Disconnect
        if (wifiConnected) {
          disconnectWiFi();
        } else {
          setupWiFi();
        }
        break;
      case 1: // Change Network
        startWiFiConfiguration();
        break;
      case 2: // Forget Network
      currentMenu = MENU_MAIN;
      updateMainMenu();
      return; // Langsung kembali tanpa update menu WiFi settings
      break;
    }
    updateWiFiSettingsMenu(wifiOption);
  }
}

void showMessage(String title, String message) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  int y = 0;
  display.drawString(0, y, title);
  y += 10;
  display.drawString(0, y, "-------------------");
  y += 12;
  display.drawString(0, y, message);
  display.display();
}

uint64_t getSDCardFreeSpace() {
  if (!sdCardOK) return 0;
  return (SD.totalBytes() - SD.usedBytes()) / (1024 * 1024); // Return in MB
}

void showErrorMessage(String title, String message) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  // Title at top
  display.drawString(0, 0, title);
  
  // Separator line 2 pixels below title
  display.drawHorizontalLine(0, 12, SCREEN_WIDTH);
  
  // Message below separator
  display.drawString(0, 14, message);
  
  display.display();
  delay(2000);
  updateMainMenu();
}

void initI2S() {
  Serial.println("Initializing I2S...");
  
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = (i2s_bits_per_sample_t)SAMPLE_BITS,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BUFFER_SIZE,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

void startRecording() {
  // Initialize I2S for recording
  initI2S();
  
  // Create a new filename with a counter to avoid overwriting
  currentFilename = "/recording_" + String(fileCounter++) + ".wav";
  
  // Open file for writing
  audioFile = SD.open(currentFilename, FILE_WRITE);
  if (!audioFile) {
    showErrorMessage("File Error", "Can't create file");
    return;
  }
  
  // Write a placeholder header (will be updated when closing)
  wav_header_t header;
  // Set header fields to zero for now (will update when stopping)
  header.chunk_size = 0;
  header.data_size = 0;
  
  audioFile.write((const uint8_t*)&header, sizeof(wav_header_t));
  
  // Set recording flag and start time
  isRecording = true;
  recordingStartTime = millis();
  currentMenu = MENU_RECORDING;
  
  Serial.println("Recording started: " + currentFilename);
  updateRecordingScreen();
}

void stopRecording() {
  if (!isRecording || !audioFile) {
    return;
  }
  
  isRecording = false;
  
  // Get the size of recorded data (excluding header)
  uint32_t data_size = audioFile.size() - sizeof(wav_header_t);
  
  // Update the WAV header with the actual size
  wav_header_t header;
  header.chunk_size = data_size + 36; // 36 = size of the rest of the header
  header.data_size = data_size;
  
  // Seek back to the beginning of the file and write the updated header
  audioFile.seek(0);
  audioFile.write((const uint8_t*)&header, sizeof(wav_header_t));
  
  // Close the file
  audioFile.close();
  
  // Clean up I2S
  i2s_driver_uninstall(I2S_PORT);
  
  // Calculate and display recording duration
  recordingDuration = millis() - recordingStartTime;
  unsigned long duration = recordingDuration / 1000;
  
  Serial.println("Recording stopped!");
  Serial.printf("Duration: %lu seconds\n", duration);
  
  showMessage("Recording Stopped", "Duration: " + String(duration) );
  delay(2000);
}

void processAudioData() {
  // Buffer for audio data
  uint8_t buffer[BUFFER_SIZE];
  size_t bytes_read = 0;
  
  // Read audio data from I2S
  i2s_read(I2S_PORT, buffer, sizeof(buffer), &bytes_read, 0);
  
  if (bytes_read > 0) {
    // Write audio data to file
    audioFile.write(buffer, bytes_read);
    
    // Periodically display recording progress in Serial
    unsigned long elapsedSecs = (millis() - recordingStartTime) / 1000;
    static unsigned long lastReportTime = 0;
    
    if (millis() - lastReportTime >= 1000) {  // Update every second
      Serial.printf("Recording... %lu seconds\n", elapsedSecs);
      lastReportTime = millis();
    }
  }
}

void uploadRecording(String filename) {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    showMessage("WiFi Error", "Reconnecting...");
    setupWiFi();
    if (!wifiConnected) {
      showErrorMessage("WiFi Failed", "Upload canceled");
      return;
    }
  }
  
  Serial.println("Opening file: " + filename);
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  
  int y = 0;
  display.drawString(0, y, "Uploading:");
  y += 10;
  display.drawString(0, y, filename.substring(1));
  y += 10;
  display.drawString(0, y, "-------------------");
  y += 12;
  display.display();
  
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    showErrorMessage("File Error", "Can't open file");
    return;
  }
  
  uint32_t fileSize = file.size();
  Serial.println("File size: " + String(fileSize) + " bytes");
  
  display.drawString(0, y, "File size: " + String(fileSize/1024) + " KB");
  y += 10;
  display.display();
  
  // Create HTTP client
  HTTPClient http;
  http.setConnectTimeout(10000); // 10 seconds timeout for connection
  
  Serial.print("Connecting to server...");
  display.drawString(0, y, "Connecting...");
  y += 10;
  display.display();
  
  // Start connection and send HTTP header
  http.begin(serverUrl);
  http.addHeader("Content-Type", "audio/wav");
  http.addHeader("Content-Disposition", "attachment; filename=" + filename.substring(1)); // Remove leading slash
  
  Serial.println("Uploading file...");
  display.drawString(0, y, "Sending data...");
  y += 10;
  display.display();
  
  // Start the POST request with the file size
  int httpCode = http.sendRequest("POST", &file, fileSize);
  
  // httpCode will be negative on error
  if (httpCode > 0) {
    Serial.printf("HTTP Response code: %d\n", httpCode);
    display.drawString(0, y, "Response: " + String(httpCode));
    y += 10;
    
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Server response: " + payload);
      display.drawString(0, y, "Success!");
    } else {
      display.drawString(0, y, "Server error!");
    }
  } else {
    Serial.printf("HTTP POST failed, error: %s\n", http.errorToString(httpCode).c_str());
    display.drawString(0, y, "Failed!");
    y += 10;
    display.drawString(0, y, http.errorToString(httpCode));
  }
  
  display.display();
  delay(2000);
  
  http.end();
  file.close();
  Serial.println("Upload process completed!");
}