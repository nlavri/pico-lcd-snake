#include "calculator_engine.h"

#include <stdbool.h>
#include <stddef.h>

#define CALCULATOR_MAX_PARSE_DEPTH 16

typedef struct CalculatorParser
{
    const char *current;
    CalculatorResult status;
    unsigned int depth;
} CalculatorParser;

static void skip_spaces(CalculatorParser *parser)
{
    while (*parser->current == ' ' || *parser->current == '\t')
    {
        parser->current++;
    }
}

static double parse_expression(CalculatorParser *parser);

static double parse_number(CalculatorParser *parser)
{
    double value = 0.0;
    bool has_digit = false;

    while (*parser->current >= '0' && *parser->current <= '9')
    {
        has_digit = true;
        value = value * 10.0 + (double)(*parser->current - '0');
        parser->current++;
    }

    if (*parser->current == '.')
    {
        parser->current++;
        double place = 0.1;
        while (*parser->current >= '0' && *parser->current <= '9')
        {
            has_digit = true;
            value += (double)(*parser->current - '0') * place;
            place *= 0.1;
            parser->current++;
        }
    }

    if (!has_digit)
    {
        parser->status = CALCULATOR_RESULT_SYNTAX_ERROR;
    }
    return value;
}

static double parse_primary(CalculatorParser *parser)
{
    skip_spaces(parser);
    if (*parser->current != '(')
    {
        return parse_number(parser);
    }

    if (parser->depth >= CALCULATOR_MAX_PARSE_DEPTH)
    {
        parser->status = CALCULATOR_RESULT_SYNTAX_ERROR;
        return 0.0;
    }

    parser->current++;
    parser->depth++;
    const double value = parse_expression(parser);
    parser->depth--;
    skip_spaces(parser);
    if (*parser->current != ')')
    {
        parser->status = CALCULATOR_RESULT_SYNTAX_ERROR;
        return 0.0;
    }
    parser->current++;
    return value;
}

static double parse_unary(CalculatorParser *parser)
{
    skip_spaces(parser);
    if (*parser->current == '+')
    {
        parser->current++;
        return parse_unary(parser);
    }
    if (*parser->current == '-')
    {
        parser->current++;
        return -parse_unary(parser);
    }
    return parse_primary(parser);
}

static double parse_term(CalculatorParser *parser)
{
    double value = parse_unary(parser);
    while (parser->status == CALCULATOR_RESULT_OK)
    {
        skip_spaces(parser);
        const char operation = *parser->current;
        if (operation != '*' && operation != '/')
        {
            break;
        }

        parser->current++;
        const double operand = parse_unary(parser);
        if (parser->status != CALCULATOR_RESULT_OK)
        {
            break;
        }
        if (operation == '*')
        {
            value *= operand;
        }
        else if (operand == 0.0)
        {
            parser->status = CALCULATOR_RESULT_DIVIDE_BY_ZERO;
        }
        else
        {
            value /= operand;
        }
    }
    return value;
}

static double parse_expression(CalculatorParser *parser)
{
    double value = parse_term(parser);
    while (parser->status == CALCULATOR_RESULT_OK)
    {
        skip_spaces(parser);
        const char operation = *parser->current;
        if (operation != '+' && operation != '-')
        {
            break;
        }

        parser->current++;
        const double operand = parse_term(parser);
        value = operation == '+' ? value + operand : value - operand;
    }
    return value;
}

CalculatorResult calculator_evaluate(
    const char *expression,
    double *result)
{
    if (expression == NULL || result == NULL || *expression == '\0')
    {
        return CALCULATOR_RESULT_SYNTAX_ERROR;
    }

    CalculatorParser parser = {
        .current = expression,
        .status = CALCULATOR_RESULT_OK,
        .depth = 0,
    };
    const double value = parse_expression(&parser);
    skip_spaces(&parser);
    if (parser.status == CALCULATOR_RESULT_OK && *parser.current != '\0')
    {
        parser.status = CALCULATOR_RESULT_SYNTAX_ERROR;
    }
    if (parser.status == CALCULATOR_RESULT_OK)
    {
        *result = value;
    }
    return parser.status;
}
