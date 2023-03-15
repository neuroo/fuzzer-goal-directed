#include "sequence.h"
#include "common/logger.h"
#include "utils.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

#include "seqan/align.h"
#include "seqan/graph_msa.h"

namespace fuzz {

namespace seq {

typedef seqan::String<uint16_t> sequence_t; // sequence type
typedef seqan::StringSet<sequence_t> string_set_t;
typedef seqan::StringSet<sequence_t, seqan::Dependent<>> dep_string_set_t;

typedef seqan::Align<sequence_t, seqan::ArrayGaps> align_t; // align type
typedef seqan::Row<align_t>::Type row_t; // gapped sequence type

typedef seqan::Graph<seqan::Alignment<dep_string_set_t>> align_graph_t;
typedef seqan::Id<align_graph_t>::Type graph_id_t;
typedef seqan::Iterator<align_graph_t, seqan::VertexIterator>::Type
    vertex_const_iterator;
typedef seqan::Iterator<align_graph_t, seqan::EdgeIterator>::Type
    edge_const_iterator;

#define DEBUG_PRINT_FRAGMENTS 0

// seqan doesn't play well when the first byte is 0 in the sequence.
// that means we need to shift everything and add 1 to all of them, it doesn't
// matter for the result since we care about indices.
uint16_t *to_offset_buffer(uint8_t *buffer, size_t size) {
  uint16_t *offset_buffer = new (std::nothrow) uint16_t[size];
  for (size_t i = 0; i < size; i++) {
    offset_buffer[i] = ((uint16_t)buffer[i]) + 1;
  }
  return offset_buffer;
}

// Returns the set of index in `source_buffer` that aren't aligned with
// `cmp_buffer`. That means there is no common sub-sequence overlapping
// this index.
set<size_t> get_not_aligned(uint8_t *source_buffer, size_t source_size,
                            uint8_t *cmp_buffer, size_t cmp_size,
                            bool flexible) {
  // Generate a set with all indices from the size
  set<size_t> result;
  size_t counter = 0;
  std::generate_n(std::inserter(result, result.begin()), source_size,
                  [&counter]() { return counter++; });

  uint16_t *offset_source_buffer = to_offset_buffer(source_buffer, source_size),
           *offset_cmp_buffer = to_offset_buffer(cmp_buffer, cmp_size);

  // seq_1 is the reference buffer that we care about.
  sequence_t seq_1, seq_2;
  seqan::assign(seq_1, offset_source_buffer, source_size);
  seqan::assign(seq_2, offset_cmp_buffer, cmp_size);

  if (seqan::length(seq_1) < 1 || seqan::length(seq_2) < 1) {
    LOG(ERROR)
        << "Error creating sequences with SeqAn from the two input buffers";
    LOG(ERROR) << "  source_buffer:\n"
               << utils::hex_dump(source_buffer, source_size);
    LOG(ERROR) << "  cmp_buffer:\n" << utils::hex_dump(cmp_buffer, cmp_size);

    delete[] offset_source_buffer;
    delete[] offset_cmp_buffer;
    return set<size_t>();
  }

#if (DEBUG_PRINT_FRAGMENTS == 1)
  LOG(INFO) << "  source_buffer:\n"
            << utils::hex_dump(source_buffer, source_size);
  LOG(INFO) << "  cmp_buffer:\n" << utils::hex_dump(cmp_buffer, cmp_size);
  LOG(INFO) << "  length_source=" << seqan::length(seq_1);
  LOG(INFO) << "  length_cmp=" << seqan::length(seq_2);

#endif

  if (!flexible) {
    // Actually compare aligned sequences at the same position between
    // the two buffers.
    align_t alignment;
    seqan::resize(seqan::rows(alignment), /*num of sequences*/ 2);
    seqan::assignSource(seqan::row(alignment, 0), seq_1);
    seqan::assignSource(seqan::row(alignment, 1), seq_2);

#if (DEBUG_PRINT_FRAGMENTS == 1)
    ostringstream oss_align;
    oss_align << alignment;

    LOG(INFO) << "  algn:\n" << oss_align.str();
#endif

    // XXX

  } else {
    // In this variant, we allow for a gap between alignments.
    string_set_t sequences;
    seqan::appendValue(sequences, seq_1);
    seqan::appendValue(sequences, seq_2);

    if (seqan::length(sequences) != 2) {
      LOG(ERROR) << "Error wile creating the sequences. Failing.";
      delete[] offset_source_buffer;
      delete[] offset_cmp_buffer;
      return set<size_t>();
    }

    align_graph_t align_graph(sequences);

    int score = seqan::globalAlignment(
        align_graph, seqan::Score<int, seqan::Simple>(3, -1, 1),
        seqan::AlignConfig<true, true, true, true>());

#if (DEBUG_PRINT_FRAGMENTS == 1)
    {
      ostringstream oss;
      seqan::writeRecords(oss, align_graph, seqan::DotDrawing());
      LOG(INFO) << "score: " << score;
      LOG(INFO) << "dot:\n" << oss.str();
    }
#endif

    // No alignment
    if (seqan::empty(align_graph)) {
#if (DEBUG_PRINT_FRAGMENTS == 1)
      LOG(INFO) << "Empty alignment";
#endif
      delete[] offset_source_buffer;
      delete[] offset_cmp_buffer;
      return set<size_t>();
    }

    edge_const_iterator iter(align_graph);
    for (; !seqan::atEnd(iter); ++iter) {
      auto source_descriptor = sourceVertex(align_graph, *iter),
           target_descriptor = targetVertex(align_graph, *iter);

      graph_id_t src_id = seqan::sequenceId(align_graph, source_descriptor),
                 target_id = seqan::sequenceId(align_graph, target_descriptor);

      if (src_id == target_id) {
        continue;
      }

      const size_t frag_src_begin =
                       seqan::fragmentBegin(align_graph, source_descriptor),
                   frag_target_begin =
                       seqan::fragmentBegin(align_graph, target_descriptor);

      const size_t frag_src_end =
                       frag_src_begin +
                       seqan::fragmentLength(align_graph, source_descriptor),
                   frag_target_end =
                       frag_target_begin +
                       seqan::fragmentLength(align_graph, target_descriptor);

#if (DEBUG_PRINT_FRAGMENTS == 1)
      LOG(INFO) << "Fragment: [" << frag_src_begin << "," << frag_src_end
                << ") ; [" << frag_target_begin << "," << frag_target_end << ")"
                << " ; src_id=" << (size_t)src_id
                << " target_id=" << (size_t)target_id;
#endif

      size_t src_bufer_frag_begin =
                 (src_id == 0) ? frag_src_begin : frag_target_begin,
             src_buffer_frag_end =
                 (src_id == 0) ? frag_src_end : frag_target_end;

      for (size_t i = src_bufer_frag_begin; i < src_buffer_frag_end; i++) {
        result.erase(i);
      }
    }
  }
  delete[] offset_source_buffer;
  delete[] offset_cmp_buffer;
  return result;
}

vector<pairwise_score_t>
compute_pairwise_score(const ga::individual_set &individuals, bool flexible) {
  const size_t num_individuals = individuals.size();
  vector<pair<uint16_t *, size_t>> buffers(num_individuals);
  vector<pairwise_score_t> scores;

  sequence_t *ind_seq = new sequence_t[num_individuals];
  string_set_t sequences;

  // First, we convert our individuals into offseted buffers.
  for (size_t idx = 0; idx < num_individuals; idx++) {
    const ga::Individual &ind = individuals[idx];
    uint8_t *buffer = ind.memory.buffer(ind.index);
    size_t length = ind.memory.length(ind.index);

    buffers[idx] = make_pair(to_offset_buffer(buffer, length), length);
    seqan::assign(ind_seq[idx], buffers[idx].first, buffers[idx].second);
    seqan::appendValue(sequences, ind_seq[idx]);
  }

#define SCORE_UPDATE(i, j, z) score_matrix[i + num_individuals * j] += z
#define SCORE_GET(i, j) score_matrix[i + num_individuals * j]

  size_t *score_matrix = nullptr;
  align_graph_t align_graph(sequences);

  LOG(INFO) << "Start MSA on " << num_individuals << " individuals";
  seqan::globalMsaAlignment(align_graph,
                            seqan::Score<int, seqan::Simple>(1, 0, 0));
  LOG(INFO) << "Done with MSA";

#if (DEBUG_PRINT_FRAGMENTS == 1)
  {
    ostringstream oss;
    seqan::writeRecords(oss, align_graph, seqan::DotDrawing());
    LOG(INFO) << "dot:\n" << oss.str();
  }
#endif

  edge_const_iterator iter(align_graph);
  score_matrix = new size_t[num_individuals * num_individuals]();

  for (; !seqan::atEnd(iter); ++iter) {
    auto source_descriptor = sourceVertex(align_graph, *iter),
         target_descriptor = targetVertex(align_graph, *iter);

    graph_id_t src_id = seqan::sequenceId(align_graph, source_descriptor),
               target_id = seqan::sequenceId(align_graph, target_descriptor);

    if (src_id == target_id) {
      continue;
    }

    // We have a sub-sequence from src_id -> target_id, we compute the score
    // by summing the size of the common sub-sequence
    const size_t frag_size =
        seqan::fragmentLength(align_graph, source_descriptor);

    SCORE_UPDATE(src_id, target_id, frag_size);
  }

  // Now, we just sum each row in the matrix to get the scores...
  for (size_t idx_1 = 0; idx_1 < num_individuals; idx_1++) {
    for (size_t idx_2 = 0; idx_2 < num_individuals; idx_2++) {
      if (idx_1 == idx_2)
        continue;
      scores.push_back(pairwise_score_t(idx_1, idx_2, SCORE_GET(idx_1, idx_2)));
    }
  }

#undef SCORE_UPDATE
#undef SCORE_GET

  // cleanup
  delete[] ind_seq;
  if (score_matrix != nullptr)
    delete[] score_matrix;
  for (size_t idx = 0; idx < num_individuals; idx++) {
    delete[] buffers[idx].first;
  }

  return scores;
}

#undef DEBUG_PRINT_FRAGMENTS

// end namespace=seq
}
// end namespace=fuzz
}
