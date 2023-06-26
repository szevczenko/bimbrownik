#include <pthread.h>
#include "unity.h"
#include "unity_fixture.h"
#include "app_config.h"

static void RunAllTests( void )
{
  RUN_TEST_GROUP(JsonParser);
}

int main( int argc, const char* argv[] )
{
  configInit();
  return UnityMain(argc, argv, RunAllTests);
}