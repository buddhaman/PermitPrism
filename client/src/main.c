#include <stdio.h>
#include <stdlib.h>

#include "cJSON.h"
#include "cJSON.c"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>


LPSTR make_http_request(LPCWSTR host, INTERNET_PORT port, LPCWSTR path, int use_https, const char* content_type, const char* payload) {
    HINTERNET hInternet = WinHttpOpen(L"WinHTTP Example/1.0",  
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, 
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hInternet) {
        fprintf(stderr, "WinHttpOpen failed\n");
        return NULL;
    }

    HINTERNET hConnect = WinHttpConnect(hInternet, host, 
                                      port,  // Explicitly set port to 8080
                                      0);
    if (!hConnect) {
        fprintf(stderr, "WinHttpConnect failed\n");
        WinHttpCloseHandle(hInternet);
        return NULL;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path,
                                          NULL, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          use_https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        fprintf(stderr, "WinHttpOpenRequest failed\n");
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hInternet);
        return NULL;
    }

    // Construct the headers
    wchar_t headers[256];
    swprintf(headers, sizeof(headers) / sizeof(wchar_t), L"Content-Type: %S\r\n", content_type);

    // Send the request with the specified payload and content type
    if (!WinHttpSendRequest(hRequest, 
                            headers, 
                            -1, 
                            (LPVOID)payload, 
                            strlen(payload), 
                            strlen(payload), 
                            0)) {
        fprintf(stderr, "WinHttpSendRequest failed\n");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hInternet);
        return NULL;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        fprintf(stderr, "WinHttpReceiveResponse failed\n");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hInternet);
        return NULL;
    }

    // Read data in chunks
    DWORD totalSize = 0;  // Make sure this is initialized to 0
    DWORD bufferSize = 4096;  // Start with 4KB
    char* buffer = (char*)malloc(bufferSize);
    char tempBuffer[4096];
    
    if (!buffer) {
        fprintf(stderr, "Initial memory allocation failed\n");
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hInternet);
        return NULL;
    }

    // Zero out the initial buffer
    memset(buffer, 0, bufferSize);
    fprintf(stderr, "Initial buffer allocated at %p with size %lu\n", (void*)buffer, bufferSize);

    while (1) {
        DWORD availableSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &availableSize)) 
        {
            fprintf(stderr, "WinHttpQueryDataAvailable failed\n");
            free(buffer);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hInternet);
            return NULL;
        }

        fprintf(stderr, "Available size: %lu bytes\n", availableSize);

        if (availableSize == 0) {
            break;  // No more data
        }

        DWORD bytesToRead = min(availableSize, sizeof(tempBuffer));
        DWORD bytesRead = 0;
        
        // Zero out the temp buffer before reading
        memset(tempBuffer, 0, sizeof(tempBuffer));
        
        fprintf(stderr, "Attempting to read %lu bytes\n", bytesToRead);
        
        if (!WinHttpReadData(hRequest, tempBuffer, bytesToRead, &bytesRead)) {
            fprintf(stderr, "WinHttpReadData failed\n");
            free(buffer);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hInternet);
            return NULL;
        }

        fprintf(stderr, "Actually read %lu bytes (total so far: %lu)\n", bytesRead, totalSize);
        fprintf(stderr, "Data read: '");
        for (DWORD i = 0; i < bytesRead && i < 100; i++) {
            if (isprint(tempBuffer[i])) {
                fprintf(stderr, "%c", tempBuffer[i]);
            } else {
                fprintf(stderr, "\\x%02x", (unsigned char)tempBuffer[i]);
            }
        }
        fprintf(stderr, "'\n");

        if (bytesRead == 0) {
            break;  // No more data
        }

        // Verify we have enough space
        if (totalSize + bytesRead >= bufferSize) {
            DWORD newSize = totalSize + bytesRead + 4096;  // Grow by 4KB chunks
            fprintf(stderr, "Growing buffer from %lu to %lu bytes\n", bufferSize, newSize);
            
            char* newBuffer = (char*)realloc(buffer, newSize);
            if (!newBuffer) {
                fprintf(stderr, "Memory allocation failed when trying to grow to %lu bytes\n", newSize);
                free(buffer);
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hInternet);
                return NULL;
            }
            buffer = newBuffer;
            bufferSize = newSize;
            fprintf(stderr, "Buffer successfully grown, new address: %p\n", (void*)buffer);
        }

        fprintf(stderr, "Copying %lu bytes to position %lu (buffer: %p)\n", 
                bytesRead, totalSize, (void*)(buffer + totalSize));
                
        // Verify our pointers before copy
        if (!buffer || totalSize + bytesRead > bufferSize) {
            fprintf(stderr, "Buffer validation failed before copy\n");
            free(buffer);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hInternet);
            return NULL;
        }

        memcpy(buffer + totalSize, tempBuffer, bytesRead);
        totalSize += bytesRead;
    }

    // Trim buffer to exact size needed
    if (buffer && totalSize > 0) {
        char* finalBuffer = (char*)realloc(buffer, totalSize + 1);
        if (finalBuffer) {
            buffer = finalBuffer;
        }
        buffer[totalSize] = '\0';  // Null terminate the string
        fprintf(stderr, "Successfully read %lu bytes\n", totalSize);
    } else {
        fprintf(stderr, "No data was read\n");
        free(buffer);
        buffer = NULL;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hInternet);

    return buffer;
}

int main() {
    // Test data
    const char* test_license = "{\"key\":\"TEST-123-456\",\"expires_at\":1735689600,\"features\":[\"basic\",\"pro\"]}";
    const char* content_type = "application/json";

    // Convert test data to wide string for path
    wchar_t verify_path[256] = L"/verify";

    // Use port 8080 for local development
    LPSTR response = make_http_request(L"localhost", 8080, verify_path, 0, content_type, test_license);

    if (response) {
        printf("License verification response: %s\n", response);

        // Parse the JSON response
        cJSON *json = cJSON_Parse(response);
        if (json == NULL) {
            fprintf(stderr, "Error parsing JSON response\n");
        } else {
            cJSON *valid = cJSON_GetObjectItemCaseSensitive(json, "valid");
            if (cJSON_IsBool(valid)) {
                if (cJSON_IsTrue(valid)) {
                    printf("License is valid!\n");
                } else {
                    printf("License is invalid!\n");
                }
            }

            cJSON *features = cJSON_GetObjectItemCaseSensitive(json, "features");
            if (cJSON_IsArray(features)) {
                printf("Features:\n");
                cJSON *feature;
                cJSON_ArrayForEach(feature, features) {
                    if (cJSON_IsString(feature)) {
                        printf(" - %s\n", feature->valuestring);
                    }
                }
            }

            cJSON_Delete(json);
        }

        free(response);
    } else {
        printf("Failed to verify license\n");
    }

    return 0;
}
