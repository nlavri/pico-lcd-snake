#include <assert.h>

#include "calculator_engine.h"

static void expect_value(const char *expression, double expected)
{
    double result = 0.0;
    assert(calculator_evaluate(expression, &result) == CALCULATOR_RESULT_OK);
    assert(result == expected);
}

int main(void)
{
    expect_value("2+3*4", 14.0);
    expect_value("(2+3)*4", 20.0);
    expect_value("8/2/2", 2.0);
    expect_value("-3+10", 7.0);
    expect_value("2*(-3+5)", 4.0);
    expect_value(".5+1.25", 1.75);
    expect_value("--4", 4.0);

    double result = 0.0;
    assert(calculator_evaluate("1/0", &result) ==
        CALCULATOR_RESULT_DIVIDE_BY_ZERO);
    assert(calculator_evaluate("", &result) ==
        CALCULATOR_RESULT_SYNTAX_ERROR);
    assert(calculator_evaluate("2+", &result) ==
        CALCULATOR_RESULT_SYNTAX_ERROR);
    assert(calculator_evaluate("(2+3", &result) ==
        CALCULATOR_RESULT_SYNTAX_ERROR);
    assert(calculator_evaluate("2 3", &result) ==
        CALCULATOR_RESULT_SYNTAX_ERROR);

    return 0;
}
