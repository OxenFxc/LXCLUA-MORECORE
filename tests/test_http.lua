
local http = require "http"
print("HTTP library loaded: " .. tostring(http))
print("http.get: " .. tostring(http.get))
print("http.post: " .. tostring(http.post))

-- Attempt a request (expect failure due to no internet, but code execution path is tested)
local status, body = http.get("http://example.com")
print("Status: " .. tostring(status))
print("Body: " .. tostring(body))

-- Test invalid URL
status, body = http.get("invalid-url")
print("Invalid URL Status: " .. tostring(status))
print("Invalid URL Body: " .. tostring(body))

-- Test HTTPS (expect not supported on Linux)
status, body = http.get("https://example.com")
print("HTTPS Status: " .. tostring(status))
print("HTTPS Body: " .. tostring(body))
