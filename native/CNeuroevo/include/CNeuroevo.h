#ifndef CNEUROEVO_H
#define CNEUROEVO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NESession NESession;

typedef enum NETask {
    NE_TASK_CLASSIFICATION = 0,
    NE_TASK_REGRESSION = 1
} NETask;

typedef struct NEConfig {
    const char* data_path;
    const char* output_path;
    NETask task;
    size_t target_columns;
    int has_header;
    size_t population_size;
    size_t generations;
    size_t elite_count;
    size_t tournament_size;
    size_t threads;
    uint64_t seed;
    double weight_mutation_rate;
    double weight_mutation_sigma;
    double topology_mutation_rate;
    double activation_mutation_rate;
    double immigrant_rate;
    double complexity_penalty;
    double target_score;
    size_t patience;
    size_t initial_hidden;
    size_t max_hidden_layers;
    size_t max_layer_width;
} NEConfig;

typedef struct NEDatasetInfo {
    size_t input_count;
    size_t output_count;
    size_t class_count;
    size_t train_rows;
    size_t validation_rows;
    size_t test_rows;
} NEDatasetInfo;

typedef struct NEProgress {
    size_t generation;
    size_t generation_limit;
    double best_train_score;
    double best_validation_score;
    double mean_fitness;
    size_t parameter_count;
    size_t evaluations;
    double elapsed_seconds;
    const char* topology;
} NEProgress;

typedef void (*NEProgressCallback)(const NEProgress* progress, void* context);

typedef struct NETrainResult {
    int cancelled;
    double validation_score;
    double test_score;
    double test_loss;
    size_t parameter_count;
    size_t evaluations;
    double elapsed_seconds;
    char topology[256];
} NETrainResult;

typedef struct NEPrediction {
    size_t count;
    double values[256];
    double class_value;
    int is_classification;
} NEPrediction;

void ne_default_config(NEConfig* config);
const char* ne_backend_name(void);

NESession* ne_session_create(void);
void ne_session_cancel(NESession* session);
void ne_session_destroy(NESession* session);

int ne_inspect_dataset(const NEConfig* config,
                       NEDatasetInfo* result,
                       char* error,
                       size_t error_capacity);

int ne_train(NESession* session,
             const NEConfig* config,
             NEProgressCallback callback,
             void* callback_context,
             NETrainResult* result,
             char* error,
             size_t error_capacity);

int ne_predict(const char* model_path,
               const float* features,
               size_t feature_count,
               NEPrediction* result,
               char* error,
               size_t error_capacity);

double ne_prediction_value(const NEPrediction* prediction, size_t index);

#ifdef __cplusplus
}
#endif

#endif
