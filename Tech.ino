#include <Wire.h>
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h> // For NTP time

// WiFi credentials
#define WIFI_SSID "Rakibul Hasan"
#define WIFI_PASSWORD "7254123456"

// Firebase credentials
#define FIREBASE_HOST "restaurant-6c771-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "YVgOSjamCI5PCcIX6fuSTGxE3lNfrwV1hszZpAzz"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// OLED setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Button pins
#define BUTTON_MENU    0   // GPIO0
#define BUTTON_SELECT 14   // GPIO14
#define BUTTON_UP     12   // GPIO12
#define BUTTON_DOWN   13   // GPIO13

// Menu and prices
String menuItems[] = {"Burger", "Pizza", "Pasta", "Fries", "Drinks"};
int prices[] = {200, 500, 120, 100, 50}; // Matching prices
int totalItems = sizeof(menuItems) / sizeof(menuItems[0]);
int selectedItem = 0;
bool showMenu = true;
bool selectingQuantity = false;
int quantity = 1;

// Cart
String cart[10];
int quantities[10];
int cartIndex = 0;

// Debounce helper
bool isButtonPressed(int pin) {
  static unsigned long lastPressTimes[40] = {0};
  const unsigned long debounceDelay = 50;

  if (digitalRead(pin) == LOW) {
    if (millis() - lastPressTimes[pin] > debounceDelay) {
      lastPressTimes[pin] = millis();
      return true;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);

  // WiFi connect
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");

  // Firebase setup
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // NTP Time setup
  configTime(21600, 0, "pool.ntp.org"); // GMT+6 for Bangladesh
  Serial.println("Waiting for NTP time sync...");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nTime synchronized!");

  // OLED setup
  Wire.begin(5, 4); // SDA=GPIO5, SCL=GPIO4
  pinMode(BUTTON_MENU, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED not found"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Loading Menu...");
  display.display();
  delay(1000);
}

void loop() {
  static unsigned long selectPressedTime = 0;
  static bool selectPressed = false;

  if (showMenu && !selectingQuantity) {
    if (isButtonPressed(BUTTON_UP)) {
      selectedItem--;
      if (selectedItem < 0) selectedItem = totalItems - 1;
      delay(100);
    }

    if (isButtonPressed(BUTTON_DOWN)) {
      selectedItem++;
      if (selectedItem >= totalItems) selectedItem = 0;
      delay(100);
    }

    if (isButtonPressed(BUTTON_SELECT)) {
      selectingQuantity = true;
      quantity = 1;
      delay(150);
    }

    drawMenu();
  }

  if (selectingQuantity) {
    if (isButtonPressed(BUTTON_UP)) {
      quantity++;
      delay(100);
    }

    if (isButtonPressed(BUTTON_DOWN) && quantity > 1) {
      quantity--;
      delay(100);
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      if (!selectPressed) {
        selectPressed = true;
        selectPressedTime = millis();
      } else if (millis() - selectPressedTime > 1500) {
        placeOrder();
        selectingQuantity = false;
        showMenu = true;
        selectPressed = false;
        delay(1000);
        return;
      }
    } else if (selectPressed) {
      selectPressed = false;

      if (millis() - selectPressedTime < 1500) {
        if (cartIndex < 10) {
          cart[cartIndex] = menuItems[selectedItem];
          quantities[cartIndex] = quantity;
          cartIndex++;
        }

        display.clearDisplay();
        display.setCursor(0, 0);
        display.setTextColor(SSD1306_WHITE);
        display.println("Added to cart:");
        display.setCursor(0, 20);
        display.println(menuItems[selectedItem] + " x" + quantity);
        display.display();
        delay(2000);

        selectingQuantity = false;
        showMenu = true;
        delay(200);
      }
    }

    drawQuantitySelection();
  }
}

void drawMenu() {
  display.clearDisplay();
  for (int i = 0; i < totalItems; i++) {
    if (i == selectedItem) {
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(0, i * 10);
    display.println(menuItems[i]);
  }
  display.display();
}

void drawQuantitySelection() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Qty for:");
  display.setCursor(0, 10);
  display.println(menuItems[selectedItem]);
  display.setCursor(0, 30);
  display.print("Quantity: ");
  display.println(quantity);
  display.setCursor(0, 50);
  display.println("Hold SELECT to order");
  display.display();
}

String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "00:00:00";
  }
  char timeStr[12];
  strftime(timeStr, sizeof(timeStr), "%I:%M:%S %p", &timeinfo);
  return String(timeStr);
}

String generateOrderID() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Order_" + String(millis());
  }
  char orderId[32];
  strftime(orderId, sizeof(orderId), "Order_%Y%m%d_%H%M%S", &timeinfo);
  return String(orderId);
}

void placeOrder() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextColor(SSD1306_WHITE);
  display.println("Order Placed!");
  display.setCursor(0, 15);
  display.println("Sending...");
  display.display();

  // Prepare data
  String orderId = generateOrderID();
  String itemsList = "";
  int totalAmount = 0;

  for (int i = 0; i < cartIndex; i++) {
    itemsList += cart[i] + " x" + quantities[i];
    if (i < cartIndex - 1) itemsList += ", ";
    for (int j = 0; j < totalItems; j++) {
      if (cart[i] == menuItems[j]) {
        totalAmount += prices[j] * quantities[i];
        break;
      }
    }
  }

  String tableNo = "5"; // Example Table
  String orderTime = getCurrentTime();
  String paymentStatus = "Pending";
  String orderStatus = "Processing";

  // Push to Firebase
  FirebaseJson orderData;
  orderData.set("tableNo", tableNo);
  orderData.set("items", itemsList);
  orderData.set("orderTime", orderTime);
  orderData.set("payment", paymentStatus);
  orderData.set("status", orderStatus);
  orderData.set("amount", totalAmount); // Push total amount also!

  if (Firebase.setJSON(fbdo, "/orders/" + orderId, orderData)) {
    Serial.println("Order pushed successfully!");
    Serial.print("Total Amount: ৳");
    Serial.println(totalAmount);
  } else {
    Serial.println("Firebase push failed:");
    Serial.println(fbdo.errorReason());
  }

  delay(3000);

  cartIndex = 0; // Clear cart after placing order
}
