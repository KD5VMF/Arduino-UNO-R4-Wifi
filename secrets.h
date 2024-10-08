// secrets.h
// This file stores sensitive information such as WiFi credentials and API tokens.
// Ensure this file is added to your `.gitignore` to prevent it from being uploaded to version control systems.

#ifndef SECRETS_H
#define SECRETS_H

// ------------------------------
// WiFi Credentials
// ------------------------------

// Replace the placeholder text with your actual WiFi SSID and Password.
// Ensure that these strings are enclosed in double quotes.

const char* WIFI_SSID = "SSID";
const char* WIFI_PASSWORD = "PASSWORD";

// ------------------------------
// Weather API Credentials
// ------------------------------

// NOAA CDO (Climate Data Online) API Token
// Obtain your token from https://www.ncdc.noaa.gov/cdo-web/token
const char* WEATHER_API_TOKEN = "TOKEN";

// ------------------------------
// Additional Secrets (Optional)
// ------------------------------

// If your project requires other sensitive information, such as API keys or passwords for other services,
// you can add them below following the same pattern.

// Example:
// const char* API_KEY = "Your_API_Key_Here";
// const char* ADMIN_PASSWORD = "Your_Admin_Password";

#endif // SECRETS_H
