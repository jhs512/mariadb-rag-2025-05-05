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
    @Cacheable(value = ["embedding"], key = "#text")
    fun embed(text: String): FloatArray {
        return embeddingModel.embed(text)
    }


    data class BaseEmbedReqBody(
        val text: String
    )

    @PostMapping("/embed")
    @Cacheable(value = ["embedding"], key = "#reqBody.text")
    fun embed(@RequestBody reqBody: BaseEmbedReqBody): FloatArray {
        return embeddingModel.embed(reqBody.text)
    }
}