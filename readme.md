# 공통
- [POSTMAN 콜렉션](https://api.postman.com/collections/3330654-8ef262ef-5dd4-4619-9535-96db3d4ab99e?access_key=PMAT-01JTEZSH01QF1J248SR6ZWF49D)
  - https://api.postman.com/collections/3330654-8ef262ef-5dd4-4619-9535-96db3d4ab99e?access_key=PMAT-01JTEZSH01QF1J248SR6ZWF49D
  - POSTMAN에 추가하는 방법
    - ![img](https://i.postimg.cc/44hdnX3x/OnPaste.png)

# 커밋 1 : 프로젝트 세팅(Spring Boot, Kotlin, Spring AI)
- [COMMIT](https://github.com/jhs512/mariadb-rag-2025-05-05/commit/b948a8b)

## build.gradle.kts
```kotlin
plugins {
  kotlin("jvm") version "1.9.25"
  kotlin("plugin.spring") version "1.9.25"
  id("org.springframework.boot") version "3.4.5"
  id("io.spring.dependency-management") version "1.1.7"
}

group = "com"
version = "0.0.1-SNAPSHOT"

java {
  toolchain {
    languageVersion = JavaLanguageVersion.of(21)
  }
}

repositories {
  mavenCentral()
}

extra["springAiVersion"] = "1.0.0-M8"

dependencies {
  implementation("org.springframework.boot:spring-boot-starter-web")
  implementation("com.fasterxml.jackson.module:jackson-module-kotlin")
  implementation("org.jetbrains.kotlin:kotlin-reflect")
  implementation("org.springframework.ai:spring-ai-starter-model-transformers")
  developmentOnly("org.springframework.boot:spring-boot-devtools")
  testImplementation("org.springframework.boot:spring-boot-starter-test")
  testImplementation("org.jetbrains.kotlin:kotlin-test-junit5")
  testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

dependencyManagement {
  imports {
    mavenBom("org.springframework.ai:spring-ai-bom:${property("springAiVersion")}")
  }
}

kotlin {
  compilerOptions {
    freeCompilerArgs.addAll("-Xjsr305=strict")
  }
}

tasks.withType<Test> {
  useJUnitPlatform()
}
```

# 커밋 2 : Spring AI 의존성 추가, paraphrase-multilingual-MiniLM-L12-v2-onnx 모델 빈을 통한 임베딩기능 구현
- [COMMIT](https://github.com/jhs512/mariadb-rag-2025-05-05/commit/9cb2421)

## src/main/resources/application.yml
```yaml
spring:
  application:
    name: back
  ai:
    embedding:
      transformer:
        onnx:
          modelUri: https://huggingface.co/onnx-models/paraphrase-multilingual-MiniLM-L12-v2-onnx/resolve/main/model.onnx
          modelOutputName: token_embeddings
        tokenizer:
          uri: https://huggingface.co/onnx-models/paraphrase-multilingual-MiniLM-L12-v2-onnx/resolve/main/tokenizer.json
```

## src/main/kotlin/com.back.domain.base.ai.controller.ApiV1VectorEmbedController.kt
```kotlin
package com.back.domain.base.ai.controller

import org.springframework.ai.embedding.EmbeddingModel
import org.springframework.web.bind.annotation.*

@RestController
@RequestMapping("/api/v1/base/ai/vectorEmbed")
class ApiV1BaseController(
  val embeddingModel: EmbeddingModel
) {
  @GetMapping("/embed")
  fun embed(text: String): FloatArray {
    return embeddingModel.embed(text)
  }


  data class BaseEmbedReqBody(
    val text: String
  )

  @PostMapping("/embed")
  fun embed(@RequestBody reqBody: BaseEmbedReqBody): FloatArray {
    return embeddingModel.embed(reqBody.text)
  }
}
```

## HTTP 요청 테스트
- 임베딩/GET 임베드
  - ![img](https://i.postimg.cc/nc2thWqL/image.png)
- 임베딩/POST 임베드
  - ![img](https://i.postimg.cc/g0X94kQw/image.png)

# 커밋 3 : 임베딩에 캐시적용
- [COMMIT](https://github.com/jhs512/mariadb-rag-2025-05-05/commit/4a06528)

## src/main/kotlin/com.back.domain.base.ai.controller.ApiV1VectorEmbedController.kt
```kotlin
package com.back.domain.base.ai.controller

import org.springframework.ai.embedding.EmbeddingModel
import org.springframework.cache.annotation.Cacheable
import org.springframework.web.bind.annotation.*

@RestController
@RequestMapping("/api/v1/base/ai/vectorEmbed")
class ApiV1BaseController(
  val embeddingModel: EmbeddingModel
) {
  @GetMapping("/embed")
  @Cacheable(value = ["embedding"], key = "#text") // 캐시 사용
  fun embed(text: String): FloatArray {
    return embeddingModel.embed(text)
  }


  data class BaseEmbedReqBody(
    val text: String
  )

  @PostMapping("/embed")
  @Cacheable(value = ["embedding"], key = "#reqBody.text") // 캐시 사용
  fun embed(@RequestBody reqBody: BaseEmbedReqBody): FloatArray {
    return embeddingModel.embed(reqBody.text)
  }
}
```

## src/main/kotlin/com.back.BackApplication.kt
```kotlin
package com.back

import org.springframework.boot.autoconfigure.SpringBootApplication
import org.springframework.boot.runApplication
import org.springframework.cache.annotation.EnableCaching

@SpringBootApplication
@EnableCaching // 캐시 사용
class BackApplication

fun main(args: Array<String>) {
  runApplication<BackApplication>(*args)
}
```

# 커밋 4 : http_post UDF가 내장된 MariaDB 띄우고 http_post 테스트
- [COMMIT](https://github.com/jhs512/mariadb-rag-2025-05-05/commit/0b95c1b)

## docker-compose.yml
```yaml
version: "3.8"

services:
  mariadb_1:
    build:
      context: infra/mariadb_1
    restart: unless-stopped
    environment:
      MARIADB_ROOT_PASSWORD: 1234
      MARIADB_DATABASE: db_dev
      MARIADB_USER: lldj
      MARIADB_PASSWORD: 123414
    ports:
      - "3306:3306"
    volumes:
      - mariadb_1_data:/var/lib/mysql # 데이터 영속화 볼륨
volumes:
  mariadb_1_data:
```

## infra/mariadb_1/Dockerfile
```dockerfile
# 1단계: 빌드 스테이지
FROM mariadb:11 AS builder

RUN apt-get update && \
    apt-get install -y \
        gcc \
        make \
        libcurl4-openssl-dev \
        libmariadb-dev

COPY http_post.c /usr/src/http_post.c

RUN gcc -std=c99 -Wall \
    -I/usr/include/mariadb \
    -fPIC -shared \
    /usr/src/http_post.c \
    -o /usr/lib/mysql/plugin/http_post.so \
    -lcurl

# 2단계: 최종 스테이지
FROM mariadb:11

RUN apt-get update && \
    apt-get install -y \
        mariadb-plugin-mroonga \
        libcurl4-openssl-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY --from=builder /usr/lib/mysql/plugin/http_post.so /usr/lib/mysql/plugin/http_post.so

COPY init.sql /docker-entrypoint-initdb.d/
```

## infra/mariadb_1/http_post.c
```c
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
```

## infra/mariadb_1/init.sql
```sql
-- http_post UDF 등록
CREATE FUNCTION http_post RETURNS STRING SONAME 'http_post.so';
```

## docker-compose.yml 실행하여 http_post UDF가 내장된 MariaDB 컨테이너 실행
- ![img](https://i.postimg.cc/QCjzkDvw/image.png)

## http_post 함수 테스트
```sql
SELECT http_post("http://host.docker.internal:8080/api/v1/base/ai/vectorEmbed/embed", "text", "안녕");
```
- ![img](https://i.postimg.cc/ncc5XH4Q/image.png)

# 커밋 5 : 샘플 데이터 추가

## 데이터 생성
```sql
DROP DATABASE IF EXISTS `db_dev`;

CREATE DATABASE `db_dev`;

-- 'post' 테이블 생성
CREATE TABLE post (
                    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY, -- 자동 증가하는 기본 키
                    createDate DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, -- 생성 날짜, 기본값은 현재 시간
                    modifyDate DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, -- 수정 날짜, 기본값은 현재 시간이며, 레코드 수정 시 자동 갱신
                    title VARCHAR(255) NOT NULL, -- 제목, 최대 255자
                    content LONGTEXT NOT NULL -- 내용, 긴 텍스트 저장
);

/*
카이론 블레이크 / 남성 / 용인

린 시엔화 / 여성 / 인간

아론 실버문 / 남성 / 엘프

에리사 문브리즈 / 여성 / 엘프

드로그 바위주먹 / 남성 / 드워프

브린다 섀도우핸드 / 여성 / 드워프

그로크 스톤브루 / 남성 / 오크

모르가나 다크위버 / 여성 / 언데드

펠릭스 골드리프 / 남성 / 인간

세레나 스타글로우 / 여성 / 요정

로한 블러드후프 / 남성 / 오크

이사벨 문하트 / 여성 / 인간

탈리온 화이트파이어 / 남성 / 드래곤본

네이라 리프위스퍼 / 여성 / 요정

울프강 그룬드블러드 / 남성 / 오크

자이라 블랙윙 / 여성 / 악마

마일로 스톤포지 / 남성 / 드워프

오펠리아 실버레이크 / 여성 / 인간

세바스찬 그림토크 / 남성 / 언데드

미라 벨라하트 / 여성 / 인간

발록 쉐도우블레이드 / 남성 / 악마

일린다 문패스 / 여성 / 엘프

볼티모어 아이언스킨 / 남성 / 드워프

아벨린 골드리프 / 여성 / 드래곤본

고로크 블러드케인 / 남성 / 오크

제네비브 트윙클스타 / 여성 / 페어리

티볼드 하이랜드 / 남성 / 인간

세레일 스카이라이트 / 여성 / 엘프

도미닉 럼블록 / 남성 / 드워프

릴리아 블로섬하트 / 여성 / 인간

하르곤 넥로스 / 남성 / 언데드

에벨린 문글로우 / 여성 / 엘프

로크 해머폴 / 남성 / 드워프

실비아 하트브리즈 / 여성 / 인간

카르마 스톤아이 / 여성 / 고블린

이안 나이트폴 / 남성 / 인간

엘로디 골드위버 / 여성 / 페어리

그루바크 아이언쉴드 / 남성 / 드워프

네노라 미스트쉐이드 / 여성 / 언데드

벨렌드리오 스타시커 / 남성 / 엘프

자레드 블레이드리퍼 / 남성 / 악마

루나 문샤인 / 여성 / 요정

스톤볼트 크러셔 / 남성 / 오크

일라이자 실버송 / 여성 / 드래곤본

트레반 코발트 / 남성 / 정령

미에르디스 블러드레이크 / 여성 / 언데드

라일락 페더윈드 / 여성 / 페어리

본자리크 파이어토스트 / 남성 / 드래곤본

알렉산더 딥워터 / 남성 / 인어

힐다 아이언브레이스 / 여성 / 드워프

에드윈 블랙펄 / 남성 / 인간

시에라 썬더콜 / 여성 / 정령

매그너스 헬하운드 / 남성 / 악마

네로 브라이트쉴드 / 남성 / 인간

티아라 스윗브리즈 / 여성 / 요정

듀란 록본 / 남성 / 드래곤본

세릴 마운틴브루 / 여성 / 드워프

카일로 섀도우폭스 / 남성 / 늑대인간

네리아 윈터블룸 / 여성 / 엘프

고라도스 언더크러쉬 / 남성 / 오크

아이리스 문게이저 / 여성 / 인간

제드릭 다크슬레이어 / 남성 / 언데드

벨타르 썬세이버 / 남성 / 인간

오로라 스타브리즈 / 여성 / 페어리

듀크쉐도 크리드 / 남성 / 악마

세피라 글림머링 / 여성 / 엘프

베른 아이언피스트 / 남성 / 드워프

레이나 블러드문 / 여성 / 반인반수

라그나 로스트혼 / 남성 / 드래곤본

엠마 리프가든 / 여성 / 인간

타이론 그림스토커 / 남성 / 언데드

온딜리아 실버스피어 / 여성 / 엘프

바루스 썬파이어 / 남성 / 정령

마리엘루 나이트위스퍼 / 여성 / 엘프

드라칸 데스브링어 / 남성 / 악마

카산드라 문플릿 / 여성 / 인간

옥타비우스 블라이트 / 남성 / 언데드

셀레스티아 스타퀴든 / 여성 / 페어리

모르도크 블러드해머 / 남성 / 드워프

일리아나 헬로즈 / 여성 / 악마

제노스 스톰크래프트 / 남성 / 인간

에반젤린 소울라이트 / 여성 / 엘프

그린스킨 클로후프 / 남성 / 오크

로살린 무지개꽃 / 여성 / 요정

드레이크 하트오브레드 / 남성 / 드래곤본

이사도라 실크윈드 / 여성 / 인간

번스 스톤하트 / 남성 / 드워프

리비아 섀도우릴 / 여성 / 엘프

울릭 데들리그로브 / 남성 / 언데드

카밀리아 문차임 / 여성 / 페어리

발도르 아이언울프 / 남성 / 늑대인간

나탈리아 소프트리프 / 여성 / 엘프

미홈 블러드페인 / 여성 / 오크

레오나 데스가디언 / 여성 / 언데드

콜터 스카이퓨리 / 남성 / 인간

세라피나 골든하트 / 여성 / 드래곤본

구스타프 암흑의손 / 남성 / 악마

에밀리아 문리버 / 여성 / 인간

드락 마운틴블레이드 / 남성 / 드워프

아델린 새턴위스퍼 / 여성 / 요정
*/

INSERT INTO post
SET title = '카이론 블레이크 / 남성 / 용인',
content = TRIM(BOTH '\r\n' FROM "
1살: 화산 분화구 가장자리의 달빛 아래, 두꺼운 알껍질을 깨고 세상에 첫 숨을 들이마셨다. 갓 깨어난 그는 아직 날개도 작고 비늘도 연약했지만, 호기심 어린 눈빛으로 주변을 살폈다.

5살: 호수 마을의 평민 가정에 입양되어 인간 부모와 첫 걸음을 떼었다. 비늘이 햇빛에 반짝일 때마다 마을 사람들은 두려움 반, 경이로움 반으로 바라보았고, 카이론은 따뜻한 사랑 속에서 자랐다.

12살: 마법 학교 ‘에스탈라’에 입학해 기초 마법부터 드래곤언어 연구까지 배우며, 자신의 태생이 남다르다는 사실을 조금씩 깨달았다. 친구들과 함께 호수 위에서 비늘 반짝이는 창공을 날며 자유를 만끽했다.

18살: 졸업식 날, 급작스러운 마을 습격이 벌어졌다. 어둠의 무도가들과 맞서 싸우던 중, 숨겨진 용의 화염을 발현해 위기를 막았다. 이때 그는 자신의 피 속에 흐르는 고대 용족의 힘을 진정으로 의식하게 되었다.

25살: 북쪽 사막지를 순례하다 전설의 흑룡 ‘칼렌스’와 조우했다. 칼렌스는 카이론의 용인으로서의 자질을 인정하고, 비늘에 은은한 검은 빛을 더해 주며 연합을 맺었다. 이로써 그는 ‘검은 용의 후예’라는 칭호를 얻었다.

30살: 동료 마법사, 검사, 주술사들과 ‘황혼의 맹약’ 모험단을 결성해 대륙 각지를 누볐다. 고대 유적을 탐사하고, 봉인된 마나 정수를 해방하며 이름을 알렸다.

40살: 대륙을 뒤흔든 ‘밤의 제왕’과의 대전쟁에 참전하여 검은 용 칼렌스와 합동 공격을 펼쳤다. 용의 화염과 카이론의 마법이 융합된 일격은 전쟁의 행방을 바꾸는 결정적 전투가 되었다.

50살: 왕국의 흑룡기사단 단장으로 임명되어 평화를 수호하는 임무를 맡았다. 그간 쌓은 지혜와 용맹으로 수많은 후배 용인들과 인간 병사들을 가르치고 이끌었다.

60살: 전장의 상처와 마법 사용의 피로로 잠시 은둔을 결심, 고향 호수 마을 근처 숲속 오두막에서 심신을 가다듬었다. 이 시기 그는 고대 비룡 문헌을 집필하며 학자로서 면모를 드러냈다.

75살: 제자들을 위해 ‘검은 용의 전서’를 발간하고, 세계 곳곳을 순회하며 강연을 열었다. 이 강연은 용인과 인간의 이해를 돕는 다리 역할을 했고, 종족 간 평화 증진에 기여했다.

90살: 은빛이 감도는 비늘과 백발의 모습으로 ‘전설의 장수’가 되었다. 그의 이름은 노래와 이야기에 남아, 어린아이들조차도 용맹과 지혜의 상징으로 기억했다.

100살: 호수 마을의 고목 아래에 누워 마지막 숨을 내쉬기 전, 자신이 이룬 모든 여정을 되돌아보았다. 알에서 깨어난 그 순간부터 시작된 삶은 고난과 영광, 사랑과 희생으로 가득했다. 세상을 떠나기 직전 카이론은 검은 용 칼렌스에게 마지막 비늘을 나누어 주었고, 그의 영혼은 용의 화염에 녹아 새로운 전설로 남았다.
");

INSERT INTO post
SET title = '린 시엔화 / 여성 / 인간',
content = TRIM(BOTH '\r\n' FROM "
1살: 안개 낀 새벽, 강호의 작은 산골 마을에서 태어났다. 주위 사람들은 그의 울음소리에 귀 기울이며 이 아이가 남다른 운명을 가졌음을 직감했다. 부잣집 자제였지만, 맑고 호기심 어린 눈동자는 언제나 세상을 향해 열린 문 같았다.

5살: 마을의 작은 문파인 천무문에 들어가 문하생이 되었다. 어린 나이에 검술과 기초 내공 수련을 익히며, 손바닥만 한 목도리 검을 휘두르는 법을 먼저 배워 놀라운 재능을 보였다. 매일 새벽부터 해 질 녘까지 연무장에 나와 스승의 가르침을 따라 단련하며, 결코 지치지 않는 집중력을 키웠다.

12살: 강호를 떠돌며 여러 문파 무인이 되길 원했으나, 천무문 문주에게 사부로 인정받아 정식 제자가 되었다. 이때부터 본격적인 인생의 전환점이 시작되었다. 청룡검법을 배워 스스로 검격을 일으키는 법을 깨달으며, 첫 안개 검무를 완성해 보였다.

18살: 첫 세상 밖 여정으로 북방 사막을 건너 가혹한 역병군과 맞섰다. 극한의 환경 속에서 내공을 한층 끌어올려, 사막의 폭풍을 막아내는 기이한 기운을 몸에 익혔다. 이때 만난 사막 유목민이 전해준 수련 비법은 훗날 그녀의 최대 무공 비밀이 되었다.

24살: 천무문을 떠나 운남의 구름협곡으로 향했다. 그곳에서 운룡파의 마지막 후예와 검술 난투를 벌여 승리했고, 칼날이 부서진 후에도 변치 않는 정신력으로 스승과 동료들의 존경을 한몸에 받았다.

30살: 강호 최고의 무림맹주를 호위하는 호위단장에 임명되었다. 무림맹 내부의 암투를 중재하며 평화를 유지했으며, 각 문파의 균형을 잡는 데 기여했다. 그녀의 공정함은 적과 우호 세력을 가리지 않고 칭송을 받았다.

38살: 은밀한 암살자 집단 ‘흑풍회’의 대공습이 발생했을 때, 흑풍회 수장과 일대일 결투를 벌여 승리했다. 이 승리로 인해 그녀의 이름은 악명 높은 암살자들 사이에서도 공포의 대상이 되었고, 오히려 흑풍회의 일부 암살자들이 그녀에게 귀의했다.

48살: 자신만의 문파를 창설해 ‘무아문’이라 명명하고, 무아무념의 경지를 구하는 수행과 검술을 접목한 새로운 무공을 완성했다. 이후 많은 문사들이 그녀의 문하로 몰려들어, 제법 빠른 시간 안에 무아문은 강호에 새로운 바람을 일으켰다.

58살: 한때 죽음의 위기에 내몰린 제자를 구하기 위해 자신의 생명력을 일시적으로 내공으로 전환해 큰 대가를 치렀다. 이후 몇 개월간 산속 암자에서 수행하며 몸과 마음을 회복했으며, 이 사건으로 제자와의 유대가 더욱 두터워졌다.

70살: 무아문의 경지를 다룬 책 『무아검경』을 집필해 문학과 무공 양면에서 기념비적 업적을 남겼다. 이 책은 후대 무인들에게 영감을 주는 고전으로 자리잡아, 강호 곳곳에서 필독서로 여겨졌다.

85살: 산속 작은 암자에서 노장으로서 제자들을 가르쳤다. 젊은 무인들에게 진정한 무공의 의미와 강호의 마음가짐을 전수했으며, 때로는 은둔자처럼 생활하면서도 방문객들을 환대했다.

100살: 마지막 청룡검 시연을 끝내고, 해 질 녘 검을 땅에 꽂은 채 깊은 명상 상태로 눈을 감았다. 그녀가 남긴 업보와 가르침은 강호 도처에 퍼져, 수백 년 동안 전설로 전해졌다.
");

INSERT INTO post
SET title = '아론 실버문 / 남성 / 엘프',
content = TRIM(BOTH '\r\n' FROM "
1살: 은빛 이슬이 맺힌 새벽, 은나무 사이로 비치는 달빛 아래 아론 실버문은 조용히 숨을 틔웠다. 순백의 은발과 호수처럼 맑은 눈동자를 가진 그는 태어나자마자 엘프 대가족의 기대를 받았다. 신비로운 기운 속에서 그의 울음소리는 마치 숲의 노래처럼 퍼졌다.

5살: 실버문 대도서관 옆 아카데미에 입학하여 고대 룬 문자부터 자연 속성 마법까지 기초를 연마했다. 짧고 가는 손가락 끝에서 작은 마법 구슬을 만들어내자 교사들은 놀라워하며 그의 재능을 칭송했다. 이른 새벽부터 별 아래 예습하며 엘프의 자긍심을 키웠다.

12살: 리프브리즈 축제에서 연출된 대나무 연극 ‘숲의 탄생’ 주인공으로 발탁되어 첫 무대에 올랐다. 무대 위에서 자연 마나를 눈빛만으로 조종하는 장면을 선보이며 관중의 기립박수를 이끌었다. 이때 그는 엘프 예술 마법과 전투 마법을 조화하는 기틀을 잡았다.

18살: 북쪽 폭풍 해안선으로 떠난 첫 성인 의식 여정에서 파도 정령과의 교감에 성공했다. 거친 파도를 가르고 달빛 수면 위를 활공하며 대지와 바다, 하늘을 잇는 균형의 중요성을 깨달았다. 그 경험은 그의 마법관을 더욱 넓게 열어 주었다.

26살: 암흑의 연합 '그림자 칼날'이 대륙을 위협할 때, 아론은 빛의 의회 파견단으로 선발되어 결투를 벌였다. 어둠 마력을 순백의 은빛 마나로 정화하며 단장과 일대일 대결 끝에 승리, 연합을 해체하는 결정적 계기를 마련했다.

34살: 실버문 왕국 마법 원로회에 가장 젊은 자문관으로 임명되어 법과 예술, 군사 전략을 아우르는 자문을 담당했다. 왕실 서고에서 고대 주문서를 연구하며 엘프 사회의 지속가능한 번영을 위한 정책을 제안했다.

45살: 다크엘프가 이끄는 남부 침공군이 성벽을 무너뜨리고 실버문을 점령하려 할 때, 그는 숲의 뿌리 마법을 발동시켜 거대한 나무벽을 일으켰다. 전투는 치열했지만, 그의 용맹과 마법이 어울려 왕국을 구원했다.

55살: 성대한 전투 후 내면의 평화를 찾고자 고대 사원으로 은둔, 수십 년간 명상을 통해 자신의 영혼을 다스렸다. 그 과정에서 잊힌 치유 마법을 재발견하여 부상당한 대륙 전사들을 돕는 비밀 의식을 재개했다.

68살: 세계 각지에서 모은 마법 기록과 전투 기술을 집대성한 '엘더그림 내역 보관록'을 완성해 대륙 대학과 마법사 아카데미에 기증했다. 이 책은 수세기 동안 표준 교과서로 사용되었다.

80살: 실버문 왕국의 황금시대를 기념하는 대축전에 참석해 왕실과 평민 사이의 조화를 기리기 위해 고대 신전 복원 프로젝트를 총괄 지휘했다. 완공된 신전은 대륙 예술의 걸작으로 평가받는다.

92살: 후계 엘프 사절단을 이끌고 인간 제국, 드워프 연방, 요정 숲과 문화 교류 협정을 체결하며 평화의 다리를 놓았다. 그의 중재 덕분에 종족 간 상호 이해와 협력이 새로운 국면을 맞았다.

100살: 생명나무 성역 아래, 백발이 된 그는 마지막 주문 '영원의 노래'를 읊조리며 은은한 빛으로 승화했다. 그의 영혼은 숲의 기운이 되어 영원히 대지를 지키는 전설이 되었다.
");

INSERT INTO post
SET title = '에리사 문브리즈 / 여성 / 엘프',
content = TRIM(BOTH '\r\n' FROM "
1살: 달빛이 은은히 빛나는 요정의 숲 속 작은 오두막에서 태어났다. 마치 꽃잎 속의 이슬처럼 맑은 울음소리는 숲의 정령마저 멈춰 서게 했고, 부모인 숲의 수호 요정 부부는 그녀에게 ‘문브리즈’라는 이름을 지어 주었다.

5살: 숲의 정령학교에 들어가 자연의 언어를 배우기 시작했다. 나뭇잎을 말려 그 안에 숨은 별의 기운을 읽어내는 법을 익히자, 주변의 작은 동물들이 그녀를 따르며 놀았다. 이때부터 에리사는 자신이 숲과 하나라는 감각을 느꼈다.

10살: 달빛 무도회에서 처음으로 요정춤을 선보였다. 은빛 글리터가 춤추는 발끝에서 흩날리자, 숲의 고목과 바위마저 생명력을 얻은 듯 반응했다. 이 모습은 대지의 여왕에게까지 전해져 칭송을 받았다.

18살: 성인이 되는 의식으로 숲의 심장이라 불리는 ‘영원의 나무’를 찾아 깊은 동굴 속 마나 샘물에서 시련을 치렀다. 차가운 물살 속에서 호흡을 가다듬으며 자신의 내면에 숨겨진 불안을 극복하고, 요정 마법의 균형을 완성했다.

24살: 숲을 위협하는 어둠의 포자로부터 동료 요정들과 함께 요정의 빛 정수를 추출해 백병전을 이끌었다. 그녀의 손끝에서 뿜어져 나온 순수한 빛의 마법은 포자를 정화하며 승리를 거두었고, 숲의 주민들은 그녀를 ‘빛의 사절’이라 불렀다.

30살: 대륙 각지를 순회하며 인간, 엘프, 드워프, 요정 간의 평화 협정을 중재하는 사절로 임명되었다. 각 종족의 대표들을 만나 대화를 이끌어 내며, 숲과 다른 문명 사이에 두터운 신뢰의 다리를 놓았다.

40살: 숲의 연합의회 의장으로 선출되어 숲의 정책과 수호 마법의 법칙을 수립했다. 에리사의 공정한 판단은 숲의 조화와 번영을 이끌었고, 대지와 대양, 하늘과 불의 정령들이 한자리에 모이는 ‘대정령 축제’를 처음으로 주관했다.

50살: 은빛 마나 결정의 은둔 수행을 위해 고대 요정 사원에 들어가 수년간 명상을 이어갔다. 그 과정에서 ‘달의 속삭임’이라 불리는 고대 주술을 재발견해, 숲의 병든 뿌리들을 치유하는 비법을 완성했다.

60살: 고대 문헌을 바탕으로 ‘숲의 서사시’를 집필해 요정 문학에 획기적인 흐름을 가져왔다. 이 작품은 자연의 기운을 글과 음악으로 조합하는 독창적 형식을 담아 후대 요정들에게 큰 영감을 주었다.

70살: 대륙의 마법학교에서 귀빈 강연을 통해 자연 마법과 예술 마법의 융합을 전파했다. 에리사의 강연은 인간과 엘프 학생들 사이에서 전설로 회자되었고, 수많은 젊은 마법사들이 그녀의 제자가 되기 위해 몰려들었다.

85살: 숲의 평화가 무너지려 할 때, 잃어버린 요정의 불꽃을 되찾기 위해 사막과 화산을 횡단했다. 치열한 여정 끝에 얻은 불꽃을 숲의 나무마다 심어 다시금 생명과 활력을 불어넣었다.

100살: 은빛 안개가 자욱한 요정의 숲 축제에서 마지막 축복의 주문을 읊으며 미소 지었다. 그녀의 목소리는 숲의 바람이 되어 날아가 모든 생명에게 평화를 선사했고, 영원의 나무 아래로 돌아간 에리사 문브리즈는 숲 그 자체로 전설이 되었다.
");

INSERT INTO post
SET title = '드로그 바위주먹 / 남성 / 드워프',
content = TRIM(BOTH '\r\n' FROM "
1살: 깊고 어두운 산맥 아래 바위틈에서 태어났다. 갓 부화한 그의 첫 울음은 동굴 벽에 메아리쳤고, 주변 드워프들은 강인한 기운을 예감했다.

5살: 광산 마을의 채굴 공방에서 자라며 처음으로 작은 곡괭이를 쥐어 보았다. 손에 익은 돌가루와 쇳가루 냄새는 그의 영혼 깊이 각인되었고, 매일 벽을 다듬는 연습을 거듭했다.

12살: 대장장이 길드의 견습생으로 들어가 망치질을 배우기 시작했다. 처음 두드린 쇠덩이를 제 모습으로 다듬어 냈을 때, 스승은 그의 재능을 인정하며 ‘바위주먹’이라는 별명을 붙여 주었다.

18살: 첫 번째 자신만의 도끼를 완성해 마을 축제에서 시연했다. 불길 속에서도 한 치의 오차 없이 금속을 빚어내는 모습에 사람들은 놀라움을 금치 못했다.

25살: 왕국의 군수 공방에 발탁되어 전설의 철광석을 다루기 시작했다. 전투용 갑옷과 전쟁 망치를 주문 제작하며, 전군의 신뢰를 한 몸에 받았다.

30살: 북부 국경 전투에서 포위당한 동료들을 구하기 위해 거대한 전쟁 망치를 휘둘렀다. 바위처럼 단단한 팔뚝과 힘으로 적의 돌격대를 산산이 부숴 놓았다.

40살: 대장간 길드장이 되어 수백 명의 견습생을 이끌며 새로운 기술을 전수했다. 특히 흑요석 합금 기술을 개발해 마을의 부를 크게 끌어올렸다.

55살: 잊혀진 고대 드워프 무기 ‘대지의 망치’를 복원해내며 전설 속 장인이 되었다. 복원된 무기는 왕실 보물고에 봉인되어 후대에도 전해졌다.

80살: 광산 탐사 원정대를 조직해 잔도 아래 숨겨진 보물 광맥을 발견했다. 그의 지도 아래 수천 미터 깊이의 동굴이 안전하게 개척되었다.

100살: 마지막 벼랑 위에서 망치를 땅에 내리찍으며 은퇴를 선언했다. 그의 손끝에서 태어난 수많은 무기들은 여전히 전장을 누비고, 드로그 바위주먹이라는 이름은 드워프 역사에 길이 남았다.
");

INSERT INTO post
SET title = '브린다 섀도우핸드 / 여성 / 드워프',
content = TRIM(BOTH '\r\n' FROM "
1살: 대장간 깊은 동굴 속, 불꽃 튀는 용광로 앞에서 첫 울음을 터뜨렸다. 갓난 아기의 얼굴을 비추는 쇳가루 빛이 비늘처럼 반짝이며, 그녀의 탄생을 더욱 강렬하게 알렸다.

5살: 아버지 곁에서 작은 망치를 잡고 처음으로 금속을 두드려 보았다. 두드릴 때마다 울려 퍼지는 금속 소리에 매료되어, 망치와 대장간의 소리가 인생의 배경음악이 되었다.

12살: 드워프 길드 견습공 시험에 합격해 ‘섀도우핸드’라는 별명을 얻었다. 어둠 속에서도 손놀림이 빛나듯 빠르고 정확해, 동료들은 그녀의 장인 정신에 감탄했다.

18살: 고대 드워프 유적 탐사대에 참여해 함정 해제와 보물 회수 임무를 맡았다. 동굴 내 어둠 속에서도 단검과 망치를 능숙하게 다루며 팀을 안전하게 이끌었다.

25살: 왕실 무기 수리팀장으로 발탁되어 전장에서 부서진 갑주와 무기를 완벽 복원했다. 특히 황금 장식 수리 기술을 완성해 왕가의 큰 신임을 얻었다.

35살: 용암 호수 인근 마을이 마그마 정령에 위협받았을 때, 대장간 불씨를 정령에게 바쳐 위기를 잠재웠다. 이후 그녀는 ‘정령의 불꽃 수호자’로 불리게 되었다.

50살: ‘섀도우핸드 공방’을 설립해 수십 명의 견습생을 교육했다. 그녀의 공방에서 제작된 무기들은 견고함과 예술적 아름다움을 모두 갖춰 명성을 떨쳤다.

65살: 드워프 기술 학회에서 ‘잿빛 철광석 가공법’을 발표해 대장간 생산량을 획기적으로 향상시켰다. 이 연구는 수십 년간 드워프 업계의 표준으로 자리잡았다.

85살: 전설 속 ‘어둠의 장갑’을 복원하기 위해 단독 원정을 떠났다. 수년간의 고행 끝에 장갑을 되살린 뒤, 이를 기반으로 한 방어구 시리즈를 완성했다.

100살: 마지막으로 망치를 내려놓고 은퇴를 선언했다. 그녀가 남긴 수많은 무기와 가공법은 후대 드워프 장인들의 귀중한 유산으로 전해졌다.
");

INSERT INTO post
SET title = '그로크 스톤브루 / 남성 / 오크',
content = TRIM(BOTH '\r\n' FROM "
1살: 붉은 모래 언덕 아래 작은 부족의 전쟁 북소리 속에서 태어났다. 그의 울음은 천둥처럼 우렁찼고, 주술사는 그를 '돌보다 단단한 운명의 아이'라 불렀다.

6살: 첫 전투 훈련에 참여하여 나무 방패를 들고 형들과 겨루기 시작했다. 강인한 체격과 불굴의 투지로 부족 내에서 빠르게 이름을 알렸다.

12살: 부족의 의식 '피의 도약'에 참여해 거대한 맹수를 쓰러뜨리고 전사로서의 인정을 받았다. 그 날 밤, 어른들과 함께 전쟁의 노래를 부르며 자신의 전설을 시작했다.

18살: 오크 연합군에 들어가 이웃 부족과의 분쟁을 중재하며, 힘뿐 아니라 전략과 명예의 가치를 배웠다. 이 무렵 그는 바위술을 익혀 적의 진형을 분쇄하는 기술을 터득했다.

25살: 용의 계곡에서 벌어진 종족 연합 전투에서 최전선에 나서 적의 공성기를 맨몸으로 부수었다. 이 전투로 인해 ‘스톤브루(Stonebrew)’라는 이름을 공식적으로 받게 되었다.

35살: 전사이자 외교관으로서 다른 종족과의 회담에 참여, 거친 말투 속에서도 진심 어린 화합의 의지를 보여 종족 간 긴장을 완화시켰다.

45살: 전쟁을 멈추고 자신의 고향에 대장간과 훈련장을 건설, 젊은 오크들에게 기술과 명예를 가르치기 시작했다. 그로크는 자신만의 규율을 통해 ‘전사이자 스승’으로 거듭났다.

60살: 과거의 적이던 드워프와 협력해 ‘바위의 맹약’을 체결하고, 드워프의 금속과 오크의 전투 기술이 융합된 무기를 개발했다. 이 무기는 후대 전쟁에서 수많은 생명을 구했다.

80살: 산 속 깊은 동굴에서 1년간 고행하며 스스로의 분노와 싸웠다. 이 기간 동안 그는 ‘바위 명상술’을 완성해 후계자들에게 정신 수양의 중요성을 전했다.

100살: 마지막 전투에서 부족을 위해 스스로 함정에 남아 적을 유인하고, 바위를 무너뜨려 모두를 구한 뒤 유언을 남겼다. 그의 무덤은 부족의 바위 언덕 꼭대기에 세워졌으며, 오크들은 매년 그 앞에서 전사의 춤을 추며 그를 기린다.
");

INSERT INTO post
SET title = '모르가나 다크위버 / 여성 / 언데드',
content = TRIM(BOTH '\r\n' FROM "
1살(부활): 폐허가 된 고성 지하에서 흑마법 의식에 의해 부활했다. 그녀의 눈동자는 죽음의 안개처럼 희미하게 빛났고, 망자의 언어를 자연스럽게 구사하기 시작했다.

3살: 네크로노미콘의 파편을 손에 넣고, 처음으로 언데드 하수인을 소환하는 데 성공했다. 마력은 불안정했지만, 놀라운 집중력으로 자신을 통제했다.

10살: 죽음의 숲에서 사는 네크로맨서 집단에 의해 '어둠의 직조자(Darkweaver)'라는 이름을 부여받고, 마력 결계를 엮는 기술을 익혔다. 살아있는 것과 죽은 것을 잇는 고리를 짜는 데 천부적인 재능을 보였다.

18살: 인간 마을을 습격한 언데드 군단을 막기 위해 마법사 연합에 협조했다. 역설적으로 언데드이지만 언데드를 통제할 수 있는 자로서, 균형의 수호자 역할을 수행했다.

25살: 과거의 기억을 더듬어, 자신이 생전 왕국의 왕립 연금술사였음을 밝혀낸다. 그 진실은 그녀의 정체성에 큰 혼란을 주었으나, 이후 죽음을 초월한 존재로서 새로운 길을 걷기로 결심한다.

35살: 사망자의 영혼을 정화시키는 ‘검은 달의 의식’을 완성하고, 저주받은 망자의 도시를 해방시켰다. 이로써 언데드이면서도 구원자의 역할을 하게 된다.

50살: 생명과 죽음의 경계를 잇는 마법 논문 『죽음 이후의 구조』를 집필해 마법사 사회에 큰 파장을 일으켰다. 금서로 지정되었으나, 몰래 연구하는 이들이 생겨났다.

65살: 언데드 군주 ‘자크로스’와의 결전을 통해 스스로 죽음의 영혼을 끌어안고 함께 낙원계로 추방되었다. 이후 그녀는 살아있는 이들의 꿈속에서 나타나 조언을 주는 존재가 되었다는 전설이 퍼졌다.

85살: 다차원의 틈에서 깨어난 그녀는 ‘검은 안개 시대’를 예언하며, 무너진 균형을 되돌리기 위해 다시 모습을 드러냈다. 존재 자체가 전설로서 받아들여졌고, 생자와 사자 모두 그녀의 말을 경청했다.

100살: 마지막으로 '망자의 탑'에서 자신을 봉인하며 세상과 이별했다. 그녀가 남긴 주문과 예언은 미래의 마법사들에게 길잡이가 되었고, 모르가나는 죽음을 넘어선 현자로 기억되었다.
");

INSERT INTO post
SET title = '펠릭스 골드리프 / 남성 / 인간',
content = TRIM(BOTH '\r\n' FROM "
1살: 왕국 수도의 서쪽 구역, 하층민 가정에서 태어났다. 금빛 머리카락이 햇살을 반사해 '골드리프(황금잎)'라는 별명이 생겼다. 가난했지만 웃음 많은 가족과 함께 자라며 희망을 품었다.

6살: 시장에서 잡일을 하며 자란 그는 읽고 쓰는 법을 독학으로 배웠고, 근처 마법사의 낡은 책을 주워들고 꿈을 키우기 시작했다. 친구 대신 책이 친구였고, 호기심이 인생을 움직였다.

12살: 가짜 마법서를 파는 사기꾼을 간파하고 도서관 사서에게 인정을 받아 무료로 도서관을 이용할 수 있는 권한을 얻게 된다. 그곳에서 수많은 전설과 역사, 마법 이론을 익히며 지혜를 쌓았다.

17살: 마법 입문서를 기반으로 기초 마법을 독학하며, 왕국 마법사단 입단 시험에 비정규 출신으로 도전해 합격했다. 그는 ‘거리의 마법사’라는 별명을 얻게 되었다.

23살: 왕국 북부에서 벌어진 고대 괴수 습격 사건 당시, 오직 기지와 응용 마법으로 수많은 병사를 구출하며 일약 유명 인사가 된다. 이후 정식 마법사로서 작위까지 부여받는다.

30살: 귀족 마법사들과의 정치 다툼 속에서 서민 출신이라는 이유로 차별을 겪지만, 연구와 실력으로 인정받아 마법 학회의 고문이 된다. 이 시기 그는 마법의 대중화를 주장하며 논쟁을 일으켰다.

40살: ‘왕국 공용 마도서’ 프로젝트를 주도하며 전국의 마을에 기초 마법 지식을 배포한다. 마법을 귀족의 전유물에서 국민의 힘으로 바꾸고자 했다.

55살: 후학들을 길러내기 위해 ‘골드리프 아카데미’를 설립하고 직접 강의에 나섰다. 그의 강의는 실용성과 창의성을 강조해 수많은 젊은 마법사들에게 영향을 미쳤다.

70살: 대륙 전역의 마법 학회에서 '세기의 마법 철학자'로 추앙받으며, 생애 연구를 담은 『지혜의 잎』을 출간한다. 이 책은 후대에도 마법 윤리와 실용성의 지침서로 여겨진다.

90살: 조용히 학회 고문직을 내려놓고, 왕국 외곽의 작은 마을에서 아이들에게 무료로 마법을 가르치며 노년을 보낸다. 그의 마법은 이제 불꽃이나 얼음이 아닌, 따뜻한 마음이 되었다.

100살: 제자들과 마을 사람들 사이에서 생일 축하를 받으며 마지막 강의를 마친 뒤, 황금빛 나뭇잎이 흩날리는 정원에서 평온히 숨을 거두었다. 그는 '지혜의 잎을 심은 자'로서 전설이 되었다.
");

INSERT INTO post
SET title = '세레나 스타글로우 / 여성 / 요정',
content = TRIM(BOTH '\r\n' FROM "
1살: 별빛이 쏟아지는 한여름 밤, 고요한 호수 위의 수련잎 위에서 태어났다. 그녀의 첫 울음은 별을 깨어나게 했고, 요정 숲은 하루 밤 동안 빛으로 물들었다.

4살: 숲의 별꽃 축제에서 가장 먼저 반짝이는 빛의 날개를 펼치며, 요정 원로들에게 '스타글로우(별의 빛)'라는 이름을 하사받았다. 그녀가 지나가는 자리마다 작은 별빛이 흩날렸다.

8살: 빛의 수정 언덕에서 요정 마법을 배우기 시작했다. 태양보다 별을 더 좋아한 그녀는 낮보다 밤에 빛을 내는 기술에 집중하며, 어둠 속에서 반짝이는 마법을 완성했다.

13살: 인간 왕국의 궁정 축제에 요정 사절단으로 참석해, 별무리를 불러내는 무희로 무대에 올랐다. 그 밤의 장관은 시와 노래가 되어 대륙에 퍼졌고, 인간 왕은 그녀에게 감사의 보석을 전했다.

20살: 요정 숲 외곽의 꿈나무 결계를 유지하기 위해 ‘별의 고리’를 만들어 밤마다 하늘의 흐름을 감지했다. 그녀의 고리는 수많은 생명체들의 꿈을 안정시켰고, 세레나는 ‘꿈지기’로 불리게 된다.

30살: 요정과 인간 사이의 오해를 푸는 외교 사절로서 각 도시를 순회하며 조화와 공존의 메시지를 전했다. 그녀의 목소리는 언어를 초월해 모든 종족의 마음을 어루만졌다.

45살: 잊힌 요정 마법인 ‘심연의 별빛’을 복원하여 숲 깊은 곳에 잠든 정령을 깨웠다. 이 마법은 생명 없는 곳에 빛을 심는 기적이라 불리며, 대지의 균형을 회복하는 데 큰 역할을 했다.

60살: ‘별의 정원’이라는 명상의 장소를 만들어 방황하는 이들을 위로했다. 이곳은 후에 종족을 불문하고 누구든 찾을 수 있는 평화의 공간이 되었으며, 세레나는 그곳을 지켰다.

80살: 후계 요정들을 위해 『별의 노래와 빛의 춤』이라는 마법 서적을 남겼다. 이 책은 단순한 마법 지식이 아닌, 감성과 평화의 기록으로 평가받았다.

100살: 마지막 별꽃 축제에서 하늘 위로 떠올라 은하수 위를 천천히 걸으며 마지막 춤을 선보였다. 그녀의 몸은 빛으로 흩어졌고, 그 밤 요정 숲의 별들은 천 개의 노래처럼 울렸다. 세레나는 영원한 밤의 수호성으로 기억되었다.
");

INSERT INTO post
SET title = '로한 블러드후프 / 남성 / 오크',
content = TRIM(BOTH '\r\n' FROM "
1살: 붉은 달이 떠오른 날, 황야의 들소 부족 천막에서 태어났다. 태어날 때부터 크고 단단한 뿔 같은 머리뼈가 돋아 있었고, 주술사는 그를 '피의 뿔'이라는 이름으로 예언했다.

5살: 전사들의 훈련장에 몰래 들어가 맨손으로 거대한 방패를 들어 올리며 장로들의 이목을 끌었다. 야성 속에 절제된 분노를 지닌 아이로 자라났다.

10살: 들소 부족의 성인식에서 황야의 맹수를 상대로 맨손으로 싸워 승리하며 이름을 떨쳤다. 이때 '블러드후프(피의 발굽)'라는 칭호를 정식으로 부여받는다.

16살: 이웃 부족과의 유혈 전쟁에서 최전선에 나서 피투성이가 된 채로도 적의 대장과 끝까지 싸웠고, 이를 통해 부족의 젊은 전사들을 이끄는 전술가로 성장했다.

25살: 피의 전쟁 후, 평화 회담을 위해 종족 간 의회에 참가하여 오크의 언어가 아닌 공통어로 연설을 해 모두를 놀라게 했다. 야성 속 이성을 가진 이로 명성을 얻는다.

35살: 사막의 유적에서 잠든 고대 오크의 유물을 찾아내 ‘조상의 뿔 투구’를 복원한다. 이 투구는 부족의 상징이 되었고, 그는 정신적 지도자로 떠오른다.

45살: 강철의 종족 연합을 제안하며 드워프, 인간, 요정, 오크의 협력 전선을 구축했다. 전쟁의 시대를 넘어 평화의 리더로 변모한 그는 '핏빛 외교관'이라 불렸다.

60살: 무너진 부족 전통을 되살리기 위해 ‘전사의 길’이라는 의식과 교육법을 만들고, 젊은 전사들에게 명예와 절제를 가르쳤다. 많은 오크들이 그를 스승이라 부르기 시작했다.

75살: ‘붉은 달의 전투’에서 노장임에도 불구하고 직접 전장에 나서 절체절명의 순간 종족 연합군을 지휘해 승리로 이끈다. 이 전투 이후 그는 살아있는 전설이 된다.

100살: 자신이 처음 태어난 황야 초원으로 돌아가, 붉은 달이 뜨는 밤 조용히 숨을 거둔다. 그가 남긴 방패와 투구는 대대로 오크의 전설로 전해지고, 그의 이름은 부족의 전승 속에서 매년 노래가 되어 울려 퍼진다.
");
```

## 데이터 SELECT V1
```sql
-- 사용자 정의 변수 설정
SET @chunk_size = 200;       -- 각 청크의 길이
SET @overlap = 40;           -- 청크 간 중첩 크기
SET @slide_size = @chunk_size - @overlap; -- 슬라이딩 간격
SET @min_valid_size = @overlap; -- 마지막 청크로 인정할 최소 길이

SELECT *
FROM (
       -- 재귀 CTE 시작
       WITH RECURSIVE

         -- 먼저 chunk 개수 계산
         chunk_info AS (
           SELECT P.id,
                  GREATEST(1, CEIL((CHAR_LENGTH(P.content) - @min_valid_size) / @slide_size)) AS chunkCnt
           FROM post AS P
         ),

         -- 각 id에 대해 1부터 chunkCnt까지 인덱스를 생성
         chunk_expanded AS (
           SELECT id,
                  chunkCnt,
                  0 AS idx
           FROM chunk_info

           UNION ALL

           SELECT
             ce.id,
             ce.chunkCnt,
             ce.idx + 1
           FROM chunk_expanded ce
           WHERE ce.idx + 1 < ce.chunkCnt
         )

       -- 최종 결과 출력
       SELECT id, idx
       FROM chunk_expanded
     ) AS PCK
ORDER BY id, idx
```

## 데이터 SELECT V2
```sql
-- 사용자 정의 변수 설정
SET @chunk_size = 200;       -- 각 청크의 길이
SET @overlap = 40;           -- 청크 간 중첩 크기
SET @slide_size = @chunk_size - @overlap; -- 슬라이딩 간격
SET @min_valid_size = @overlap; -- 마지막 청크로 인정할 최소 길이

SELECT PCK.id,
       PCK.idx,
       SUBSTRING(P.content, PCK.idx * @slide_size + 1, @chunk_size) AS chunk
FROM (
       -- 재귀 CTE 시작
       WITH RECURSIVE

         -- 먼저 chunk 개수 계산
         chunk_info AS (
           SELECT P.id,
                  GREATEST(1, CEIL((CHAR_LENGTH(P.content) - @min_valid_size) / @slide_size)) AS chunkCnt
           FROM post AS P
         ),

         -- 각 id에 대해 1부터 chunkCnt까지 인덱스를 생성
         chunk_expanded AS (
           SELECT id,
                  chunkCnt,
                  0 AS idx
           FROM chunk_info

           UNION ALL

           SELECT
             ce.id,
             ce.chunkCnt,
             ce.idx + 1
           FROM chunk_expanded ce
           WHERE ce.idx + 1 < ce.chunkCnt
         )

       -- 최종 결과 출력
       SELECT id, idx
       FROM chunk_expanded
     ) AS PCK
       INNER JOIN post AS P
                  ON PCK.id = P.id
ORDER BY PCK.id, PCK.idx;
```

## 청크생성
```sql
DROP TABLE IF EXISTS post_content_chunk;

CREATE TABLE post_content_chunk (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    postId INT UNSIGNED NOT NULL,
    chunkIdx INT NOT NULL,
    content LONGTEXT NOT NULL,
    embedding VECTOR(384) NOT NULL,
    createDate DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    modifyDate DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
               ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (postId) REFERENCES post(id),
    INDEX idx_postId (postId),
    VECTOR INDEX idx_embedding (embedding) M=32 DISTANCE=COSINE
) ENGINE=INNODB;

-- 사용자 정의 변수 설정
SET @chunk_size = 200;       -- 각 청크의 길이
SET @overlap = 40;           -- 청크 간 중첩 크기
SET @slide_size = @chunk_size - @overlap; -- 슬라이딩 간격
SET @min_valid_size = @overlap; -- 마지막 청크로 인정할 최소 길이

INSERT INTO post_content_chunk
(
    postId,
    chunkIdx,
    content,
    embedding
)
SELECT PCK.id,
PCK.idx,
chunk,
VEC_FromText(http_post("http://host.docker.internal:8080/api/v1/base/ai/vectorEmbed/embed", "text", chunk))
FROM (
    SELECT PCK.id,
    PCK.idx,
    SUBSTRING(P.content, PCK.idx * @slide_size + 1, @chunk_size) AS chunk
    FROM (
        -- 재귀 CTE 시작
        WITH RECURSIVE

        -- 먼저 chunk 개수 계산
        chunk_info AS (
            SELECT P.id,
            GREATEST(1, CEIL((CHAR_LENGTH(P.content) - @min_valid_size) / @slide_size)) AS chunkCnt
            FROM post AS P
        ),

        -- 각 id에 대해 1부터 chunkCnt까지 인덱스를 생성
        chunk_expanded AS (
            SELECT id,
            chunkCnt,
            0 AS idx
            FROM chunk_info
            
            UNION ALL

            SELECT
            ce.id,
            ce.chunkCnt,
            ce.idx + 1
            FROM chunk_expanded ce
            WHERE ce.idx + 1 < ce.chunkCnt
        )

        -- 최종 결과 출력
        SELECT id, idx
        FROM chunk_expanded
    ) AS PCK
    INNER JOIN post AS P
    ON PCK.id = P.id
) AS PCK
ORDER BY PCK.id, PCK.idx;
```

## 벡터검색 예시
```sql
SET @keyword = "별빛이 쏟아지는 한여름 밤";

SELECT id,
postId,
content,
VEC_DISTANCE_EUCLIDEAN(
    embedding,
    VEC_FromText(
        http_post(
            "http://host.docker.internal:8080/api/v1/base/ai/vectorEmbed/embed",
            "text",
            @keyword
        )
    )
) AS score
FROM post_content_chunk
WHERE 1
ORDER BY VEC_DISTANCE_EUCLIDEAN(
    embedding,
    VEC_FromText(
        http_post(
            "http://host.docker.internal:8080/api/v1/base/ai/vectorEmbed/embed",
            "text",
            @keyword
        )
    )
);
```