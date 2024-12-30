#include "hawkbit_config.h"
#include "http_server.h"
#include "mqtt_config.h"
#include "unity.h"

void setUp( void )
{
  // Initialize the APIs
  API_HAWKBIT_Init();
  API_MQTT_Init();
  APIDeviceConfig_Init();
}

void tearDown( void )
{
  // Clean up after each test
}

void test_hawkbit_get_address( void )
{
  struct mg_str uri = mg_str( "/api/hawkbit/address" );
  HTTPServerResponse_t response = _parse_hawkbit_cb( &uri, NULL, HTTP_SERVER_METHOD_GET );
  TEST_ASSERT_EQUAL( 200, response.code );
  TEST_ASSERT_NOT_NULL( response.msg );
}

void test_hawkbit_set_address( void )
{
  struct mg_str uri = mg_str( "/api/hawkbit/address" );
  struct mg_str data = mg_str( "http://example.com" );
  HTTPServerResponse_t response = _parse_hawkbit_cb( &uri, &data, HTTP_SERVER_METHOD_POST );
  TEST_ASSERT_EQUAL( 200, response.code );
  TEST_ASSERT_EQUAL_STRING( "OK", response.msg );
}

void test_mqtt_get_address( void )
{
  struct mg_str uri = mg_str( "/api/mqtt/address" );
  HTTPServerResponse_t response = _parse_mqtt_cb( &uri, NULL, HTTP_SERVER_METHOD_GET );
  TEST_ASSERT_EQUAL( 200, response.code );
  TEST_ASSERT_NOT_NULL( response.msg );
}

void test_mqtt_set_address( void )
{
  struct mg_str uri = mg_str( "/api/mqtt/address" );
  struct mg_str data = mg_str( "mqtt://example.com" );
  HTTPServerResponse_t response = _parse_mqtt_cb( &uri, &data, HTTP_SERVER_METHOD_POST );
  TEST_ASSERT_EQUAL( 200, response.code );
  TEST_ASSERT_EQUAL_STRING( "OK", response.msg );
}

void test_device_config_get( void )
{
  struct mg_str uri = mg_str( "/api/deviceConfig" );
  HTTPServerResponse_t response = _config_parse_cb( &uri, NULL, HTTP_SERVER_METHOD_GET );
  TEST_ASSERT_EQUAL( 200, response.code );
  TEST_ASSERT_NOT_NULL( response.msg );
}

int main( void )
{
  UNITY_BEGIN();

  return UNITY_END();
}

TEST_GROUP_RUNNER( httpApi )
{
  RUN_TEST( test_hawkbit_get_address );
  RUN_TEST( test_hawkbit_set_address );
  RUN_TEST( test_mqtt_get_address );
  RUN_TEST( test_mqtt_set_address );
  RUN_TEST( test_device_config_get );
}
