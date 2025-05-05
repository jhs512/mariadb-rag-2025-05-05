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

# 커밋 2 : Spring AI 의존성 추가
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