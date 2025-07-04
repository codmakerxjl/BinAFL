#pragma once

/**
 * @brief Runs the entire client-side logic: connects to IPC and receives data.
 * This function is designed to be called from the communication thread in the DLL.
 */
void run_client_test();
void run_file_cache_benchmark();
void run_random_access_benchmark();
void run_fuzzing_speed_test();