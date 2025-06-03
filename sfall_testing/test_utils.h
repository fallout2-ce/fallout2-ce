#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "define_lite.h"

variable test_suite_errors := 0;
variable test_suite_verbose := false;
variable test_suite_assertions := 0;

procedure assertEquals(variable desc, variable a, variable b) begin
   test_suite_assertions++;

   if (a != b or typeof(a) != typeof(b)) then begin
      display_msg("Assertion failed \""+desc+"\": "+a+" != "+b);
      test_suite_errors ++;
   end else if (test_suite_verbose) then begin
      display_msg("Assert \""+desc+"\" ok");
   end
end

procedure assertNotEquals(variable desc, variable a, variable b) begin
   test_suite_assertions++;

   if (a == b) then begin
      display_msg("Assertion failed \""+desc+"\": "+a+" == "+b);
      test_suite_errors ++;
   end else if (test_suite_verbose) then begin
      display_msg("Assert \""+desc+"\" ok");
   end
end

procedure assertFloat(variable desc, variable a, variable b, variable tolerance = 0.001) begin
   test_suite_assertions++;

   variable diff := abs(a - b);
   if (diff > tolerance) then begin
      display_msg("Assertion failed \""+desc+"\": "+a+" != "+b+" (diff: "+diff+")");
      test_suite_errors ++;
   end else if (test_suite_verbose) then begin
      display_msg("Assert \""+desc+"\" ok");
   end
end

procedure report_test_results(variable desc) begin
   
   display_msg("DONE " + desc + " " + 
     (test_suite_assertions-test_suite_errors) + "/" + test_suite_assertions + "");
   display_msg("================");

   if (test_suite_errors == 0) then begin
      float_msg(dude_obj, desc + " tests passed " + 
         (test_suite_assertions-test_suite_errors) + "/" +test_suite_assertions + "!", FLOAT_MSG_GREEN);        
   end else begin
      float_msg(dude_obj, desc + " failed " + test_suite_errors + " tests from " +
         test_suite_assertions + "!", FLOAT_MSG_RED);        
   end
end

#endif
