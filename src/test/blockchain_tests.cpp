#include <boost/test/unit_test.hpp>

#include "stdlib.h"

#include "rpc/blockchain.cpp"
#include "test/test_bitcoin.h"

bool DoubleEquals(double a, double b, double epsilon) {
  return std::abs(a - b) < epsilon;
}

CBlockIndex *CreateBlockIndexWithNbits(uint32_t nbits) {
  CBlockIndex *block_index = new CBlockIndex();
  block_index->nHeight = 46367;
  block_index->nTime = 1269211443;
  block_index->nBits = nbits;
  return block_index;
}

CChain CreateChainWithNbits(uint32_t nbits) {
  CBlockIndex *block_index = CreateBlockIndexWithNbits(nbits);
  CChain chain;
  chain.SetTip(block_index);
  return chain;
}

void RejectDifficultyMismatch(double difficulty, double expected_difficulty) {
  BOOST_CHECK_MESSAGE(DoubleEquals(difficulty, expected_difficulty, 0.00001),
                      "Difficulty was " + std::to_string(difficulty) +
                          " but was expected to be " +
                          std::to_string(expected_difficulty));
}

void TestDifficulty(uint32_t nbits, double expected_difficulty) {
  CBlockIndex *block_index = CreateBlockIndexWithNbits(nbits);

  CChain chain;

  double difficulty = GetDifficulty(chain, block_index);
  delete block_index;

  RejectDifficultyMismatch(difficulty, expected_difficulty);
}

BOOST_FIXTURE_TEST_SUITE(blockchain_difficulty_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(get_difficulty_for_very_low_target) {
  TestDifficulty(0x1f111111, 0.000001);
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_low_target) {
  TestDifficulty(0x1ef88f6f, 0.000016);
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_mid_target) {
  TestDifficulty(0x1df88f6f, 0.004023);
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_high_target) {
  TestDifficulty(0x1cf88f6f, 1.029916);
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_very_high_target) {
  TestDifficulty(0x12345678, 5913134931067755359633408.0);
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_null_tip) {
  CChain chain;
  double difficulty = GetDifficulty(chain, nullptr);
  RejectDifficultyMismatch(difficulty, 1.0);
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_null_block_index) {
  CChain chain = CreateChainWithNbits(0x1df88f6f);

  double difficulty = GetDifficulty(chain, nullptr);
  delete chain.Tip();

  double expected_difficulty = 0.004023;

  RejectDifficultyMismatch(difficulty, expected_difficulty);
}

BOOST_AUTO_TEST_CASE(get_difficulty_for_block_index_overrides_tip) {
  CChain chain = CreateChainWithNbits(0x1df88f6f);

  CBlockIndex *override_block_index = CreateBlockIndexWithNbits(0x12345678);

  double difficulty = GetDifficulty(chain, override_block_index);
  delete chain.Tip();
  delete override_block_index;

  RejectDifficultyMismatch(difficulty, 5913134931067755359633408.0);
}

BOOST_AUTO_TEST_SUITE_END()
