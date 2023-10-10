#include "json_parser.h"
#include "unity.h"
#include "unity_fixture.h"

#define OK_RESPONSE "{\"param1\":1}"

static bool init_is_running;
static bool test_bool;
static int test_int;
static double test_double;
static char test_string[128];
static size_t test_string_len;
static bool test_null;
static uint32_t test_iterator_value;

TEST_GROUP( JsonParser );

TEST_SETUP( JsonParser )
{
  JSONParser_Init();
  test_bool = false;
  test_int = 0;
  test_double = 0;
  memset( test_string, 0, sizeof( test_string ) );
  test_string_len = 0;
  test_null = false;
  init_is_running = false;
}

TEST_TEAR_DOWN( JsonParser )
{
}

static void _bool_cb( bool value, uint32_t iterator )
{
  test_bool = value;
  TEST_ASSERT_EQUAL( test_iterator_value, iterator );
}

static void _int_cb( int value, uint32_t iterator )
{
  test_int = value;
  TEST_ASSERT_EQUAL( test_iterator_value, iterator );
}

static void _double_cb( double value, uint32_t iterator )
{
  test_double = value;
  TEST_ASSERT_EQUAL( test_iterator_value, iterator );
}

static void _string_cb( const char* str, size_t str_len, uint32_t iterator )
{
  strncpy( test_string, str, str_len );
  TEST_ASSERT_EQUAL( test_iterator_value, iterator );
}

static void _null_cb( uint32_t iterator )
{
  test_null = true;
  TEST_ASSERT_EQUAL( test_iterator_value, iterator );
}

static void init_cb( void )
{
  init_is_running = true;
}

static error_code_t response_ok_cb( char* response, size_t responseLen )
{
  sprintf( response, OK_RESPONSE );
  return ERROR_CODE_OK;
}

static error_code_t response_fail_cb( void )
{
  return ERROR_CODE_FAIL;
}

TEST( JsonParser, JsonParserParseString )
{
  json_parse_token_t token[] = {
    {.bool_cb = _bool_cb,
     .name = "bool"  },
    { .int_cb = _int_cb,
     .name = "int"   },
    { .double_cb = _double_cb,
     .name = "double"},
    { .string_cb = _string_cb,
     .name = "string"},
    { .null_cb = _null_cb,
     .name = "null"  },
  };
  char response[256] = { 0 };
  uint32_t iterator = 0;
  test_iterator_value = 123;
  const char* test_string = "{\"method\":\"set\",\"data\":{\"bool\":true, \"int\":123, \"double\":1.123, \"string\":\"test_value\", \"null\": null}, \"i\":123}";
  TEST_ASSERT_EQUAL( true, JSONParser_RegisterMethod( token, sizeof( token ) / sizeof( token[0] ), "set", init_cb, response_ok_cb ) );
  TEST_ASSERT_EQUAL( ERROR_CODE_OK, JSONParse( test_string, strlen( test_string ), &iterator, response, sizeof( response ) ) );
  TEST_ASSERT_EQUAL( true, test_bool );
  TEST_ASSERT_EQUAL( 123, test_int );
  TEST_ASSERT_EQUAL( 1.123, test_double );
  TEST_ASSERT_EQUAL_STRING_LEN( OK_RESPONSE, response, strlen(OK_RESPONSE) );
  TEST_ASSERT_EQUAL( true, test_null );
  TEST_ASSERT_EQUAL( true, init_is_running );
}

TEST( JsonParser, JsonParserParseTestResultFail )
{
  json_parse_token_t token[] = {
    {.bool_cb = _bool_cb,
     .name = "bool"}
  };
  char response[256] = { 0 };
  uint32_t iterator = 0;
  test_iterator_value = 10;
  const char* test_string = "{\"method\":\"set\",\"data\":{\"bool\":true}, \"i\":10}";
  TEST_ASSERT_EQUAL( true, JSONParser_RegisterMethod( token, sizeof( token ) / sizeof( token[0] ), "set", init_cb, response_fail_cb ) );
  TEST_ASSERT_EQUAL( ERROR_CODE_FAIL, JSONParse( test_string, strlen( test_string ), &iterator, response, sizeof( response ) ) );
  TEST_ASSERT_EQUAL( true, test_bool );
  TEST_ASSERT_EQUAL( true, init_is_running );
}

TEST_GROUP_RUNNER( JsonParser )
{
  RUN_TEST_CASE( JsonParser, JsonParserParseString );
  RUN_TEST_CASE( JsonParser, JsonParserParseTestResultFail );
}
