#include <cassert>
#include <iostream>
#include <opencv2/opencv.hpp>

#include "core/blackboard.hpp"
#include "core/condition_evaluator.hpp"
#include "core/factory.hpp"

using namespace ConditionEvaluator;

int main() {
  std::cout << "Test for ConditionEvaluator\n";

  TypeRegistry::get_converters();
  Blackboard db;

  // Test 1: Primitives
  db.write("NodeA.int_val", std::any(7));
  assert(evaluate_expression(db, "$NodeA.int_val == 7") == EvalResult::TRUE);
  assert(evaluate_expression(db, "$NodeA.int_val > 5") == EvalResult::TRUE);
  assert(evaluate_expression(db, "$NodeA.int_val < 10") == EvalResult::TRUE);

  // Test 2: Strings
  db.write("NodeB.str_val", std::any(std::string("OK")));
  assert(evaluate_expression(db, "$NodeB.str_val == \"OK\"") ==
         EvalResult::TRUE);
  assert(evaluate_expression(db, "$NodeB.str_val != \"FAIL\"") ==
         EvalResult::TRUE);
  assert(evaluate_expression(db, "$NodeB.str_val.empty() == false") ==
         EvalResult::TRUE);

  db.write("NodeB.empty_str_val", std::any(std::string("")));
  assert(evaluate_expression(db, "$NodeB.empty_str_val.empty() == true") ==
         EvalResult::TRUE);

  // Test 3: Boolean & Unary
  db.write("NodeC.bool_val", std::any(false));
  assert(evaluate_expression(db, "!$NodeC.bool_val") == EvalResult::TRUE);
  assert(evaluate_expression(db, "$NodeC.bool_val == false") ==
         EvalResult::TRUE);

  // Test 4: cv::Mat property extraction
  cv::Mat empty_mat;
  db.write("NodeD.mat_val", std::any(empty_mat));
  assert(evaluate_expression(db, "$NodeD.mat_val.empty() == true") ==
         EvalResult::TRUE);
  assert(evaluate_expression(db, "$NodeD.mat_val.empty()") == EvalResult::TRUE);

  cv::Mat non_empty_mat = cv::Mat::zeros(100, 100, CV_8UC3);
  db.write("NodeE.mat_val", std::any(non_empty_mat));
  assert(evaluate_expression(db, "$NodeE.mat_val.empty() == false") ==
         EvalResult::TRUE);
  assert(evaluate_expression(db, "!$NodeE.mat_val.empty()") ==
         EvalResult::TRUE);
  assert(evaluate_expression(db, "!$NodeE.mat_val.empty") == EvalResult::TRUE);
  assert(evaluate_expression(db, "$NodeE.mat_val.cols == 100") ==
         EvalResult::TRUE);

  // Test 5: Logical Operators
  assert(evaluate_expression(
             db, "$NodeE.mat_val.cols == 100 && $NodeA.int_val == 7") ==
         EvalResult::TRUE);
  assert(evaluate_expression(
             db, "$NodeE.mat_val.cols == 50 || $NodeA.int_val == 7") ==
         EvalResult::TRUE);

  assert(
      evaluate_expression(db, "$NodeE.mat_val.cols == $NodeE.mat_val.rows") ==
      EvalResult::TRUE);

  // Test 6: Literals
  assert(evaluate_expression(db, "true") == EvalResult::TRUE);
  assert(evaluate_expression(db, "8 > 7") == EvalResult::TRUE);
  assert(evaluate_expression(db, "8 > 7 || false") == EvalResult::TRUE);
  assert(evaluate_expression(db, "\"ABC\" > \"AB\"") == EvalResult::TRUE);
  assert(evaluate_expression(db, "1") == EvalResult::TRUE);

  // Error
  assert(evaluate_expression(db, "") == EvalResult::ERROR);
  assert(evaluate_expression(db, "$NodeE.mat_val == $NodeE.mat_val.cols") ==
         EvalResult::ERROR);  // different types
  assert(evaluate_expression(db, "\"true\"") ==
         EvalResult::ERROR);  // string -> bool
  assert(evaluate_expression(db, "$NodeE.mat_val") ==
         EvalResult::ERROR);  // mat -> bool
  assert(evaluate_expression(db, "!NodeC.bool_val") ==
         EvalResult::ERROR);  // Missing '$'

  // Error: currently cannot parse complex expressions
  assert(evaluate_expression(db, "$NodeE.mat_val.cols > 150 * 1") ==
         EvalResult::ERROR);
  assert(evaluate_expression(db, "$NodeE.mat_val.cols * 2 > 150") ==
         EvalResult::ERROR);

  return 0;
}
