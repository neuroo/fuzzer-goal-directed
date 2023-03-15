#include "mocker.h"
#include "common/elements.h"
#include "common/logger.h"

#include "shared-data/shared-data.h"
using namespace shm;

#include <memory>
#include <set>
#include <vector>
using namespace std;

namespace fuzz {

#define MAX_NUMBER_BLOCKS 12
#define MAX_NUMBER_CALLS 50
#define MAX_NUMBER_FUNCTIONS 10
#define MAX_NUMBER_EDGES 100

#define ABSOLUTE_MIN_ALL 10

// Just a small utility to generate traces that could look like real traces.
// Mostly used to test the GA.
Container::mocked_trace_t
Mocker::generate_random_trace(const uint64_t testcase_id) {
  Container::mocked_trace_t trace;

  const uint16_t num_functions =
      random->next_number(MAX_NUMBER_FUNCTIONS) + ABSOLUTE_MIN_ALL;
  const uint16_t num_calls =
      random->next_number(MAX_NUMBER_CALLS) + ABSOLUTE_MIN_ALL;

  for (uint16_t c = 0; c < num_calls; c++) {
    uint16_t cur_func_id = random->next_number(num_functions);
    trace.push_back(TraceElement(E_ENTER_FUNCTION, cur_func_id));

    uint16_t num_edges_per_func = random->next_number(MAX_NUMBER_EDGES);
    uint16_t pred_block = 0, cur_block = 0;

    for (uint16_t b = 0; b < num_edges_per_func; b++) {
      cur_block = random->next_number(MAX_NUMBER_BLOCKS);
      trace.push_back(TraceElement(E_TRUE_BRANCH, /*thread_id*/ 0x4141,
                                   cur_func_id, pred_block, cur_block));
      pred_block = cur_block;
    }
    trace.push_back(TraceElement(E_EXIT_FUNCTION, cur_func_id));
  }

  // LOG(INFO) << "Generated trace of " << trace.size() << " elements";

  return trace;
}

#undef MAX_NUMBER_BLOCKS
#undef MAX_NUMBER_CALLS
#undef MAX_NUMBER_FUNCTIONS
#undef MAX_NUMBER_EDGES
#undef ABSOLUTE_MIN_ALL
}
