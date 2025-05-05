#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <mysql.h>
#include <curl/curl.h>

// UDF 구조체 정의
typedef struct UDF_INIT {
    bool maybe_null;           // 함수가 NULL을 반환할 수 있는지 여부
    unsigned int decimals;     // 소수점 이하 자릿수
    unsigned long max_length;  // 결과 최대 길이
    char *ptr;                 // 상태 저장용 포인터 (필수)
    bool const_item;           // 상수 여부
    void *extension;           // 확장용 포인터
} UDF_INIT;

typedef struct UDF_ARGS {
    unsigned int arg_count;     // 인자 개수
    enum Item_result *arg_type; // 인자 타입 배열
    char **args;                // 인자 값 배열
    unsigned long *lengths;     // 인자 길이 배열
    char *maybe_null;           // 인자가 NULL일 수 있는지 여부 배열
} UDF_ARGS;

// 수신 데이터를 저장할 구조체
struct MemoryStruct {
    char *memory;   // 수신된 데이터 저장 버퍼
    size_t size;    // 수신된 데이터 크기
};

// 전역 초기화 플래그 - curl_global_init 호출 추적
static bool curl_global_initialized = false;

// libcurl 수신 콜백
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        return 0; // 메모리 부족
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0; // 문자열 종료

    return realsize;
}

// 문자열을 JSON-safe하게 escape
static char* escape_json_string(const char *input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    size_t buffer_size = len * 6 + 1;
    char *escaped = malloc(buffer_size);
    if (!escaped) return NULL;

    char *p = escaped;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c == '\"') { *p++ = '\\'; *p++ = '\"'; }
        else if (c == '\\') { *p++ = '\\'; *p++ = '\\'; }
        else if (c == '\b') { *p++ = '\\'; *p++ = 'b'; }
        else if (c == '\f') { *p++ = '\\'; *p++ = 'f'; }
        else if (c == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else if (c == '\r') { *p++ = '\\'; *p++ = 'r'; }
        else if (c == '\t') { *p++ = '\\'; *p++ = 't'; }
        else if (c < 0x20 || c > 0x7F) {
            sprintf(p, "\\u%04X", c);
            p += 6;
        } else {
            *p++ = c;
        }
    }
    *p = '\0';
    return escaped;
}

// JSON 문자열 생성
static char* create_json(int arg_count, char **args, unsigned long *lengths) {
    if (arg_count < 1) return NULL;

    int num_pairs = (arg_count - 1) / 2;
    size_t buffer_size = 1024;
    char *json = malloc(buffer_size);
    if (!json) return NULL;

    strcpy(json, "{");

    for (int i = 0; i < num_pairs; i++) {
        char *key_raw = args[1 + i * 2];
        char *value_raw = args[1 + i * 2 + 1];

        // NULL 값 처리
        if (!key_raw || !value_raw) {
            continue;
        }

        // 메모리에 올바르게 저장하기 위해 문자열 복사
        char *key_temp = strndup(key_raw, lengths[1 + i * 2]);
        char *value_temp = strndup(value_raw, lengths[1 + i * 2 + 1]);

        char *key = escape_json_string(key_temp);
        char *value = escape_json_string(value_temp);

        free(key_temp);
        free(value_temp);

        if (!key || !value) {
            free(json);
            free(key);
            free(value);
            return NULL;
        }

        size_t needed = strlen(json) + strlen(key) + strlen(value) + 6;
        if (needed > buffer_size) {
            buffer_size = needed * 2;  // 크기 더 넉넉히 확보
            char *new_json = realloc(json, buffer_size);
            if (!new_json) {
                free(json);
                free(key);
                free(value);
                return NULL;
            }
            json = new_json;
        }

        strcat(json, "\"");
        strcat(json, key);
        strcat(json, "\":\"");
        strcat(json, value);
        strcat(json, "\"");

        if (i < num_pairs - 1) {
            strcat(json, ",");
        }

        free(key);
        free(value);
    }

    strcat(json, "}");
    return json;
}

// HTTP POST 요청 함수
static struct MemoryStruct send_post_request(const char *url, const char *json_data) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    struct MemoryStruct chunk = {0};
    long http_code = 0;

    chunk.memory = malloc(1);
    if (!chunk.memory) {
        return chunk;
    }
    chunk.size = 0;
    chunk.memory[0] = '\0';

    curl = curl_easy_init();
    if (!curl) {
        free(chunk.memory);
        chunk.memory = NULL;
        return chunk;
    }

    char content_length_header[64];
    snprintf(content_length_header, sizeof(content_length_header), "Content-Length: %zu", strlen(json_data));

    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, content_length_header);
    headers = curl_slist_append(headers, "Expect:");  // Disable "Expect: 100-continue"

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(json_data));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);  // 30초 타임아웃 (3600은 너무 길다)
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "identity");

    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK || http_code < 200 || http_code >= 300) {
        if (chunk.memory) {
            free(chunk.memory);
            chunk.memory = NULL;
        }
        chunk.size = 0;
    }

    return chunk;
}

// UDF 초기화 함수
bool http_post_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
    // curl_global_init은 한 번만 호출
    if (!curl_global_initialized) {
        if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
            strcpy(message, "curl_global_init() failed.");
            return true;
        }
        curl_global_initialized = true;
    }

    if (args->arg_count < 3 || (args->arg_count - 1) % 2 != 0) {
        strcpy(message, "http_post() requires at least 3 arguments: URL followed by key-value pairs.");
        return true;
    }

    initid->maybe_null = true;
    initid->max_length = 1024 * 1024;  // 넉넉하게 잡아줌
    initid->const_item = false;
    initid->ptr = NULL;

    return false;
}

// UDF 메인 함수
char* http_post(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error) {
    if (args->arg_count < 3) {
        *is_null = 1;
        return NULL;
    }

    char *url = args->args[0];
    if (!url) {
        *is_null = 1;
        return NULL;
    }

    // URL 문자열을 NUL 종료되도록 복사
    char *url_copy = strndup(url, args->lengths[0]);
    if (!url_copy) {
        *error = 1;
        return NULL;
    }

    char *json_data = create_json(args->arg_count, args->args, args->lengths);
    if (!json_data) {
        free(url_copy);
        *error = 1;
        return NULL;
    }

    struct MemoryStruct response = send_post_request(url_copy, json_data);
    free(url_copy);
    free(json_data);

    if (!response.memory || response.size == 0) {
        *is_null = 1;
        return NULL;
    }

    // 이전 결과 메모리 해제
    if (initid->ptr) {
        free(initid->ptr);
    }

    // 결과를 initid->ptr에 저장
    initid->ptr = response.memory;

    // 길이를 반환
    *length = response.size;

    return initid->ptr;
}

// UDF 종료 함수
void http_post_deinit(UDF_INIT *initid) {
    if (initid->ptr) {
        free(initid->ptr);
        initid->ptr = NULL;
    }
}