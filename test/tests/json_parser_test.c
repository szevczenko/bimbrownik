#include "json_parser.h"
#include "unity.h"
#include "unity_fixture.h"

static bool test_bool;
static int test_int;
static double test_double;
static char test_string[128];
static size_t test_string_len;
static bool test_null;

TEST_GROUP( JsonParser );

TEST_SETUP( JsonParser )
{
  test_bool = false;
  test_int = 0;
  test_double = 0;
  memset( test_string, 0, sizeof( test_string ) );
  test_string_len = 0;
  test_null = false;
}

TEST_TEAR_DOWN( JsonParser )
{
}

static bool _bool_cb( bool value )
{
  test_bool = value;
  return true;
}

static bool _int_cb( int value )
{
  test_int = value;
  return true;
}

static bool _double_cb( double value )
{
  test_double = value;
  return true;
}

static bool _string_cb( const char* str, size_t str_len )
{
  strncpy( test_string, str, str_len );
  return true;
}

static bool _null_cb( void )
{
  test_null = true;
  return true;
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
  const char* test_string = "{\"set\":{\"bool\":true, \"int\":123, \"double\":1.123, \"string\":\"test_value\", \"null\": null}}";
  TEST_ASSERT_EQUAL( true, JSONParser_RegisterMethod( token, sizeof( token ) / sizeof( token[0] ), "set" ) );
  TEST_ASSERT_EQUAL( true, JSONParse( test_string ) );
  TEST_ASSERT_EQUAL( true, test_bool );
  TEST_ASSERT_EQUAL( 123, test_int );
  TEST_ASSERT_EQUAL( 1.123, test_double );
  TEST_ASSERT_EQUAL_STRING_LEN( "test_value", test_string, test_string_len );
  TEST_ASSERT_EQUAL( true, test_null );
}

TEST_GROUP_RUNNER( JsonParser )
{
  RUN_TEST_CASE( JsonParser, JsonParserParseString );
}
