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
    int task;
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
    double baseline_score;
    double baseline_loss;
    double improvement_over_baseline;
    double balanced_accuracy;
    double mean_absolute_error;
    double r_squared;
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

typedef enum NEDataPattern {
    NE_PATTERN_LINEAR = 0,
    NE_PATTERN_POLYNOMIAL = 1,
    NE_PATTERN_SINE = 2,
    NE_PATTERN_XOR = 3,
    NE_PATTERN_CIRCLES = 4,
    NE_PATTERN_CLUSTERS = 5,
    NE_PATTERN_SPIRAL = 6
} NEDataPattern;

typedef struct NEGenerateConfig {
    const char* output_path;
    NEDataPattern pattern;
    size_t rows;
    size_t input_count;
    double minimum;
    double maximum;
    double noise;
    uint64_t seed;
    int include_header;
} NEGenerateConfig;

typedef struct NEGenerateResult {
    size_t rows_written;
    size_t input_count;
    size_t output_count;
    int is_classification;
    char formula[256];
} NEGenerateResult;

typedef struct NEProfileConfig {
    const char* input_path;
    int has_header;
} NEProfileConfig;

typedef struct NEProfileResult {
    size_t rows;
    size_t columns;
    size_t complete_rows;
    size_t missing_cells;
    size_t malformed_rows;
    size_t duplicate_rows;
} NEProfileResult;

typedef struct NECleanConfig {
    const char* input_path;
    const char* output_path;
    int has_header;
    size_t target_columns;
    int impute_missing_features;
    int remove_duplicates;
    int drop_malformed_rows;
    double clip_z_score;
} NECleanConfig;

typedef struct NECleanResult {
    size_t rows_read;
    size_t rows_written;
    size_t rows_dropped;
    size_t missing_values_imputed;
    size_t duplicates_removed;
    size_t values_clipped;
} NECleanResult;

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

int ne_generate_dataset(const NEGenerateConfig* config,
                        NEGenerateResult* result,
                        char* error,
                        size_t error_capacity);

int ne_profile_dataset(const NEProfileConfig* config,
                       NEProfileResult* result,
                       char* error,
                       size_t error_capacity);

int ne_clean_dataset(const NECleanConfig* config,
                     NECleanResult* result,
                     char* error,
                     size_t error_capacity);

#ifdef __cplusplus
}
#endif

#endif
