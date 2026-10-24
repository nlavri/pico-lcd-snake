#ifndef PICO_LCD_CALCULATOR_ENGINE_H
#define PICO_LCD_CALCULATOR_ENGINE_H

typedef enum CalculatorResult
{
    CALCULATOR_RESULT_OK,
    CALCULATOR_RESULT_SYNTAX_ERROR,
    CALCULATOR_RESULT_DIVIDE_BY_ZERO,
} CalculatorResult;

CalculatorResult calculator_evaluate(
    const char *expression,
    double *result);

#endif
