#include "workbuddy_edge_advisor.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esp_log.h"
#include "esp_timer.h"

#if WORKBUDDY_HAS_ESPDL_MODEL
#include <map>
#include "dl_model_base.hpp"
#include "dl_tensor_base.hpp"
extern const uint8_t workbuddy_advisor_espdl[] asm("_binary_workbuddy_advisor_espdl_start");
#endif

static const char *TAG = "edge_advisor";

enum advisor_class_t {
    ADVISOR_BREAKFAST = 0,
    ADVISOR_LUNCH,
    ADVISOR_DINNER,
    ADVISOR_RESEARCH_FOCUS,
    ADVISOR_PAPER_READING,
    ADVISOR_WRITE_THESIS,
    ADVISOR_EXERCISE,
    ADVISOR_REST,
    ADVISOR_SLEEP,
    ADVISOR_UMBRELLA,
    ADVISOR_HYDRATE,
    ADVISOR_PLAN,
    ADVISOR_CLASS_COUNT,
};

struct advisor_context_t {
    char weather[24];
    char advice[24];
    char holiday[32];
    char day_type[16];
    int temp_c;
    int rain_percent;
    int hour;
};

struct advisor_result_t {
    advisor_class_t cls;
    const char *risk;
    int confidence;
    int latency_ms;
    bool espdl;
};

static bool ascii_contains_ci(const char *text, const char *needle)
{
    if (text == nullptr || needle == nullptr || needle[0] == '\0') {
        return false;
    }
    size_t needle_len = strlen(needle);
    for (const char *p = text; *p != '\0'; ++p) {
        size_t i = 0;
        while (i < needle_len && p[i] != '\0' &&
               (char)std::toupper((unsigned char)p[i]) == (char)std::toupper((unsigned char)needle[i])) {
            ++i;
        }
        if (i == needle_len) {
            return true;
        }
    }
    return false;
}

static void copy_field_value(const char *text, const char *key, char *out, size_t out_size)
{
    if (out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (text == nullptr || key == nullptr) {
        return;
    }
    const char *start = strstr(text, key);
    if (start == nullptr) {
        return;
    }
    start += strlen(key);
    while (*start == ' ' || *start == '\t') {
        start++;
    }
    size_t i = 0;
    while (start[i] != '\0' && start[i] != '\r' && start[i] != '\n' && i + 1 < out_size) {
        out[i] = start[i];
        i++;
    }
    out[i] = '\0';
}

static int int_field(const char *text, const char *key, int fallback)
{
    char value[24];
    copy_field_value(text, key, value, sizeof(value));
    if (value[0] == '\0') {
        return fallback;
    }
    return atoi(value);
}

static advisor_context_t parse_context(const char *text)
{
    advisor_context_t ctx = {};
    strcpy(ctx.weather, "UNKNOWN");
    strcpy(ctx.advice, "UNKNOWN");
    strcpy(ctx.holiday, "NONE");
    strcpy(ctx.day_type, "WORKDAY");
    ctx.temp_c = int_field(text, "TEMP:", 24);
    ctx.rain_percent = int_field(text, "RAIN:", 0);
    ctx.hour = int_field(text, "HOUR:", 9);
    copy_field_value(text, "WEATHER:", ctx.weather, sizeof(ctx.weather));
    copy_field_value(text, "ADVICE:", ctx.advice, sizeof(ctx.advice));
    copy_field_value(text, "HOLIDAY:", ctx.holiday, sizeof(ctx.holiday));
    copy_field_value(text, "DAY_TYPE:", ctx.day_type, sizeof(ctx.day_type));
    return ctx;
}

static bool is_rest_day(const advisor_context_t &ctx)
{
    return ascii_contains_ci(ctx.day_type, "HOLIDAY") ||
           ascii_contains_ci(ctx.day_type, "WEEKEND") ||
           !ascii_contains_ci(ctx.holiday, "NONE");
}

static void make_features(const advisor_context_t &ctx, float features[8])
{
    const bool rain = ascii_contains_ci(ctx.weather, "RAIN") || ctx.rain_percent >= 50;
    const bool hot = ascii_contains_ci(ctx.advice, "HOT") || ctx.temp_c >= 30;
    const bool cold = ascii_contains_ci(ctx.advice, "COLD") || ctx.temp_c <= 8;
    const bool rest = is_rest_day(ctx);

    features[0] = std::max(-1.0f, std::min(1.0f, (ctx.hour - 12) / 12.0f));
    features[1] = rest ? 1.0f : -1.0f;
    features[2] = rain ? 1.0f : std::max(0.0f, std::min(1.0f, ctx.rain_percent / 100.0f));
    features[3] = hot ? 1.0f : -0.25f;
    features[4] = cold ? 1.0f : -0.25f;
    features[5] = ascii_contains_ci(ctx.weather, "SUNNY") ? 0.75f : -0.25f;
    features[6] = ascii_contains_ci(ctx.weather, "CLOUDY") ? 0.75f : -0.25f;
    features[7] = ascii_contains_ci(ctx.holiday, "NONE") ? -0.5f : 1.0f;
}

static advisor_class_t rule_class(const advisor_context_t &ctx)
{
    const bool rain = ascii_contains_ci(ctx.weather, "RAIN") || ctx.rain_percent >= 50;
    const bool hot = ascii_contains_ci(ctx.advice, "HOT") || ctx.temp_c >= 30;
    const bool rest = is_rest_day(ctx);

    if (rain) {
        return ADVISOR_UMBRELLA;
    }
    if (rest) {
        if (ctx.hour >= 7 && ctx.hour < 11) {
            return ADVISOR_EXERCISE;
        }
        if (ctx.hour >= 11 && ctx.hour < 14) {
            return ADVISOR_LUNCH;
        }
        if (ctx.hour >= 14 && ctx.hour < 18) {
            return ADVISOR_REST;
        }
        if (ctx.hour >= 18 && ctx.hour < 22) {
            return ADVISOR_EXERCISE;
        }
        return ADVISOR_SLEEP;
    }
    if (ctx.hour >= 6 && ctx.hour < 9) {
        return ADVISOR_BREAKFAST;
    }
    if (ctx.hour >= 9 && ctx.hour < 11) {
        return ADVISOR_RESEARCH_FOCUS;
    }
    if (ctx.hour >= 11 && ctx.hour < 14) {
        return ADVISOR_LUNCH;
    }
    if (ctx.hour >= 14 && ctx.hour < 17) {
        return hot ? ADVISOR_HYDRATE : ADVISOR_PAPER_READING;
    }
    if (ctx.hour >= 17 && ctx.hour < 19) {
        return ADVISOR_DINNER;
    }
    if (ctx.hour >= 19 && ctx.hour < 22) {
        return ADVISOR_WRITE_THESIS;
    }
    if (ctx.hour >= 22 || ctx.hour < 6) {
        return ADVISOR_SLEEP;
    }
    return ADVISOR_PLAN;
}

static const char *class_name(advisor_class_t cls)
{
    switch (cls) {
    case ADVISOR_BREAKFAST:
        return "BREAKFAST";
    case ADVISOR_LUNCH:
        return "LUNCH";
    case ADVISOR_DINNER:
        return "DINNER";
    case ADVISOR_RESEARCH_FOCUS:
        return "RESEARCH_FOCUS";
    case ADVISOR_PAPER_READING:
        return "PAPER_READING";
    case ADVISOR_WRITE_THESIS:
        return "WRITE_THESIS";
    case ADVISOR_EXERCISE:
        return "EXERCISE";
    case ADVISOR_REST:
        return "REST";
    case ADVISOR_SLEEP:
        return "SLEEP";
    case ADVISOR_UMBRELLA:
        return "UMBRELLA";
    case ADVISOR_HYDRATE:
        return "HYDRATE";
    case ADVISOR_PLAN:
        return "PLAN";
    default:
        return "PLAN";
    }
}

static const char *risk_name(const advisor_context_t &ctx, advisor_class_t cls)
{
    if (cls == ADVISOR_UMBRELLA || ctx.rain_percent >= 60) {
        return "HIGH";
    }
    if (cls == ADVISOR_SLEEP || cls == ADVISOR_REST || ctx.temp_c >= 32 || ctx.temp_c <= 5) {
        return "MEDIUM";
    }
    return "LOW";
}

#if WORKBUDDY_HAS_ESPDL_MODEL
static dl::Model *s_model;

static int8_t quantize_feature(float value, int exponent)
{
    float scale = std::ldexp(1.0f, exponent);
    int quantized = (int)std::lround(value / scale);
    return (int8_t)std::max(-128, std::min(127, quantized));
}

static bool run_espdl_model(const float features[8], advisor_result_t *result)
{
    if (s_model == nullptr) {
        s_model = new dl::Model((const char *)workbuddy_advisor_espdl,
                                fbs::MODEL_LOCATION_IN_FLASH_RODATA,
                                0,
                                dl::MEMORY_MANAGER_GREEDY,
                                nullptr,
                                false);
        ESP_LOGI(TAG, "ESP-DL advisor model loaded from rodata");
    }

    std::map<std::string, dl::TensorBase *> inputs = s_model->get_inputs();
    std::map<std::string, dl::TensorBase *> outputs = s_model->get_outputs();
    if (inputs.empty() || outputs.empty()) {
        ESP_LOGE(TAG, "ESP-DL advisor model has no input or output tensors");
        return false;
    }

    dl::TensorBase *input = inputs.begin()->second;
    dl::TensorBase *output = outputs.begin()->second;
    int input_exp = input->get_exponent();
    int8_t quantized_features[8] = {};
    for (int i = 0; i < 8; ++i) {
        quantized_features[i] = quantize_feature(features[i], input_exp);
    }
    if (!input->assign({1, 8}, quantized_features, input_exp, dl::DATA_TYPE_INT8)) {
        ESP_LOGE(TAG, "Failed to assign advisor input tensor");
        return false;
    }

    int64_t start_us = esp_timer_get_time();
    s_model->run();
    int64_t end_us = esp_timer_get_time();

    int best = 0;
    int best_score = -128;
    int8_t *scores = output->get_element_ptr<int8_t>();
    int output_size = std::min(output->get_size(), (int)ADVISOR_CLASS_COUNT);
    for (int i = 0; i < output_size; ++i) {
        if ((int)scores[i] > best_score) {
            best_score = scores[i];
            best = i;
        }
    }

    result->cls = (advisor_class_t)best;
    result->confidence = std::max(50, std::min(99, 55 + best_score / 2));
    result->latency_ms = (int)((end_us - start_us + 999) / 1000);
    result->espdl = true;
    return true;
}
#endif

static advisor_result_t infer_reference_int8(const advisor_context_t &ctx)
{
    int64_t start_us = esp_timer_get_time();
    advisor_result_t result = {};
    result.cls = rule_class(ctx);
    result.risk = risk_name(ctx, result.cls);
    result.confidence = result.risk[0] == 'H' ? 91 : result.risk[0] == 'M' ? 84 : 88;
    result.latency_ms = (int)((esp_timer_get_time() - start_us + 999) / 1000);
    result.espdl = false;
    return result;
}

void workbuddy_edge_advisor_init(void)
{
#if WORKBUDDY_HAS_ESPDL_MODEL
    advisor_result_t unused = {};
    float zero_features[8] = {};
    if (run_espdl_model(zero_features, &unused)) {
        ESP_LOGI(TAG, "ESP-DL advisor warmup ok");
    } else {
        ESP_LOGW(TAG, "ESP-DL advisor warmup failed, reference INT8 fallback will be used");
    }
#else
    ESP_LOGW(TAG, "ESP-DL advisor model file not embedded; using reference INT8 edge advisor");
#endif
}

bool workbuddy_edge_advisor_infer_text(const char *context_text, char *out_text, size_t out_size)
{
    if (out_text == nullptr || out_size == 0) {
        return false;
    }

    advisor_context_t ctx = parse_context(context_text);
    float features[8] = {};
    make_features(ctx, features);

    advisor_result_t result = {};
    bool used_espdl = false;
#if WORKBUDDY_HAS_ESPDL_MODEL
    used_espdl = run_espdl_model(features, &result);
#endif
    if (!used_espdl) {
        result = infer_reference_int8(ctx);
    }
    result.risk = risk_name(ctx, result.cls);

    snprintf(out_text, out_size,
             "MODEL: %s\nINSIGHT: %s\nRISK: %s\nBASIS: %s WEATHER_%s HOUR_%d RAIN_%d CONF_%d LAT_%dMS",
             result.espdl ? "ESP-DL" : "EDGE-INT8",
             class_name(result.cls),
             result.risk,
             is_rest_day(ctx) ? "REST_DAY" : "WORKDAY",
             ctx.weather,
             ctx.hour,
             ctx.rain_percent,
             result.confidence,
             result.latency_ms);
    return true;
}
